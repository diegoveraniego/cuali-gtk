#include "window.h"
#include "importer.h"
#include "database.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

const char *style_css = 
  "textview { font-family: 'Cantarell', sans-serif; font-size: 12pt; background-color: transparent; }"
  ".sidebar-list { background-color: transparent; }"
  ".sidebar-title { font-weight: bold; opacity: 0.5; font-size: 0.8rem; margin: 18px 0 6px 12px; }"
  ".document-view { background-color: @window_bg_color; border-radius: 12px; margin: 0; }"
  ".paper-sheet { background-color: @view_bg_color; border-radius: 4px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); margin: 40px auto; min-width: 650px; max-width: 800px; transition: all 0.3s; }"
  ".tag-badge { background-color: @accent_bg_color; color: white; border-radius: 6px; padding: 2px 10px; font-size: 0.85rem; font-weight: 600; }"
  ".tag-count-badge { background-color: rgba(0,0,0,0.1); border-radius: 12px; padding: 1px 8px; font-size: 0.8rem; margin-right: 8px; }"
  ".result-card { background-color: @view_bg_color; border-radius: 12px; padding: 20px; border: 1px solid rgba(0,0,0,0.05); margin-bottom: 12px; }"
  ".result-snippet { font-style: italic; font-family: serif; font-size: 1.1rem; line-height: 1.6; margin-bottom: 12px; display: block; }"
  ".result-meta { font-size: 0.85rem; opacity: 0.6; margin-top: 8px; }";

static char*
map_html (const char *html, int **out_map, int *out_len)
{
  if (!html) return NULL;
  int html_len = strlen(html);
  GString *out = g_string_new ("");
  int *map = g_new (int, html_len + 1);
  bool in_tag = false;
  int j = 0;
  for (int i = 0; html[i]; i++) {
    if (html[i] == '<') in_tag = true;
    else if (html[i] == '>') in_tag = false;
    else if (!in_tag) {
      map[j++] = i;
      g_string_append_c (out, html[i]);
    }
  }
  *out_map = map;
  *out_len = j;
  return g_string_free (out, FALSE);
}

static char*
strip_html (const char *html)
{
  if (!html) return NULL;
  GString *out = g_string_new ("");
  bool in_tag = false;
  for (const char *p = html; *p; p++) {
    if (*p == '<') in_tag = true;
    else if (*p == '>') in_tag = false;
    else if (!in_tag) g_string_append_c (out, *p);
  }
  return g_string_free (out, FALSE);
}

static int
html_to_plain (int html_offset, int *map, int len)
{
    for (int i = 0; i < len; i++) {
        if (map[i] >= html_offset) return i;
    }
    return len;
}

static void refresh_documents (CualiAppState *state);
static void load_document (CualiAppState *state, int document_id, const char *name, const char *contents);

static void
on_edit_toggle_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  if (state->current_document_id <= 0) return;

  state->is_editing = !state->is_editing;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  
  if (state->is_editing) {
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->text_view), TRUE);
    gtk_button_set_icon_name (GTK_BUTTON (state->edit_toggle), "emblem-ok-symbolic");
    // Remove highlights while editing to avoid confusion
    gtk_text_buffer_remove_tag_by_name (buffer, "highlight", NULL, NULL);
  } else {
    // Save changes
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    
    GString *html = g_string_new ("<p>");
    for (char *p = text; *p; p++) {
        if (*p == '\n') g_string_append (html, "</p><p>");
        else g_string_append_c (html, *p);
    }
    g_string_append (html, "</p>");
    
    db_document_update_contents (state->current_document_id, html->str);
    
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->text_view), FALSE);
    gtk_button_set_icon_name (GTK_BUTTON (state->edit_toggle), "document-edit-symbolic");
    
    // Reload document to refresh highlights and offset map
    load_document (state, state->current_document_id, "dummy", html->str);
    
    g_string_free (html, TRUE);
    g_free (text);
    refresh_documents (state);
  }
}

static void
refresh_project_info (CualiAppState *state)
{
  char *name = NULL, *desc = NULL;
  if (db_project_get_info (state->current_project_id, &name, &desc)) {
    gtk_editable_set_text (GTK_EDITABLE (state->project_name_entry), name ? name : "");
    gtk_editable_set_text (GTK_EDITABLE (state->project_desc_entry), desc ? desc : "");
    g_free (name);
    g_free (desc);
  }
}

static void
on_project_info_changed (GtkEditable *editable, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  const char *name = gtk_editable_get_text (GTK_EDITABLE (state->project_name_entry));
  const char *desc = gtk_editable_get_text (GTK_EDITABLE (state->project_desc_entry));
  db_project_update_info (state->current_project_id, name, desc);
}

static void
refresh_results_tags (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_tag_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_tag_list), child);

  sqlite3_stmt *stmt = db_tags_get_stats (state->current_project_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      const char *path = (const char *)sqlite3_column_text (stmt, 0);
      int count = sqlite3_column_int (stmt, 1);
      
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      
      GtkWidget *label = gtk_label_new (path);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_margin_start (label, 12);
      gtk_widget_set_margin_top (label, 8);
      gtk_widget_set_margin_bottom (label, 8);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);
      
      char count_str[16];
      snprintf (count_str, sizeof(count_str), "%d", count);
      GtkWidget *count_label = gtk_label_new (count_str);
      gtk_widget_add_css_class (count_label, "tag-count-badge");
      gtk_box_append (GTK_BOX (box), count_label);
      
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      gtk_list_box_append (GTK_LIST_BOX (state->results_tag_list), row);
    }
    sqlite3_finalize (stmt);
  }
}

static void
refresh_results (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_list), child);

  sqlite3_stmt *stmt = db_results_get_all (state->current_project_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      const char *snippet = (const char *)sqlite3_column_text (stmt, 0);
      const char *doc_name = (const char *)sqlite3_column_text (stmt, 1);
      const char *tags_str = (const char *)sqlite3_column_text (stmt, 2);
      
      GtkWidget *row = gtk_list_box_row_new ();
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
      
      GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
      gtk_widget_add_css_class (card, "result-card");
      
      GtkWidget *snip_label = gtk_label_new (NULL);
      char *clean_snippet = strip_html (snippet);
      gtk_label_set_markup (GTK_LABEL (snip_label), g_strdup_printf ("“%s”", clean_snippet));
      g_free (clean_snippet);
      gtk_label_set_wrap (GTK_LABEL (snip_label), TRUE);
      gtk_widget_set_halign (snip_label, GTK_ALIGN_START);
      gtk_widget_add_css_class (snip_label, "result-snippet");
      gtk_box_append (GTK_BOX (card), snip_label);
      
      GtkWidget *meta_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_box_append (GTK_BOX (card), meta_box);
      
      // Split tags and create badges
      if (tags_str) {
        char **tags = g_strsplit (tags_str, ", ", -1);
        for (int i = 0; tags[i]; i++) {
            GtkWidget *tag_badge = gtk_label_new (tags[i]);
            gtk_widget_add_css_class (tag_badge, "tag-badge");
            gtk_box_append (GTK_BOX (meta_box), tag_badge);
        }
        g_strfreev (tags);
      }

      GtkWidget *doc_label = gtk_label_new (doc_name);
      gtk_widget_add_css_class (doc_label, "result-meta");
      gtk_box_append (GTK_BOX (meta_box), doc_label);
      
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), card);
      gtk_list_box_append (GTK_LIST_BOX (state->results_list), row);
    }
    sqlite3_finalize (stmt);
  }
  refresh_results_tags (state);
}

static void
refresh_tags (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->tag_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->tag_list), child);

  sqlite3_stmt *stmt = db_tags_get_all (state->current_project_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      int id = sqlite3_column_int (stmt, 0);
      const char *path = (const char *)sqlite3_column_text (stmt, 1);
      int count = db_tag_get_count (id);
      
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      
      GtkWidget *label = gtk_label_new (path);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_margin_start (label, 12);
      gtk_widget_set_margin_top (label, 10);
      gtk_widget_set_margin_bottom (label, 10);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);
      
      if (count > 0) {
        char count_str[16];
        snprintf (count_str, sizeof(count_str), "%d", count);
        GtkWidget *count_label = gtk_label_new (count_str);
        gtk_widget_add_css_class (count_label, "tag-count-badge");
        gtk_box_append (GTK_BOX (box), count_label);
      }
      
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data (G_OBJECT (row), "tag-id", GINT_TO_POINTER (id));
      gtk_list_box_append (GTK_LIST_BOX (state->tag_list), row);
    }
    sqlite3_finalize (stmt);
  }
}

static void
load_document (CualiAppState *state, int document_id, const char *name, const char *contents)
{
  state->current_document_id = document_id;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  
  if (state->offset_map) g_free (state->offset_map);
  char *clean_text = map_html (contents, &state->offset_map, &state->plain_text_len);
  
  gtk_text_buffer_set_text (buffer, clean_text ? clean_text : "", -1);
  g_free (clean_text);
  
  sqlite3_stmt *stmt = db_highlights_get_for_document (document_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      int start_off = sqlite3_column_int (stmt, 0);
      int end_off = sqlite3_column_int (stmt, 1);
      
      int plain_start = html_to_plain (start_off, state->offset_map, state->plain_text_len);
      int plain_end = html_to_plain (end_off, state->offset_map, state->plain_text_len);
      
      GtkTextIter start_iter, end_iter;
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, plain_start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, plain_end);
      gtk_text_buffer_apply_tag_by_name (buffer, "highlight", &start_iter, &end_iter);
    }
    sqlite3_finalize (stmt);
  }
}

static void
on_delete_doc_clicked (GtkButton *button, gpointer user_data)
{
  GtkWidget *row = GTK_WIDGET (user_data);
  CualiAppState *state = g_object_get_data (G_OBJECT (row), "app-state");
  int id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "doc-id"));
  if (id > 0 && db_document_delete (id)) {
    if (state->current_document_id == id) {
      state->current_document_id = -1;
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view)), "", -1);
    }
    refresh_documents (state);
  }
}

static void
on_doc_imported (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  CualiAppState *state = (CualiAppState *)user_data;
  GFile *file = gtk_file_dialog_open_finish (dialog, res, NULL);
  if (file != NULL) {
    char *contents = NULL;
    gsize length;
    if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL)) {
      char *name = g_file_get_basename (file);
      GString *html = g_string_new ("<p>");
      for (char *p = contents; *p; p++) {
          if (*p == '\n') g_string_append (html, "</p><p>");
          else g_string_append_c (html, *p);
      }
      g_string_append (html, "</p>");
      db_document_add (state->current_project_id, name, html->str);
      refresh_documents (state);
      g_string_free (html, TRUE);
      g_free (name);
      g_free (contents);
    }
    g_object_unref (file);
  }
}

static void
refresh_documents (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->doc_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->doc_list), child);

  sqlite3_stmt *stmt = db_documents_get_all (state->current_project_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      int id = sqlite3_column_int (stmt, 0);
      const char *name = (const char *)sqlite3_column_text (stmt, 1);
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      GtkWidget *label = gtk_label_new (name);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_margin_start (label, 12);
      gtk_widget_set_margin_top (label, 10);
      gtk_widget_set_margin_bottom (label, 10);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);
      
      GtkWidget *delete_btn = gtk_button_new_from_icon_name ("user-trash-symbolic");
      gtk_widget_add_css_class (delete_btn, "flat");
      gtk_widget_set_valign (delete_btn, GTK_ALIGN_CENTER);
      g_object_set_data (G_OBJECT (row), "doc-id", GINT_TO_POINTER (id));
      g_object_set_data (G_OBJECT (row), "app-state", state);
      g_signal_connect (delete_btn, "clicked", G_CALLBACK (on_delete_doc_clicked), row);
      gtk_box_append (GTK_BOX (box), delete_btn);

      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      const char *contents = (const char *)sqlite3_column_text (stmt, 2);
      g_object_set_data_full (G_OBJECT (row), "doc-contents", g_strdup (contents), g_free);
      g_object_set_data_full (G_OBJECT (row), "doc-name", g_strdup (name), g_free);
      gtk_list_box_append (GTK_LIST_BOX (state->doc_list), row);
    }
    sqlite3_finalize (stmt);
  }
}

static void
on_doc_row_selected (GtkListBox    *listbox,
                     GtkListBoxRow *row,
                     gpointer       user_data)
{
  if (!row) return;
  CualiAppState *state = (CualiAppState *)user_data;
  int id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "doc-id"));
  const char *name = g_object_get_data (G_OBJECT (row), "doc-name");
  const char *contents = g_object_get_data (G_OBJECT (row), "doc-contents");
  load_document (state, id, name, contents);
}

static void
on_new_tag_activated (GtkEntry *entry, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (text && text[0] != '\0') {
    db_tag_add (state->current_project_id, text, "");
    gtk_editable_set_text (GTK_EDITABLE (entry), "");
    refresh_tags (state);
  }
}

static void
refresh_all (CualiAppState *state)
{
    refresh_project_info (state);
    refresh_tags (state);
    refresh_documents (state);
    refresh_results (state);
}

static char*
get_recent_file_path ()
{
    const char *config_dir = g_get_user_config_dir ();
    char *full_dir = g_build_filename (config_dir, "cuali-gtk", NULL);
    g_mkdir_with_parents (full_dir, 0755);
    char *path = g_build_filename (full_dir, "recent.txt", NULL);
    g_free (full_dir);
    return path;
}

static void
add_to_recent (const char *path)
{
    if (!path) return;
    char *recent_file = get_recent_file_path ();
    GList *lines = NULL;
    char *contents = NULL;
    
    if (g_file_get_contents (recent_file, &contents, NULL, NULL)) {
        char **split = g_strsplit (contents, "\n", -1);
        for (int i = 0; split[i]; i++) {
            if (strlen(split[i]) > 0 && g_strcmp0 (split[i], path) != 0)
                lines = g_list_append (lines, g_strdup (split[i]));
        }
        g_strfreev (split);
        g_free (contents);
    }
    
    lines = g_list_prepend (lines, g_strdup (path));
    
    GString *new_contents = g_string_new ("");
    int count = 0;
    for (GList *l = lines; l && count < 10; l = l->next) {
        g_string_append_printf (new_contents, "%s\n", (char *)l->data);
        count++;
    }
    
    g_file_set_contents (recent_file, new_contents->str, -1, NULL);
    g_string_free (new_contents, TRUE);
    g_list_free_full (lines, g_free);
    g_free (recent_file);
}

static void open_project_at_path (CualiAppState *state, const char *path);

static void
on_recent_row_selected (GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data)
{
    if (!row) return;
    CualiAppState *state = (CualiAppState *)user_data;
    const char *path = g_object_get_data (G_OBJECT (row), "project-path");
    open_project_at_path (state, path);
}

static void
populate_recent_list (CualiAppState *state)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (state->recent_list)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (state->recent_list), child);

    char *recent_file = get_recent_file_path ();
    char *contents = NULL;
    if (g_file_get_contents (recent_file, &contents, NULL, NULL)) {
        char **split = g_strsplit (contents, "\n", -1);
        bool empty = true;
        for (int i = 0; split[i]; i++) {
            if (strlen(split[i]) == 0) continue;
            if (!g_file_test (split[i], G_FILE_TEST_EXISTS)) continue;
            
            empty = false;
            GtkWidget *row = gtk_list_box_row_new ();
            GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_start (box, 12);
            gtk_widget_set_margin_end (box, 12);
            gtk_widget_set_margin_top (box, 8);
            gtk_widget_set_margin_bottom (box, 8);
            
            GtkWidget *icon = gtk_image_new_from_icon_name ("document-open-symbolic");
            gtk_box_append (GTK_BOX (box), icon);
            
            char *basename = g_path_get_basename (split[i]);
            GtkWidget *label = gtk_label_new (basename);
            gtk_widget_set_halign (label, GTK_ALIGN_START);
            gtk_box_append (GTK_BOX (box), label);
            g_free (basename);
            
            gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
            g_object_set_data_full (G_OBJECT (row), "project-path", g_strdup (split[i]), g_free);
            gtk_list_box_append (GTK_LIST_BOX (state->recent_list), row);
        }
        g_strfreev (split);
        g_free (contents);
        
        gtk_widget_set_visible (gtk_widget_get_parent (state->recent_list), !empty);
    } else {
        gtk_widget_set_visible (gtk_widget_get_parent (state->recent_list), FALSE);
    }
    g_free (recent_file);
}

static void
open_project_at_path (CualiAppState *state, const char *path)
{
    if (db_init (path)) {
      state->current_project_id = db_project_get_first_id ();
      state->current_document_id = -1;
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view)), "", -1);
      refresh_all (state);
      adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (state->root_stack), "main");
      add_to_recent (path);
    }
}

static void
on_project_created (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  CualiAppState *state = (CualiAppState *)user_data;
  GFile *file = gtk_file_dialog_save_finish (dialog, res, NULL);
  if (file != NULL) {
    char *path = g_file_get_path (file);
    if (db_init (path)) {
      db_project_add ("Nuevo Proyecto", "");
      open_project_at_path (state, path);
    }
    g_free (path);
    g_object_unref (file);
  }
}

static void
on_new_project_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_save (dialog, GTK_WINDOW (state->window), NULL, on_project_created, state);
}

static void
on_project_opened (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  CualiAppState *state = (CualiAppState *)user_data;
  GFile *file = gtk_file_dialog_open_finish (dialog, res, NULL);
  if (file != NULL) {
    char *path = g_file_get_path (file);
    open_project_at_path (state, path);
    g_free (path);
    g_object_unref (file);
  }
}

static void
on_open_project_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (dialog, GTK_WINDOW (state->window), NULL, on_project_opened, state);
}

static void
on_add_button_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (dialog, GTK_WINDOW (state->window), NULL, on_doc_imported, state);
}

static void
on_highlight_button_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  if (state->current_document_id <= 0) return;
  GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (state->tag_list));
  if (!selected_row) return;
  int tag_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (selected_row), "tag-id"));
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  GtkTextIter start, end;
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end)) {
    int start_off = gtk_text_iter_get_offset(&start);
    int end_off = gtk_text_iter_get_offset(&end);
    
    int html_start = state->offset_map[start_off];
    int html_end = state->offset_map[end_off];
    
    char *snippet = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    int hl_id = db_highlight_add (state->current_document_id, html_start, html_end, snippet);
    if (hl_id > 0) {
      db_highlight_link_tag (hl_id, tag_id);
      gtk_text_buffer_apply_tag_by_name (buffer, "highlight", &start, &end);
      refresh_tags (state);
      refresh_results (state);
    }
    g_free (snippet);
  }
}

static void
on_view_stack_visible_child_changed (GObject *object, GParamSpec *pspec, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *name = adw_view_stack_get_visible_child_name (ADW_VIEW_STACK (state->view_stack));
    if (g_strcmp0 (name, "results") == 0) refresh_results (state);
    if (g_strcmp0 (name, "info") == 0) refresh_project_info (state);
}

void window_init(GtkApplication *app) {
    CualiAppState *state = g_new0 (CualiAppState, 1);
    state->current_document_id = -1;

    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (provider, style_css);
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *window = adw_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Cuali GTK");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 800);

    state->root_stack = adw_view_stack_new ();
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (window), state->root_stack);

    /* --- Welcome Screen --- */
    GtkWidget *welcome_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign (welcome_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (welcome_box, GTK_ALIGN_CENTER);
    adw_view_stack_add_named (ADW_VIEW_STACK (state->root_stack), welcome_box, "welcome");

    GtkWidget *logo = gtk_image_new_from_icon_name ("org.gnome.TextEditor-symbolic");
    gtk_image_set_pixel_size (GTK_IMAGE (logo), 128);
    gtk_box_append (GTK_BOX (welcome_box), logo);

    GtkWidget *title = gtk_label_new ("Bienvenido a Cuali");
    gtk_widget_add_css_class (title, "title-1");
    gtk_box_append (GTK_BOX (welcome_box), title);

    GtkWidget *btns_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append (GTK_BOX (welcome_box), btns_box);

    GtkWidget *open_btn = gtk_button_new_with_label ("Abrir Proyecto Existente");
    gtk_widget_add_css_class (open_btn, "suggested-action");
    gtk_widget_add_css_class (open_btn, "pill");
    g_signal_connect (open_btn, "clicked", G_CALLBACK (on_open_project_clicked), state);
    gtk_box_append (GTK_BOX (btns_box), open_btn);

    GtkWidget *new_btn = gtk_button_new_with_label ("Crear Nuevo Proyecto");
    gtk_widget_add_css_class (new_btn, "pill");
    g_signal_connect (new_btn, "clicked", G_CALLBACK (on_new_project_clicked), state);
    gtk_box_append (GTK_BOX (btns_box), new_btn);

    GtkWidget *recent_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top (recent_box, 30);
    gtk_box_append (GTK_BOX (welcome_box), recent_box);
    
    GtkWidget *recent_label = gtk_label_new ("Proyectos Recientes");
    gtk_widget_add_css_class (recent_label, "sidebar-title");
    gtk_box_append (GTK_BOX (recent_box), recent_label);
    
    state->recent_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->recent_list, "sidebar-list");
    gtk_widget_set_size_request (state->recent_list, 300, -1);
    g_signal_connect (state->recent_list, "row-selected", G_CALLBACK (on_recent_row_selected), state);
    gtk_box_append (GTK_BOX (recent_box), state->recent_list);

    populate_recent_list (state);

    /* --- Main App Content --- */
    GtkWidget *toast_overlay = adw_toast_overlay_new ();
    state->toast_overlay = toast_overlay;
    adw_view_stack_add_named (ADW_VIEW_STACK (state->root_stack), toast_overlay, "main");

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adw_toast_overlay_set_child (ADW_TOAST_OVERLAY (toast_overlay), main_box);

    GtkWidget *header_bar = adw_header_bar_new();
    gtk_box_append(GTK_BOX(main_box), header_bar);
    // Add a back button to welcome screen in header bar
    GtkWidget *back_btn = gtk_button_new_from_icon_name ("go-previous-symbolic");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), back_btn);
    g_signal_connect_data(back_btn, "clicked", G_CALLBACK(adw_view_stack_set_visible_child_name), g_strdup("welcome"), (GClosureNotify)g_free, G_CONNECT_SWAPPED);
    g_signal_connect_swapped (back_btn, "clicked", G_CALLBACK (populate_recent_list), state);

    GtkWidget *open_button = gtk_button_new_from_icon_name ("folder-open-symbolic");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), open_button);
    g_signal_connect (open_button, "clicked", G_CALLBACK (on_open_project_clicked), state);

    GtkWidget *add_button = gtk_button_new_from_icon_name ("list-add-symbolic");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), add_button);
    g_signal_connect (add_button, "clicked", G_CALLBACK (on_add_button_clicked), state);

    state->view_stack = adw_view_stack_new ();
    g_signal_connect (state->view_stack, "notify::visible-child", G_CALLBACK (on_view_stack_visible_child_changed), state);
    
    GtkWidget *view_switcher = adw_view_switcher_new ();
    adw_view_switcher_set_stack (ADW_VIEW_SWITCHER (view_switcher), ADW_VIEW_STACK (state->view_stack));
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), view_switcher);

    GtkWidget *hl_button = gtk_button_new_from_icon_name ("format-text-underline-symbolic");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), hl_button);
    g_signal_connect (hl_button, "clicked", G_CALLBACK (on_highlight_button_clicked), state);
    
    state->edit_toggle = gtk_button_new_from_icon_name ("document-edit-symbolic");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), state->edit_toggle);
    g_signal_connect (state->edit_toggle, "clicked", G_CALLBACK (on_edit_toggle_clicked), state);
    
    GtkWidget *export_button = gtk_button_new_from_icon_name ("document-save-symbolic");
    adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), export_button);

    /* --- Pestaña 1: Información --- */
    GtkWidget *info_page = adw_preferences_page_new ();
    AdwViewStackPage *page;
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), info_page, "info", "Información");
    adw_view_stack_page_set_icon_name (page, "dialog-information-symbolic");
    
    GtkWidget *info_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (info_group), "Metadatos del Proyecto");
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (info_page), ADW_PREFERENCES_GROUP (info_group));
    
    GtkWidget *name_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Nombre del Proyecto");
    state->project_name_entry = name_row;
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (info_group), name_row);
    g_signal_connect (name_row, "changed", G_CALLBACK (on_project_info_changed), state);

    GtkWidget *desc_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (desc_row), "Descripción");
    state->project_desc_entry = desc_row;
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (info_group), desc_row);
    g_signal_connect (desc_row, "changed", G_CALLBACK (on_project_info_changed), state);

    /* --- Pestaña 2: Documentos --- */
    GtkWidget *analysis_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), analysis_box, "analysis", "Documentos");
    adw_view_stack_page_set_icon_name (page, "text-x-generic-symbolic");
    
    GtkWidget *split_view = adw_overlay_split_view_new();
    gtk_box_append(GTK_BOX(analysis_box), split_view);
    gtk_widget_set_hexpand(split_view, TRUE);

    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (sidebar_box, "sidebar");
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_view), sidebar_box);
    
    GtkWidget *docs_title = gtk_label_new("DOCUMENTOS");
    gtk_widget_add_css_class (docs_title, "sidebar-title");
    gtk_widget_set_halign (docs_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar_box), docs_title);
    state->doc_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->doc_list, "sidebar-list");
    gtk_box_append (GTK_BOX (sidebar_box), state->doc_list);
    g_signal_connect (state->doc_list, "row-selected", G_CALLBACK (on_doc_row_selected), state);

    GtkWidget *tags_title = gtk_label_new("ETIQUETAS");
    gtk_widget_add_css_class (tags_title, "sidebar-title");
    gtk_widget_set_halign (tags_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar_box), tags_title);
    GtkWidget *tag_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (tag_entry), "Nueva etiqueta...");
    gtk_widget_set_margin_start (tag_entry, 10);
    gtk_widget_set_margin_end (tag_entry, 10);
    gtk_widget_set_margin_top (tag_entry, 5);
    gtk_widget_set_margin_bottom (tag_entry, 5);
    gtk_box_append (GTK_BOX (sidebar_box), tag_entry);
    g_signal_connect (tag_entry, "activate", G_CALLBACK (on_new_tag_activated), state);
    GtkWidget *scrolled_tags = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled_tags, TRUE);
    gtk_box_append (GTK_BOX (sidebar_box), scrolled_tags);
    state->tag_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->tag_list, "sidebar-list");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_tags), state->tag_list);

    GtkWidget *content_scroll = gtk_scrolled_window_new ();
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_view), content_scroll);
    GtkWidget *paper_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (paper_box, "paper-sheet");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_scroll), paper_box);
    GtkWidget *text_view = gtk_text_view_new();
    state->text_view = text_view;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_create_tag (buffer, "highlight", 
                               "background", "#f9f06b", 
                               "foreground", "#1a1a1a", 
                               NULL);
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_box_append (GTK_BOX (paper_box), text_view);

    /* --- Pestaña 3: Resultados --- */
    GtkWidget *results_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), results_box, "results", "Resultados");
    adw_view_stack_page_set_icon_name (page, "view-list-bullet-symbolic");

    GtkWidget *res_split_view = adw_overlay_split_view_new();
    gtk_box_append(GTK_BOX(results_box), res_split_view);
    gtk_widget_set_hexpand(res_split_view, TRUE);

    GtkWidget *res_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (res_sidebar, "sidebar");
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_sidebar);

    GtkWidget *stats_title = gtk_label_new("ESTADÍSTICAS");
    gtk_widget_add_css_class (stats_title, "sidebar-title");
    gtk_widget_set_halign (stats_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(res_sidebar), stats_title);

    state->results_tag_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->results_tag_list, "sidebar-list");
    gtk_box_append (GTK_BOX (res_sidebar), state->results_tag_list);

    GtkWidget *res_content_scroll = gtk_scrolled_window_new ();
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_content_scroll);
    
    state->results_list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->results_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class (state->results_list, "sidebar-list");
    gtk_widget_set_margin_start (state->results_list, 20);
    gtk_widget_set_margin_end (state->results_list, 20);
    gtk_widget_set_margin_top (state->results_list, 20);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (res_content_scroll), state->results_list);

    gtk_box_append (GTK_BOX (main_box), state->view_stack);
    gtk_widget_set_vexpand (state->view_stack, TRUE);

    gtk_window_present(GTK_WINDOW(window));
}
