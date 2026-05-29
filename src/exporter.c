#include "exporter.h"
#include "database.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>

/* Escape CSV field: wrap in quotes, double any internal quotes */
static char *
csv_escape (const char *s)
{
    if (!s) return g_strdup ("\"\"");
    GString *out = g_string_new ("\"");
    for (const char *p = s; *p; p++) {
        if (*p == '"') g_string_append (out, "\"\"");
        else           g_string_append_c (out, *p);
    }
    g_string_append_c (out, '"');
    return g_string_free (out, FALSE);
}

bool
export_highlights_csv (int project_id, const char *path)
{
    FILE *f = fopen (path, "w");
    if (!f) return false;

    fprintf (f, "snippet,document,tags,memo\n");

    sqlite3_stmt *stmt = db_results_get_all (project_id);
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            const char *snippet  = (const char *)sqlite3_column_text (stmt, 0);
            const char *doc_name = (const char *)sqlite3_column_text (stmt, 1);
            const char *tags_raw = (const char *)sqlite3_column_text (stmt, 2);
            const char *memo     = (const char *)sqlite3_column_text (stmt, 4);

            /* tags_raw = "path|||color@@@path|||color..." → extract paths only */
            GString *tags_clean = g_string_new ("");
            if (tags_raw) {
                char **entries = g_strsplit (tags_raw, "@@@", -1);
                for (int i = 0; entries[i]; i++) {
                    char **parts = g_strsplit (entries[i], "|||", 2);
                    if (parts[0]) {
                        if (tags_clean->len > 0) g_string_append (tags_clean, "; ");
                        g_string_append (tags_clean, parts[0]);
                    }
                    g_strfreev (parts);
                }
                g_strfreev (entries);
            }

            /* Strip HTML from snippet */
            char *clean_snip = NULL;
            if (snippet) {
                GRegex *re = g_regex_new ("<[^>]+>", 0, 0, NULL);
                clean_snip = g_regex_replace_literal (re, snippet, -1, 0, "", 0, NULL);
                g_regex_unref (re);
            }

            char *e_snip = csv_escape (clean_snip ? clean_snip : "");
            char *e_doc  = csv_escape (doc_name);
            char *e_tags = csv_escape (tags_clean->str);
            char *e_memo = csv_escape (memo);

            fprintf (f, "%s,%s,%s,%s\n", e_snip, e_doc, e_tags, e_memo);

            g_free (e_snip); g_free (e_doc); g_free (e_tags); g_free (e_memo);
            g_free (clean_snip);
            g_string_free (tags_clean, TRUE);
        }
        sqlite3_finalize (stmt);
    }

    fclose (f);
    return true;
}

bool
export_codebook_csv (int project_id, const char *path)
{
    FILE *f = fopen (path, "w");
    if (!f) return false;

    fprintf (f, "tag,description,color,highlight_count\n");

    sqlite3_stmt *stmt = db_tags_get_stats (project_id);
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            /* stats returns: id, path, color, count */
            const char *tag_path = (const char *)sqlite3_column_text (stmt, 1);
            const char *color    = (const char *)sqlite3_column_text (stmt, 2);
            int count            = sqlite3_column_int (stmt, 3);

            char *e_path  = csv_escape (tag_path);
            char *e_color = csv_escape (color);
            fprintf (f, "%s,\"\",%s,%d\n", e_path, e_color, count);
            /* description column empty — not stored in current schema */
            g_free (e_path); g_free (e_color);
        }
        sqlite3_finalize (stmt);
    }

    fclose (f);
    return true;
}


typedef struct {
    char *theme_name;
    GString *codes;
    int frequency;
    char *description;
} ThematicRow;

static void thematic_row_free (ThematicRow *row) {
    if (!row) return;
    g_free(row->theme_name);
    if (row->codes) g_string_free(row->codes, TRUE);
    g_free(row->description);
    g_free(row);
}

bool
export_thematic_table_html (int project_id, const char *path)
{
    FILE *f = fopen (path, "w");
    if (!f) return false;

    fprintf(f, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>\n");
    fprintf(f, "<table border=\"1\" style=\"border-collapse: collapse; width: 100%%; font-family: sans-serif;\">\n");
    fprintf(f, "<thead><tr><th>Tema Principal</th><th>Códigos Emergentes</th><th>Frecuencia (Únicas)</th><th>Descripción Analítica</th></tr></thead>\n");
    fprintf(f, "<tbody>\n");

    GHashTable *themes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) thematic_row_free);
    GPtrArray *ordered_themes = g_ptr_array_new ();

    // First get the codes and descriptions
    sqlite3_stmt *stmt = db_tags_get_stats (project_id);
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            const char *path_str = (const char *)sqlite3_column_text (stmt, 1);
            const char *desc     = (const char *)sqlite3_column_text (stmt, 4);

            if (!path_str) continue;

            char **parts = g_strsplit (path_str, "/", 2);
            char *parent = parts[0];
            char *child = parts[1];

            if (!parent) {
                g_strfreev(parts);
                continue;
            }

            ThematicRow *row = g_hash_table_lookup(themes, parent);
            if (!row) {
                row = g_new0(ThematicRow, 1);
                row->theme_name = g_strdup(parent);
                row->codes = g_string_new("");
                row->frequency = 0;
                row->description = g_strdup("");
                g_hash_table_insert(themes, g_strdup(parent), row);
                g_ptr_array_add(ordered_themes, row);
            }

            if (child && strlen(child) > 0) {
                if (row->codes->len > 0) {
                    g_string_append(row->codes, ", ");
                }
                g_string_append(row->codes, child);
            }

            if (desc && strlen(desc) > 0) {
                if (strlen(row->description) > 0) {
                    char *old = row->description;
                    row->description = g_strdup_printf("%s | %s", old, desc);
                    g_free(old);
                } else {
                    g_free(row->description);
                    row->description = g_strdup(desc);
                }
            }
            g_strfreev(parts);
        }
        sqlite3_finalize (stmt);
    }

    // Now query the true unique frequency for each theme
    sqlite3_stmt *stmt_freq = db_get_theme_unique_frequencies(project_id);
    if (stmt_freq) {
        while (sqlite3_step(stmt_freq) == SQLITE_ROW) {
            const char *tema = (const char *)sqlite3_column_text(stmt_freq, 0);
            int freq = sqlite3_column_int(stmt_freq, 1);
            if (tema) {
                ThematicRow *row = g_hash_table_lookup(themes, tema);
                if (row) {
                    row->frequency = freq;
                }
            }
        }
        sqlite3_finalize(stmt_freq);
    }

    for (guint i = 0; i < ordered_themes->len; i++) {
        ThematicRow *row = g_ptr_array_index(ordered_themes, i);
        fprintf(f, "<tr><td style=\"padding: 8px;\">%s</td><td style=\"padding: 8px;\">%s</td><td style=\"padding: 8px; text-align: center;\">%d</td><td style=\"padding: 8px;\">%s</td></tr>\n", 
                row->theme_name ? row->theme_name : "", 
                row->codes ? row->codes->str : "", 
                row->frequency, 
                row->description ? row->description : "");
    }
    fprintf(f, "</tbody></table></body></html>\n");

    g_ptr_array_free(ordered_themes, TRUE);
    g_hash_table_destroy(themes);
    fclose (f);
    return true;
}
