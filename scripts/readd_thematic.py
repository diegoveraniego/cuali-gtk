import sys

# 1. Update database.h
with open('include/database.h', 'r') as f:
    content = f.read()

if "db_get_theme_unique_frequencies" not in content:
    content = content.replace(
        "sqlite3_stmt* db_tags_get_stats(int project_id);",
        "sqlite3_stmt* db_tags_get_stats(int project_id);\nsqlite3_stmt* db_get_theme_unique_frequencies(int project_id);"
    )
    with open('include/database.h', 'w') as f:
        f.write(content)

# 2. Update database.c
with open('src/database.c', 'r') as f:
    content = f.read()

new_db_func = """
sqlite3_stmt* db_get_theme_unique_frequencies(int project_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT substr(t.path, 1, instr(t.path, '/') - 1) as tema, COUNT(DISTINCT ht.highlight_id) as freq "
        "FROM tags t "
        "JOIN highlight_tags ht ON t.id = ht.tag_id "
        "WHERE t.project_id = ? AND instr(t.path, '/') > 0 "
        "GROUP BY tema;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, project_id);
    return stmt;
}
"""

if "db_get_theme_unique_frequencies" not in content:
    content = content + "\n" + new_db_func
    with open('src/database.c', 'w') as f:
        f.write(content)


# 3. Update exporter.h
with open('include/exporter.h', 'r') as f:
    content = f.read()

if "export_thematic_table_html" not in content:
    content = content.replace(
        "bool export_codebook_csv (int project_id, const char *path);",
        "bool export_codebook_csv (int project_id, const char *path);\nbool export_thematic_table_html (int project_id, const char *path);"
    )
    with open('include/exporter.h', 'w') as f:
        f.write(content)


# 4. Update exporter.c
with open('src/exporter.c', 'r') as f:
    content = f.read()

thematic_code = """
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

    fprintf(f, "<!DOCTYPE html><html><head><meta charset=\\"UTF-8\\"></head><body>\\n");
    fprintf(f, "<table border=\\"1\\" style=\\"border-collapse: collapse; width: 100%%; font-family: sans-serif;\\">\\n");
    fprintf(f, "<thead><tr><th>Tema Principal</th><th>Códigos Emergentes</th><th>Frecuencia (Únicas)</th><th>Descripción Analítica</th></tr></thead>\\n");
    fprintf(f, "<tbody>\\n");

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
        fprintf(f, "<tr><td style=\\"padding: 8px;\\">%s</td><td style=\\"padding: 8px;\\">%s</td><td style=\\"padding: 8px; text-align: center;\\">%d</td><td style=\\"padding: 8px;\\">%s</td></tr>\\n", 
                row->theme_name ? row->theme_name : "", 
                row->codes ? row->codes->str : "", 
                row->frequency, 
                row->description ? row->description : "");
    }
    fprintf(f, "</tbody></table></body></html>\\n");

    g_ptr_array_free(ordered_themes, TRUE);
    g_hash_table_destroy(themes);
    fclose (f);
    return true;
}
"""

if "export_thematic_table_html" not in content:
    content = content + "\n" + thematic_code
    with open('src/exporter.c', 'w') as f:
        f.write(content)


# 5. Update window.c
with open('src/window.c', 'r') as f:
    content = f.read()

old_cb = """static void
on_export_csv_save_response (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    gboolean codebook = GPOINTER_TO_INT (args[1]);
    g_free (args);

    GFile *file = gtk_file_dialog_save_finish (dialog, res, NULL);
    if (file) {
        char *path = g_file_get_path (file);
        bool success = codebook
        ? export_codebook_csv   (state->current_project_id, path)
        : export_highlights_csv (state->current_project_id, path);"""

new_cb = """static void
on_export_csv_save_response (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int export_type = GPOINTER_TO_INT (args[1]);
    g_free (args);

    GFile *file = gtk_file_dialog_save_finish (dialog, res, NULL);
    if (file) {
        char *path = g_file_get_path (file);
        bool success = false;
        if (export_type == 0) success = export_highlights_csv (state->current_project_id, path);
        else if (export_type == 1) success = export_codebook_csv (state->current_project_id, path);
        else if (export_type == 2) success = export_thematic_table_html (state->current_project_id, path);"""

if old_cb in content:
    content = content.replace(old_cb, new_cb)

old_do_export = """static void
do_export_csv (CualiAppState *state, gboolean codebook)
{
    if (state->current_project_id <= 0) return;
    gpointer *args = g_new (gpointer, 2);
    args[0] = state;
    args[1] = GINT_TO_POINTER ((int)codebook);

    GtkFileDialog *dlg = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dlg, codebook ? "Export codebook" : "Export highlights");
    gtk_file_dialog_set_initial_name (dlg, codebook ? "codebook.csv" : "highlights.csv");
    gtk_file_dialog_save (dlg, GTK_WINDOW (state->window), NULL,
                          on_export_csv_save_response, args);
    g_object_unref (dlg);
}

static void on_export_csv_clicked      (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, FALSE); }
static void on_export_codebook_clicked (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, TRUE);  }"""

new_do_export = """static void
do_export_csv (CualiAppState *state, int export_type)
{
    if (state->current_project_id <= 0) return;
    gpointer *args = g_new (gpointer, 2);
    args[0] = state;
    args[1] = GINT_TO_POINTER (export_type);

    GtkFileDialog *dlg = gtk_file_dialog_new ();
    if (export_type == 1) {
        gtk_file_dialog_set_title (dlg, "Export codebook");
        gtk_file_dialog_set_initial_name (dlg, "codebook.csv");
    } else if (export_type == 2) {
        gtk_file_dialog_set_title (dlg, "Export thematic table");
        gtk_file_dialog_set_initial_name (dlg, "thematic_table.html");
    } else {
        gtk_file_dialog_set_title (dlg, "Export highlights");
        gtk_file_dialog_set_initial_name (dlg, "highlights.csv");
    }
    gtk_file_dialog_save (dlg, GTK_WINDOW (state->window), NULL,
                          on_export_csv_save_response, args);
    g_object_unref (dlg);
}

static void on_export_csv_clicked      (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, 0); }
static void on_export_codebook_clicked (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, 1);  }
static void on_export_thematic_clicked (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, 2);  }"""

if old_do_export in content:
    content = content.replace(old_do_export, new_do_export)

old_menu = """    GtkWidget *export_cb_item = gtk_button_new_with_label ("Export codebook (CSV)");
    gtk_widget_set_halign (export_cb_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_cb_item, "flat");
    g_signal_connect (export_cb_item, "clicked", G_CALLBACK (on_export_codebook_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_cb_item);"""

new_menu = """    GtkWidget *export_cb_item = gtk_button_new_with_label ("Export codebook (CSV)");
    gtk_widget_set_halign (export_cb_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_cb_item, "flat");
    g_signal_connect (export_cb_item, "clicked", G_CALLBACK (on_export_codebook_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_cb_item);

    GtkWidget *export_thm_item = gtk_button_new_with_label ("Export thematic table (HTML) 🛈");
    gtk_widget_set_tooltip_text(export_thm_item, "Calcula la frecuencia real de citas únicas por tema, evitando contar un mismo párrafo dos veces si tiene múltiples etiquetas del mismo tema.");
    gtk_widget_set_halign (export_thm_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_thm_item, "flat");
    g_signal_connect (export_thm_item, "clicked", G_CALLBACK (on_export_thematic_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_thm_item);"""

if old_menu in content:
    content = content.replace(old_menu, new_menu)

with open('src/window.c', 'w') as f:
    f.write(content)

print("Applied HTML export with true unique frequencies and tooltip")
