import sys

with open('src/window.c', 'r') as f:
    content = f.read()

# 1. Update on_results_tag_selected
old_tag_selected = """static void
on_results_tag_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->selected_result_tag) {
        g_free (state->selected_result_tag);
        state->selected_result_tag = NULL;
    }
    
    if (row) {
        const char *tag_path = g_object_get_data (G_OBJECT (row), "tag_path");
        if (tag_path) {
            state->selected_result_tag = g_strdup (tag_path);
        }
    }
    
    gtk_list_box_invalidate_filter (GTK_LIST_BOX (state->results_list));
}"""

new_tag_selected = """static void
on_results_tag_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->selected_result_tag) {
        g_free (state->selected_result_tag);
        state->selected_result_tag = NULL;
    }
    
    if (row) {
        const char *tag_path = g_object_get_data (G_OBJECT (row), "tag_path");
        if (tag_path) {
            state->selected_result_tag = g_strdup (tag_path);
        }
    }
    
    state->results_dirty = TRUE;
    refresh_results (state);
}"""

if old_tag_selected in content:
    content = content.replace(old_tag_selected, new_tag_selected)

# 2. Update results_filter_func to always return TRUE (so we don't break anything else relying on it)
old_filter_func = """static gboolean
results_filter_func (GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->selected_result_tag) return TRUE;

    const char *tags_str = g_object_get_data (G_OBJECT (row), "tags_str");
    if (!tags_str) return FALSE;

    char **tags = g_strsplit (tags_str, "@@@", -1);
    gboolean match = FALSE;
    size_t sel_len = strlen (state->selected_result_tag);
    for (int i = 0; tags[i]; i++) {
        char **parts = g_strsplit (tags[i], "|||", 2);
        if (parts[0]) {
            if (g_strcmp0 (parts[0], state->selected_result_tag) == 0 ||
                (strncmp (parts[0], state->selected_result_tag, sel_len) == 0 &&
                 parts[0][sel_len] == '/')) {
                match = TRUE;
            }
        }
        g_strfreev (parts);
        if (match) break;
    }
    g_strfreev (tags);
    return match;
}"""

new_filter_func = """static gboolean
results_filter_func (GtkListBoxRow *row, gpointer user_data)
{
    return TRUE;
}"""

if old_filter_func in content:
    content = content.replace(old_filter_func, new_filter_func)

# 3. Update refresh_results to filter before counting
old_refresh_loop = """      int count = 0;
      int limit = state->results_limit > 0 ? state->results_limit : 200;
      for (guint i = 0; i < len; i++) {
          if (count >= limit) {
              GtkWidget *load_more_row = gtk_list_box_row_new ();
              gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (load_more_row), FALSE);
              GtkWidget *btn = gtk_button_new_with_label ("Cargar más...");
              gtk_widget_add_css_class (btn, "pill");
              gtk_widget_set_halign (btn, GTK_ALIGN_CENTER);
              gtk_widget_set_margin_top (btn, 12);
              gtk_widget_set_margin_bottom (btn, 12);
              g_signal_connect_swapped (btn, "clicked", G_CALLBACK (on_load_more_results_clicked), state);
              gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (load_more_row), btn);
              gtk_list_box_append (GTK_LIST_BOX (state->results_list), load_more_row);
              break;
          }
          ResultRow *res = g_ptr_array_index (state->cached_results, i);"""

new_refresh_loop = """      int count = 0;
      int limit = state->results_limit > 0 ? state->results_limit : 200;
      for (guint i = 0; i < len; i++) {
          ResultRow *res = g_ptr_array_index (state->cached_results, i);

          gboolean match = TRUE;
          if (state->selected_result_tag) {
              match = FALSE;
              if (res->tags_str) {
                  char **tags = g_strsplit (res->tags_str, "@@@", -1);
                  size_t sel_len = strlen (state->selected_result_tag);
                  for (int j = 0; tags[j]; j++) {
                      char **parts = g_strsplit (tags[j], "|||", 2);
                      if (parts[0]) {
                          if (g_strcmp0 (parts[0], state->selected_result_tag) == 0 ||
                              (strncmp (parts[0], state->selected_result_tag, sel_len) == 0 &&
                               parts[0][sel_len] == '/')) {
                              match = TRUE;
                          }
                      }
                      g_strfreev (parts);
                      if (match) break;
                  }
                  g_strfreev (tags);
              }
          }
          if (!match) continue;

          if (count >= limit) {
              GtkWidget *load_more_row = gtk_list_box_row_new ();
              gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (load_more_row), FALSE);
              GtkWidget *btn = gtk_button_new_with_label ("Cargar más...");
              gtk_widget_add_css_class (btn, "pill");
              gtk_widget_set_halign (btn, GTK_ALIGN_CENTER);
              gtk_widget_set_margin_top (btn, 12);
              gtk_widget_set_margin_bottom (btn, 12);
              g_signal_connect_swapped (btn, "clicked", G_CALLBACK (on_load_more_results_clicked), state);
              gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (load_more_row), btn);
              gtk_list_box_append (GTK_LIST_BOX (state->results_list), load_more_row);
              break;
          }
"""

if old_refresh_loop in content:
    content = content.replace(old_refresh_loop, new_refresh_loop)

with open('src/window.c', 'w') as f:
    f.write(content)
print("Done filtering fix")
