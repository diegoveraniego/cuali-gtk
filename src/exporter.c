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
