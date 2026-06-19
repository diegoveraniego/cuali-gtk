import sys

with open('src/window.c', 'r') as f:
    content = f.read()

old_cb = """static void
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

new_cb = """static void
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

if old_cb in content:
    content = content.replace(old_cb, new_cb)


old_do_export = """static void
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

new_do_export = """static void
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

if old_do_export in content:
    content = content.replace(old_do_export, new_do_export)


old_menu = """    GtkWidget *export_cb_item = gtk_button_new_with_label ("Export codebook (CSV)");
    gtk_widget_set_halign (export_cb_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_cb_item, "flat");
    g_signal_connect (export_cb_item, "clicked", G_CALLBACK (on_export_codebook_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_cb_item);

    GtkWidget *export_thm_item = gtk_button_new_with_label ("Export thematic table (HTML)");
    gtk_widget_set_halign (export_thm_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_thm_item, "flat");
    g_signal_connect (export_thm_item, "clicked", G_CALLBACK (on_export_thematic_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_thm_item);"""

new_menu = """    GtkWidget *export_cb_item = gtk_button_new_with_label ("Export codebook (CSV)");
    gtk_widget_set_halign (export_cb_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_cb_item, "flat");
    g_signal_connect (export_cb_item, "clicked", G_CALLBACK (on_export_codebook_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_cb_item);"""

if old_menu in content:
    content = content.replace(old_menu, new_menu)

with open('src/window.c', 'w') as f:
    f.write(content)
print("Reverted window.c")
