#include "window.h"
#include "importer.h"
#include "database.h"
#include "exporter.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <math.h>
#include "visualizations.h"

const char *style_css = 
  "* { font-family: \"Inter\", sans-serif; }"
  "textview { font-family: \"Inter\", sans-serif; font-size: 11pt; font-weight: 400; letter-spacing: 0.01em; }"
  "label { font-family: \"Inter\", sans-serif; }"
  "textview.vim-visual > text > selection { background-color: @theme_fg_color; color: @theme_bg_color; }"
  "textview > text > selection { background-color: #3584e4; color: #ffffff; }"
  "textview.vim-normal > text { caret-color: transparent; }"
  "textview.vim-visual > text { caret-color: transparent; }"
  ".heading { font-family: \"Inter\", sans-serif; font-weight: 500; }"
  ".vim-badge { background-color: @accent_bg_color; color: @accent_fg_color; font-family: \"Inter\", monospace; font-weight: 700; font-size: 10pt; padding: 2px 8px; border-radius: 4px; }"
  ".sidebar-list { margin: 6px; }"
  ".sidebar-title { font-weight: bold; opacity: 0.5; font-size: 0.8rem; margin-top: 18px; margin-bottom: 6px; margin-left: 12px; }"
  ".document-view { background-color: @window_bg_color; border-radius: 12px; }"
  ".paper-sheet { background-color: transparent; border-radius: 0px; margin: 0px; transition: all 0.3s; }"
  ".tag-badge { background-color: @accent_bg_color; color: white; border-radius: 6px; padding: 2px 10px; font-size: 0.85rem; font-weight: 600; }"
  ".tag-count-badge { background-color: rgba(0,0,0,0.1); border-radius: 12px; padding: 1px 8px; font-size: 0.8rem; margin-right: 8px; }"
  ".results-list { background-color: @window_bg_color; }"
  ".result-card { background-color: @view_bg_color; border-radius: 12px; padding: 20px; border: 1px solid rgba(0,0,0,0.05); margin-bottom: 12px; }"
  ".result-snippet { font-style: italic; font-size: 1.1rem; line-height: 1.6; margin-bottom: 12px; }"
  ".result-meta { font-size: 0.85rem; opacity: 0.6; margin-top: 8px; }"
  ".result-memo-box { background-color: rgba(229, 165, 10, 0.08); border-left: 3px solid #e5a50a; border-radius: 4px; padding: 10px 14px; margin-top: 8px; margin-bottom: 8px; }";

static void
set_button_resource_icon (GtkWidget *btn, const char *resource_path)
{
    GFile *file = g_file_new_for_uri (resource_path);
    GIcon *gicon = g_file_icon_new (file);
    g_object_unref (file);

    GtkWidget *img = gtk_image_new_from_gicon (gicon);
    g_object_unref (gicon);

    gtk_button_set_child (GTK_BUTTON (btn), img);
}

static GtkWidget *
create_resource_icon_button (const char *resource_path)
{
    GtkWidget *btn = gtk_button_new ();
    set_button_resource_icon (btn, resource_path);
    return btn;
}

static char*
map_html (const char *html, int **out_map, int *out_len)
{
  if (!html) return NULL;
  int html_len = strlen(html);
  GString *out = g_string_new ("");
  int *map = g_new (int, html_len * 2 + 1);
  bool in_tag = false;
  int j = 0;
  for (int i = 0; html[i]; i++) {
    if (html[i] == '<') {
      in_tag = true;
      if (strncmp(&html[i], "</p>", 4) == 0 || strncmp(&html[i], "<br", 3) == 0) {
         if (out->len > 0 && out->str[out->len - 1] != '\n') {
           map[j++] = i;
           g_string_append_c (out, '\n');
         }
      }
    }
    else if (html[i] == '>') in_tag = false;
    else if (!in_tag) {
      if (html[i] == '\n' && out->len > 0 && out->str[out->len - 1] == '\n') {
          // skip duplicate
      } else {
          map[j++] = i;
          g_string_append_c (out, html[i]);
      }
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
    if (*p == '<') {
        in_tag = true;
        if (strncmp(p, "</p>", 4) == 0 || strncmp(p, "<br", 3) == 0) {
            if (out->len > 0 && out->str[out->len - 1] != '\n') {
                g_string_append_c (out, '\n');
            }
        }
    }
    else if (*p == '>') in_tag = false;
    else if (!in_tag) {
        if (*p == '\n' && out->len > 0 && out->str[out->len - 1] == '\n') continue;
        g_string_append_c (out, *p);
    }
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
static void populate_recent_list (CualiAppState *state);
static void refresh_results (CualiAppState *state);
static void refresh_tags (CualiAppState *state);
static void show_tag_edit_dialog (CualiAppState *state, int tag_id);
static void refresh_stats (CualiAppState *state);
static void update_status_bar (CualiAppState *state);
static void search_clear_matches (CualiAppState *state);
static void search_find (CualiAppState *state, bool forward);

static void refresh_revision_list (CualiAppState *state);
static void refresh_revision_doc_filter_list (CualiAppState *state);
static void load_revision_highlight (CualiAppState *state, int highlight_id);
static void revision_shifter_redraw (CualiAppState *state);
static void update_save_indicator (CualiAppState *state, gboolean dirty);
static void mark_results_dirty (CualiAppState *state);
static GPtrArray* fetch_results (CualiAppState *state);
static void result_row_free (ResultRow *row);
static void on_revision_row_selected (GtkListBox *list_box, GtkListBoxRow *row, gpointer user_data);
static void on_revision_save_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_prev_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_next_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_start_back_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_start_forward_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_end_back_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_end_forward_clicked (GtkButton *btn, gpointer user_data);
static void on_revision_new_tag_activated (GtkEntry *entry, gpointer user_data);
static void on_revision_sidebar_search_changed (GtkSearchEntry *entry, gpointer user_data);
static gboolean revision_sidebar_filter_func (GtkListBoxRow *row, gpointer user_data);

typedef struct {
    CualiAppState *state;
    int offset;
    double scroll_pos;
} ScrollRestoreData;

static gboolean
restore_scroll_idle (gpointer user_data)
{
    ScrollRestoreData *data = (ScrollRestoreData *)user_data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (data->state->text_view));
    
    if (data->offset >= 0) {
        GtkTextIter iter;
        int len = gtk_text_buffer_get_char_count (buffer);
        int off = data->offset > len ? len : data->offset;
        gtk_text_buffer_get_iter_at_offset (buffer, &iter, off);
        gtk_text_buffer_place_cursor (buffer, &iter);
    }
    
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gtk_widget_get_ancestor (data->state->text_view, GTK_TYPE_SCROLLED_WINDOW)));
    if (adj) {
        double max = gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj);
        double pos = data->scroll_pos > max ? max : data->scroll_pos;
        gtk_adjustment_set_value (adj, pos);
    }
    
    g_free (data);
    return FALSE;
}

static const char *TAG_COLORS[] = {
    "#3584e4",  /* Blue 3 */
    "#33d17a",  /* Green 3 */
    "#f6d32d",  /* Yellow 3 */
    "#ff7800",  /* Orange 3 */
    "#e01b24",  /* Red 3 */
    "#9141ac",  /* Purple 3 */
    "#986a44",  /* Brown 3 */
    "#3d3846",  /* Dark 3 */
};
#define TAG_COLORS_COUNT 8

static void load_document (CualiAppState *state, int document_id, const char *name, const char *contents);
static void apply_highlight_tag (GtkTextBuffer *buffer, int hl_id, GtkTextIter *start, GtkTextIter *end, const char *color);

static gboolean
tag_filter_func (GtkListBoxRow *row, gpointer user_data)
{
    GtkEditable *entry = GTK_EDITABLE (user_data);
    const char *text = gtk_editable_get_text (entry);
    if (!text || *text == '\0') return TRUE;

    GtkWidget *box = gtk_list_box_row_get_child (row);
    GtkWidget *label = gtk_widget_get_first_child (box);
    if (!GTK_IS_LABEL (label)) return TRUE;
    
    const char *tag_name = gtk_label_get_text (GTK_LABEL (label));
    
    char *lower_tag = g_utf8_strdown (tag_name, -1);
    char *lower_text = g_utf8_strdown (text, -1);
    gboolean match = strstr (lower_tag, lower_text) != NULL;
    g_free (lower_tag);
    g_free (lower_text);
    return match;
}

static gboolean
results_filter_func (GtkListBoxRow *row, gpointer user_data)
{
    return TRUE;
}


static void
on_res_sidebar_toggle (GtkToggleButton *btn, gpointer user_data)
{
    AdwOverlaySplitView *sv = ADW_OVERLAY_SPLIT_VIEW (user_data);
    adw_overlay_split_view_set_show_sidebar (sv, gtk_toggle_button_get_active (btn));
}

static void
load_document (CualiAppState *state, int document_id, const char *name, const char *contents)
{
  state->current_document_id = document_id;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  
  GtkTextMark *mark = gtk_text_buffer_get_insert (buffer);
  GtkTextIter old_cursor;
  gtk_text_buffer_get_iter_at_mark (buffer, &old_cursor, mark);
  int offset = gtk_text_iter_get_offset (&old_cursor);
  
  GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gtk_widget_get_ancestor (state->text_view, GTK_TYPE_SCROLLED_WINDOW)));
  double scroll_pos = adj ? gtk_adjustment_get_value (adj) : 0;

  /* set_text is irreversible and clears history, cannot be inside user_action */
  if (state->offset_map) g_free (state->offset_map);
  char *clean_text = map_html (contents, &state->offset_map, &state->plain_text_len);
  
  gtk_text_buffer_set_text (buffer, clean_text ? clean_text : "", -1);
  
  gtk_text_buffer_begin_user_action (buffer);
  
  sqlite3_stmt *color_stmt = db_highlight_colors_for_document (document_id);
  GHashTable *color_map = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  if (color_stmt) {
      while (sqlite3_step (color_stmt) == SQLITE_ROW) {
          int h_id = sqlite3_column_int (color_stmt, 0);
          const char *color_str = (const char *)sqlite3_column_text (color_stmt, 1);
          if (color_str) {
              g_hash_table_insert (color_map, GINT_TO_POINTER (h_id), g_strdup (color_str));
          }
      }
      sqlite3_finalize (color_stmt);
  }

  sqlite3_stmt *stmt = db_highlights_get_for_document (document_id);
  state->cached_highlight_count = 0;
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      state->cached_highlight_count++;
      int start_off = sqlite3_column_int (stmt, 0);
      int end_off = sqlite3_column_int (stmt, 1);
      int hl_id = sqlite3_column_int (stmt, 2);
      
      if (clean_text) {
          int len = (int)strlen (clean_text);
          int p_start = start_off < 0 ? 0 : (start_off > len ? len : start_off);
          int p_end   = end_off   < p_start ? p_start : (end_off > len ? len : end_off);
          
          int char_start = (int)g_utf8_pointer_to_offset (clean_text, clean_text + p_start);
          int char_end   = (int)g_utf8_pointer_to_offset (clean_text, clean_text + p_end);
          
          GtkTextIter start_iter, end_iter;
          gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, char_start);
          gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, char_end);
          const char *hl_color = g_hash_table_lookup (color_map, GINT_TO_POINTER (hl_id));
          apply_highlight_tag (buffer, hl_id, &start_iter, &end_iter, hl_color);
      }
    }
    sqlite3_finalize (stmt);
  }
  g_hash_table_destroy (color_map);
  gtk_text_buffer_end_user_action (buffer);

  ScrollRestoreData *data = g_new0 (ScrollRestoreData, 1);
  data->state = state;
  data->offset = offset;
  data->scroll_pos = scroll_pos;
  g_idle_add (restore_scroll_idle, data);

  g_free (clean_text);

  update_status_bar (state);
  update_save_indicator (state, FALSE);
}

static void
on_results_tag_tree_cursor_changed(GtkTreeView *tree_view, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (state->selected_result_tag) {
        g_free (state->selected_result_tag);
        state->selected_result_tag = NULL;
    }

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GString *full_path = g_string_new("");
        GtkTreeIter current = iter;
        while (TRUE) {
            char *name = NULL;
            gtk_tree_model_get(model, &current, 1, &name, -1);
            if (name) {
                if (full_path->len > 0) g_string_prepend(full_path, "/");
                g_string_prepend(full_path, name);
                g_free(name);
            }
            GtkTreeIter parent;
            if (!gtk_tree_model_iter_parent(model, &parent, &current)) break;
            current = parent;
        }
        if (full_path->len > 0) {
            state->selected_result_tag = g_string_free(full_path, FALSE);
        } else {
            g_string_free(full_path, TRUE);
        }
    }

    state->results_dirty = TRUE;
    refresh_results(state);
}

static void on_results_tag_edit_clicked(GtkButton *btn, gpointer user_data);
static void on_results_tag_merge_clicked(GtkButton *btn, gpointer user_data);

static void on_results_tag_tree_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    GtkTreePath *path;
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(state->results_tag_tree_view), (int)x, (int)y, &path, NULL, NULL, NULL)) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(state->results_tag_tree_view));
        gtk_tree_selection_select_path(selection, path);
        
        GtkTreeModel *model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            int tag_id = -1;
            gtk_tree_model_get(model, &iter, 0, &tag_id, -1);
            state->results_context_tag_id = tag_id;
            
            GdkRectangle rect = { (int)x, (int)y, 1, 1 };
            gtk_popover_set_pointing_to(GTK_POPOVER(state->results_tag_popover), &rect);
            gtk_popover_popup(GTK_POPOVER(state->results_tag_popover));
        }
        gtk_tree_path_free(path);
    }
}

static void on_edit_tag_save_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "dialog"));
    CualiAppState *state = (CualiAppState *)g_object_get_data(G_OBJECT(btn), "state");
    GtkEntry *name_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(btn), "name_entry"));
    GtkEntry *desc_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(btn), "desc_entry"));
    
    const char *new_name = gtk_editable_get_text(GTK_EDITABLE(name_entry));
    const char *new_desc = gtk_editable_get_text(GTK_EDITABLE(desc_entry));
    
    if (strlen(new_name) > 0) {
        db_tag_update(state->results_context_tag_id, new_name, new_desc);
        refresh_tags(state);
        state->results_dirty = TRUE;
        refresh_results(state);
    }
    
    adw_dialog_close(ADW_DIALOG(dialog));
}

static void on_results_tag_edit_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    gtk_popover_popdown(GTK_POPOVER(state->results_tag_popover));
    
    char *path = NULL, *desc = NULL;
    if (!db_tag_get_info(state->results_context_tag_id, &path, &desc, NULL)) return;
    
    GtkWidget *dialog = GTK_WIDGET(adw_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), "Editar Etiqueta");
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 400);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    
    GtkWidget *name_label = gtk_label_new("Ruta / Nombre:");
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), name_label);
    
    GtkWidget *name_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(name_entry), path ? path : "");
    gtk_box_append(GTK_BOX(box), name_entry);
    
    GtkWidget *desc_label = gtk_label_new("Descripción:");
    gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), desc_label);
    
    GtkWidget *desc_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(desc_entry), desc ? desc : "");
    gtk_box_append(GTK_BOX(box), desc_entry);
    
    GtkWidget *save_btn = gtk_button_new_with_label("Guardar");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    gtk_widget_set_halign(save_btn, GTK_ALIGN_END);
    gtk_widget_set_margin_top(save_btn, 12);
    gtk_box_append(GTK_BOX(box), save_btn);
    
    g_object_set_data(G_OBJECT(save_btn), "dialog", dialog);
    g_object_set_data(G_OBJECT(save_btn), "state", state);
    g_object_set_data(G_OBJECT(save_btn), "name_entry", name_entry);
    g_object_set_data(G_OBJECT(save_btn), "desc_entry", desc_entry);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_edit_tag_save_clicked), NULL);
    
    adw_dialog_set_child(ADW_DIALOG(dialog), box);
    adw_dialog_present(ADW_DIALOG(dialog), state->window);
    
    g_free(path);
    g_free(desc);
}

static void on_merge_tag_save_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "dialog"));
    CualiAppState *state = (CualiAppState *)g_object_get_data(G_OBJECT(btn), "state");
    GtkDropDown *dropdown = GTK_DROP_DOWN(g_object_get_data(G_OBJECT(btn), "dropdown"));
    
    guint selected_pos = gtk_drop_down_get_selected(dropdown);
    GListModel *model = gtk_drop_down_get_model(dropdown);
    if (selected_pos == GTK_INVALID_LIST_POSITION) return;
    
    GtkStringObject *strobj = GTK_STRING_OBJECT(g_list_model_get_item(model, selected_pos));
    const char *sel_str = gtk_string_object_get_string(strobj);
    
    int target_id = atoi(sel_str); 
    
    if (target_id > 0 && target_id != state->results_context_tag_id) {
        if (db_tag_merge(state->results_context_tag_id, target_id)) {
            refresh_tags(state);
            state->results_dirty = TRUE;
            refresh_results(state);
        } else {
            adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                         adw_toast_new ("Error al fusionar la etiqueta"));
        }
    }
    
    g_object_unref(strobj);
    adw_dialog_close(ADW_DIALOG(dialog));
}

static void on_results_tag_merge_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    gtk_popover_popdown(GTK_POPOVER(state->results_tag_popover));
    
    GtkWidget *dialog = GTK_WIDGET(adw_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), "Fusionar Etiqueta");
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 400);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    
    GtkWidget *lbl = gtk_label_new("Selecciona la etiqueta destino:");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), lbl);
    
    GtkStringList *strlist = gtk_string_list_new(NULL);
    sqlite3_stmt *stmt = db_tags_get_all(state->current_project_id);
    if (stmt) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            if (id == state->results_context_tag_id) continue;
            const char *path = (const char *)sqlite3_column_text(stmt, 1);
            char *encoded = g_strdup_printf("%d: %s", id, path);
            gtk_string_list_append(strlist, encoded);
            g_free(encoded);
        }
        sqlite3_finalize(stmt);
    }
    
    GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(strlist), NULL);
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(dropdown), TRUE);
    gtk_box_append(GTK_BOX(box), dropdown);
    
    GtkWidget *save_btn = gtk_button_new_with_label("Fusionar");
    gtk_widget_add_css_class(save_btn, "destructive-action");
    gtk_widget_set_halign(save_btn, GTK_ALIGN_END);
    gtk_widget_set_margin_top(save_btn, 12);
    gtk_box_append(GTK_BOX(box), save_btn);
    
    g_object_set_data(G_OBJECT(save_btn), "dialog", dialog);
    g_object_set_data(G_OBJECT(save_btn), "state", state);
    g_object_set_data(G_OBJECT(save_btn), "dropdown", dropdown);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_merge_tag_save_clicked), NULL);
    
    adw_dialog_set_child(ADW_DIALOG(dialog), box);
    adw_dialog_present(ADW_DIALOG(dialog), state->window);
}

static void
on_results_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    state->results_dirty = TRUE;
    refresh_results (state);
}


static void
on_tag_search_changed (GtkEditable *editable, gpointer user_data)
{
    GtkListBox *list_box = GTK_LIST_BOX (user_data);
    gtk_list_box_invalidate_filter (list_box);
}

static void
on_tag_check_toggled (GtkCheckButton *check, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int highlight_id = GPOINTER_TO_INT (args[1]);
    int tag_id = GPOINTER_TO_INT (args[2]);
    
    if (gtk_check_button_get_active (check)) {
        db_highlight_link_tag (highlight_id, tag_id);
    } else {
        db_highlight_unlink_tag (highlight_id, tag_id);
    }
    refresh_results (state);
}

static gboolean
tag_flow_box_filter_func (GtkFlowBoxChild *child, gpointer user_data)
{
    const char *query = (const char *)user_data;
    if (!query || *query == '\0') return TRUE;
    
    const char *path = g_object_get_data (G_OBJECT (child), "tag-path");
    if (!path) return TRUE;
    
    char *lower_label = g_utf8_strdown (path, -1);
    char *lower_query = g_utf8_strdown (query, -1);
    gboolean match = (strstr (lower_label, lower_query) != NULL);
    
    g_free (lower_label);
    g_free (lower_query);
    return match;
}

static void
on_dialog_tag_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
    GtkFlowBox *flow_box = GTK_FLOW_BOX (user_data);
    const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
    gtk_flow_box_set_filter_func (flow_box, tag_flow_box_filter_func, g_strdup (text), g_free);
}

static void
populate_tag_dialog_list (CualiAppState *state, int highlight_id, GtkWidget *flow_box)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (flow_box)) != NULL)
        gtk_flow_box_remove (GTK_FLOW_BOX (flow_box), child);

    GHashTable *assigned_ids = g_hash_table_new (g_direct_hash, g_direct_equal);
    sqlite3_stmt *stmt_check = db_tags_get_for_highlight (highlight_id);
    if (stmt_check) {
        while (sqlite3_step (stmt_check) == SQLITE_ROW) {
            int tag_id = sqlite3_column_int (stmt_check, 0);
            g_hash_table_add (assigned_ids, GINT_TO_POINTER (tag_id));
        }
        sqlite3_finalize (stmt_check);
    }

    sqlite3_stmt *stmt_all = db_tags_get_all (state->current_project_id);
    if (stmt_all) {
        while (sqlite3_step (stmt_all) == SQLITE_ROW) {
            int tag_id = sqlite3_column_int (stmt_all, 0);
            const char *path = (const char *)sqlite3_column_text (stmt_all, 1);
            const char *color = (const char *)sqlite3_column_text (stmt_all, 2);
            if (!color) color = "#77767b";
            
            GtkWidget *check = gtk_check_button_new ();
            gtk_widget_set_valign (check, GTK_ALIGN_CENTER);
            
            GtkWidget *check_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            
            GtkWidget *dot = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_size_request (dot, 10, 10);
            gtk_widget_set_valign (dot, GTK_ALIGN_CENTER);
            char *dot_css = g_strdup_printf ("box { background-color: %s; border-radius: 5px; }", color);
            GtkCssProvider *provider = gtk_css_provider_new ();
            gtk_css_provider_load_from_string (provider, dot_css);
            gtk_style_context_add_provider (gtk_widget_get_style_context (dot),
                                            GTK_STYLE_PROVIDER (provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            g_object_unref (provider);
            g_free (dot_css);
            gtk_box_append (GTK_BOX (check_hbox), dot);
            
            GtkWidget *lbl = gtk_label_new (path);
            gtk_widget_set_valign (lbl, GTK_ALIGN_CENTER);
            gtk_box_append (GTK_BOX (check_hbox), lbl);
            
            gtk_check_button_set_child (GTK_CHECK_BUTTON (check), check_hbox);
            
            if (g_hash_table_contains (assigned_ids, GINT_TO_POINTER (tag_id))) {
                gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
            }
            
            gpointer *args = g_new (gpointer, 3);
            args[0] = state;
            args[1] = GINT_TO_POINTER (highlight_id);
            args[2] = GINT_TO_POINTER (tag_id);
            g_signal_connect_data (check, "toggled", G_CALLBACK (on_tag_check_toggled), args, (GClosureNotify)g_free, 0);
            
            GtkWidget *flow_child = gtk_flow_box_child_new ();
            gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (flow_child), check);
            g_object_set_data_full (G_OBJECT (flow_child), "tag-path", g_strdup (path), g_free);
            
            gtk_flow_box_append (GTK_FLOW_BOX (flow_box), flow_child);
        }
        sqlite3_finalize (stmt_all);
    }
    g_hash_table_destroy (assigned_ids);
}

static void
on_dialog_new_tag_activated (GtkEntry *entry, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int highlight_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (entry), "highlight_id"));
    const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
    
    if (text && *text != '\0') {
        int count = 0;
        sqlite3_stmt *stmt = db_tags_get_stats(state->current_project_id);
        if (stmt) {
            while (sqlite3_step(stmt) == SQLITE_ROW) count++;
            sqlite3_finalize(stmt);
        }
        const char *color = TAG_COLORS[count % TAG_COLORS_COUNT];
        
        int tag_id = db_tag_add (state->current_project_id, text, "", color);
        if (highlight_id > 0 && tag_id > 0) {
            db_highlight_link_tag (highlight_id, tag_id);
        }
        gtk_editable_set_text (GTK_EDITABLE (entry), "");
        
        GtkWidget *flow_box = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "flow_box"));
        populate_tag_dialog_list (state, highlight_id, flow_box);
        refresh_results (state);
        refresh_tags (state);
    }
}


/* ---- Tag edit dialog ---- */

static void
on_tag_edit_save_clicked (GtkButton *btn, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int tag_id = GPOINTER_TO_INT (args[1]);
    GtkWidget *name_entry = GTK_WIDGET (args[2]);
    GtkWidget *desc_entry = GTK_WIDGET (args[3]);
    GtkWidget *color_btn = GTK_WIDGET (args[4]);
    GtkWidget *dialog = GTK_WIDGET (args[5]);

    const char *name = gtk_editable_get_text (GTK_EDITABLE (name_entry));
    const char *desc = gtk_editable_get_text (GTK_EDITABLE (desc_entry));

    if (name && *name) {
        db_tag_update (tag_id, name, desc ? desc : "");

        const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (color_btn));
        char *hex = gdk_rgba_to_string (rgba);
        db_tag_update_color (tag_id, hex);
        g_free (hex);

        refresh_tags (state);
        refresh_results (state);
    }
    adw_dialog_force_close (ADW_DIALOG (dialog));
}

static void
on_tag_edit_delete_clicked (GtkButton *btn, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int tag_id = GPOINTER_TO_INT (args[1]);
    GtkWidget *dialog = GTK_WIDGET (args[2]);

    if (db_tag_delete (tag_id)) {
        refresh_tags (state);
        refresh_results (state);
    }
    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
show_tag_edit_dialog (CualiAppState *state, int tag_id)
{
    char *cur_path = NULL, *cur_desc = NULL, *cur_color = NULL;
    db_tag_get_info (tag_id, &cur_path, &cur_desc, &cur_color);

    GtkWidget *dialog = GTK_WIDGET (adw_dialog_new ());
    adw_dialog_set_title (ADW_DIALOG (dialog), "Edit tag");
    adw_dialog_set_content_width (ADW_DIALOG (dialog), 420);

    GtkWidget *toolbar_view = adw_toolbar_view_new ();
    GtkWidget *header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);

    GtkWidget *content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start (content_box, 20);
    gtk_widget_set_margin_end (content_box, 20);
    gtk_widget_set_margin_top (content_box, 20);
    gtk_widget_set_margin_bottom (content_box, 20);

    GtkWidget *group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "");

    GtkWidget *name_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Name");
    if (cur_path) gtk_editable_set_text (GTK_EDITABLE (name_row), cur_path);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), name_row);

    GtkWidget *desc_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (desc_row), "Description");
    if (cur_desc) gtk_editable_set_text (GTK_EDITABLE (desc_row), cur_desc);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), desc_row);

    gtk_box_append (GTK_BOX (content_box), group);

    /* Color picker */
    GtkWidget *color_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (color_box, 8);
    gtk_widget_set_margin_start (color_box, 6);
    GtkWidget *color_label = gtk_label_new ("Color:");
    gtk_box_append (GTK_BOX (color_box), color_label);

    GdkRGBA cur_rgba;
    gdk_rgba_parse (&cur_rgba, cur_color && *cur_color ? cur_color : "#3584e4");
    GtkColorDialog *color_dialog = gtk_color_dialog_new ();
    gtk_color_dialog_set_with_alpha (color_dialog, FALSE);
    GtkWidget *color_btn = gtk_color_dialog_button_new (color_dialog);
    gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (color_btn), &cur_rgba);
    gtk_box_append (GTK_BOX (color_box), color_btn);
    gtk_widget_set_halign (color_box, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (content_box), color_box);

    GtkWidget *action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(action_box, GTK_ALIGN_END);

    GtkWidget *delete_btn = gtk_button_new_with_label ("Delete");
    gtk_widget_add_css_class (delete_btn, "destructive-action");
    gtk_widget_add_css_class (delete_btn, "pill");
    gtk_widget_set_tooltip_text (delete_btn, "Delete this tag");
    gtk_box_append (GTK_BOX (action_box), delete_btn);

    GtkWidget *save_btn = gtk_button_new_with_label ("Save");
    gtk_widget_add_css_class (save_btn, "suggested-action");
    gtk_widget_add_css_class (save_btn, "pill");
    gtk_widget_set_tooltip_text (save_btn, "Save tag information");
    gtk_box_append (GTK_BOX (action_box), save_btn);

    gtk_box_append (GTK_BOX (content_box), action_box);

    gpointer *del_args = g_new (gpointer, 3);
    del_args[0] = state;
    del_args[1] = GINT_TO_POINTER (tag_id);
    del_args[2] = dialog;
    g_signal_connect_data (delete_btn, "clicked",
                           G_CALLBACK (on_tag_edit_delete_clicked),
                           del_args, (GClosureNotify)g_free, 0);

    gpointer *args = g_new (gpointer, 6);
    args[0] = state;
    args[1] = GINT_TO_POINTER (tag_id);
    args[2] = name_row;
    args[3] = desc_row;
    args[4] = color_btn;
    args[5] = dialog;
    g_signal_connect_data (save_btn, "clicked",
                           G_CALLBACK (on_tag_edit_save_clicked),
                           args, (GClosureNotify)g_free, 0);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), content_box);
    adw_dialog_set_child (ADW_DIALOG (dialog), toolbar_view);
    adw_dialog_present (ADW_DIALOG (dialog), state->window);

    g_free (cur_path);
    g_free (cur_desc);
    g_free (cur_color);
}

static void
on_tag_edit_btn_clicked (GtkButton *btn, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int tag_id = GPOINTER_TO_INT (args[1]);
    show_tag_edit_dialog (state, tag_id);
}

static void on_unified_dialog_closed (AdwDialog *dialog, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int hl_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "hl-id"));
    GtkTextView *memo_view = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(dialog), "memo-view"));

    if (hl_id > 0 && memo_view) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(memo_view);
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds(buf, &s, &e);
        char *memo = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
        db_highlight_set_memo(hl_id, memo ? memo : "");
        g_free(memo);
    }
    
    gtk_widget_grab_focus (state->text_view);
}

static void on_unified_delete_clicked (GtkButton *btn, gpointer user_data)
{
    GtkWidget *dialog = GTK_WIDGET(user_data);
    CualiAppState *state = (CualiAppState *)g_object_get_data(G_OBJECT(dialog), "app-state");
    int hl_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "hl-id"));
    
    if (hl_id > 0) {
        db_highlight_delete(hl_id);
    }
    adw_dialog_force_close(ADW_DIALOG(dialog));
    
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->doc_list));
    if (row) {
        const char *name = g_object_get_data(G_OBJECT(row), "doc-name");
        char *contents = db_document_get_contents(state->current_document_id);
        load_document(state, state->current_document_id, name, contents);
        g_free(contents);
    }
    refresh_results(state);
    refresh_tags(state);
}

static void
show_tag_dialog (CualiAppState *state, int highlight_id)
{
    GtkWidget *dialog = GTK_WIDGET (adw_dialog_new ());
    adw_dialog_set_title (ADW_DIALOG (dialog), "Etiquetas y Notas");
    adw_dialog_set_content_width (ADW_DIALOG (dialog), 640);
    adw_dialog_set_content_height (ADW_DIALOG (dialog), 580);

    GtkWidget *toolbar_view = adw_toolbar_view_new ();
    GtkWidget *header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);
    
    GtkWidget *content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (content_box, 16);
    gtk_widget_set_margin_end (content_box, 16);
    gtk_widget_set_margin_top (content_box, 16);
    gtk_widget_set_margin_bottom (content_box, 16);

    GtkWidget *entry_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append (GTK_BOX (content_box), entry_box);

    GtkWidget *search_entry = gtk_search_entry_new ();
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (search_entry), "Buscar etiqueta…");
    gtk_widget_set_hexpand (search_entry, TRUE);
    gtk_box_append (GTK_BOX (entry_box), search_entry);

    GtkWidget *tag_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (tag_entry), "Nueva etiqueta…");
    gtk_widget_set_hexpand (tag_entry, TRUE);
    gtk_box_append (GTK_BOX (entry_box), tag_entry);

    GtkWidget *scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_box_append (GTK_BOX (content_box), scroll);

    GtkWidget *flow_box = gtk_flow_box_new ();
    gtk_widget_set_valign (flow_box, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (flow_box), 15);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (flow_box), 12);
    gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (flow_box), 8);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), flow_box);

    /* Memo */
    gtk_box_append(GTK_BOX(content_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *memo_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 80);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 160);
    
    GtkWidget *memo_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(memo_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(memo_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(memo_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(memo_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(memo_view), 8);
    char *memo_text = NULL;
    if (highlight_id > 0) db_highlight_get_memo(highlight_id, &memo_text);
    if (memo_text) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(memo_view)), memo_text, -1);
        g_free(memo_text);
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memo_scroll), memo_view);
    gtk_box_append(GTK_BOX(content_box), memo_scroll);

    /* Delete Button */
    gtk_box_append(GTK_BOX(content_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    GtkWidget *del_btn = gtk_button_new_with_label("Delete highlight");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    gtk_widget_set_tooltip_text(del_btn, "Remove this highlight and its metadata");
    gtk_box_append(GTK_BOX(content_box), del_btn);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_unified_delete_clicked), dialog);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), content_box);

    g_object_set_data (G_OBJECT (tag_entry), "highlight_id", GINT_TO_POINTER (highlight_id));
    g_object_set_data (G_OBJECT (tag_entry), "flow_box", flow_box);
    g_signal_connect (tag_entry, "activate", G_CALLBACK (on_dialog_new_tag_activated), state);

    g_signal_connect (search_entry, "search-changed", G_CALLBACK (on_dialog_tag_search_changed), flow_box);

    populate_tag_dialog_list (state, highlight_id, flow_box);

    g_object_set_data (G_OBJECT (dialog), "app-state", state);
    g_object_set_data (G_OBJECT (dialog), "hl-id", GINT_TO_POINTER (highlight_id));
    g_object_set_data (G_OBJECT (dialog), "memo-view", memo_view);
    g_signal_connect (dialog, "closed", G_CALLBACK (on_unified_dialog_closed), state);

    adw_dialog_set_child (ADW_DIALOG (dialog), toolbar_view);
    adw_dialog_present (ADW_DIALOG (dialog), state->window);
}

static void on_popover_delete_clicked(GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->active_highlight_id <= 0) return;
    
    db_highlight_delete(state->active_highlight_id);
    gtk_popover_popdown(GTK_POPOVER(state->highlight_popover));
    
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->doc_list));
    if (row) {
        const char *name = g_object_get_data(G_OBJECT(row), "doc-name");
        char *contents = db_document_get_contents(state->current_document_id);
        load_document(state, state->current_document_id, name, contents);
        g_free(contents);
    }
    refresh_results(state);
    refresh_tags(state);
}

static void on_popover_tag_toggled(GtkCheckButton *check, gpointer user_data)
{
    CualiAppState *state = g_object_get_data(G_OBJECT(check), "app-state");
    int tag_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(check), "tag-id"));
    bool active = gtk_check_button_get_active(check);

    if (state->active_highlight_id > 0) {
        if (active) db_highlight_link_tag(state->active_highlight_id, tag_id);
        else        db_highlight_unlink_tag(state->active_highlight_id, tag_id);
        mark_results_dirty (state);
        refresh_results(state);
        refresh_tags(state);
    } else if (state->active_highlight_id == -1 && active) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
        GtkTextIter start_iter, end_iter;
        
        GtkTextIter buf_start, buf_end;
        gtk_text_buffer_get_bounds(buffer, &buf_start, &buf_end);
        char *full_text = gtk_text_buffer_get_text(buffer, &buf_start, &buf_end, FALSE);
        
        int len = full_text ? strlen(full_text) : 0;
        int p_start = state->pending_start;
        int p_end = state->pending_end;
        if (p_start < 0) p_start = 0;
        if (p_end < p_start) p_end = p_start;
        if (p_start > len) p_start = len;
        if (p_end > len) p_end = len;
        
        int vis_start = full_text ? g_utf8_pointer_to_offset(full_text, full_text + p_start) : 0;
        int vis_end   = full_text ? g_utf8_pointer_to_offset(full_text, full_text + p_end) : 0;
        g_free(full_text);
        
        gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, vis_start);
        gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, vis_end);
        
        char *snippet = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);
        int hl_id = db_highlight_add(state->current_document_id, state->pending_start, state->pending_end, snippet);
        g_free(snippet);
        
        if (hl_id > 0) {
            db_highlight_link_tag(hl_id, tag_id);
            mark_results_dirty (state);
            state->cached_highlight_count++;
            apply_highlight_tag(buffer, hl_id, &start_iter, &end_iter, NULL);
            state->active_highlight_id = hl_id;
            refresh_results(state);
            refresh_tags(state);
            update_status_bar (state);
        }
    }
}

static void populate_popover_tags(CualiAppState *state)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->popover_tag_list))) {
        gtk_list_box_remove(GTK_LIST_BOX(state->popover_tag_list), child);
    }

    GHashTable *assigned_ids = NULL;
    if (state->active_highlight_id > 0) {
        assigned_ids = g_hash_table_new(g_direct_hash, g_direct_equal);
        sqlite3_stmt *stmt = db_tags_get_for_highlight(state->active_highlight_id);
        if (stmt) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tag_id = sqlite3_column_int(stmt, 0);
                g_hash_table_add(assigned_ids, GINT_TO_POINTER(tag_id));
            }
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_stmt *stmt_all = db_tags_get_all(state->current_project_id);
    if (stmt_all) {
        while (sqlite3_step(stmt_all) == SQLITE_ROW) {
            int tag_id = sqlite3_column_int(stmt_all, 0);
            const char *path = (const char *)sqlite3_column_text(stmt_all, 1);
            
            GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
            GtkWidget *check = gtk_check_button_new_with_label(path);
            
            bool assigned = (state->active_highlight_id > 0) &&
                            assigned_ids && g_hash_table_contains(assigned_ids, GINT_TO_POINTER(tag_id));
            
            gtk_check_button_set_active(GTK_CHECK_BUTTON(check), assigned);
            g_object_set_data(G_OBJECT(check), "tag-id", GINT_TO_POINTER(tag_id));
            g_object_set_data(G_OBJECT(check), "app-state", state);
            g_signal_connect(check, "toggled", G_CALLBACK(on_popover_tag_toggled), NULL);
            
            gtk_list_box_row_set_child(row, check);
            gtk_list_box_append(GTK_LIST_BOX(state->popover_tag_list), GTK_WIDGET(row));
        }
        sqlite3_finalize(stmt_all);
    }

    if (assigned_ids) g_hash_table_destroy(assigned_ids);
}

static void on_highlight_dialog_closed (AdwDialog *dialog, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    
    /* Save memo if active */
    if (state->active_highlight_id > 0) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->popover_memo_view));
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds(buf, &s, &e);
        char *memo = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
        db_highlight_set_memo(state->active_highlight_id, memo ? memo : "");
        g_free(memo);
    }
    
}

static void show_highlight_dialog_at(CualiAppState *state, int highlight_id)
{
    state->active_highlight_id = highlight_id;
    populate_popover_tags(state);
    gtk_widget_set_visible(state->popover_delete_btn, highlight_id > 0);

    /* Load memo */
    char *memo = NULL;
    if (highlight_id > 0) db_highlight_get_memo(highlight_id, &memo);
    GtkTextBuffer *mbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->popover_memo_view));
    gtk_text_buffer_set_text(mbuf, memo ? memo : "", -1);
    g_free(memo);
    gtk_widget_set_visible(state->popover_memo_view, highlight_id > 0);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextIter start_iter, end_iter;
    bool bounds_valid = false;

    if (highlight_id > 0) {
        int start_offset, end_offset;
        if (db_highlight_get_offsets(highlight_id, &start_offset, &end_offset)) {
            GtkTextIter buf_start, buf_end;
            gtk_text_buffer_get_bounds(buffer, &buf_start, &buf_end);
            char *full_text = gtk_text_buffer_get_text(buffer, &buf_start, &buf_end, FALSE);
            if (full_text) {
                int len = strlen(full_text);
                if (start_offset > len) start_offset = len;
                if (end_offset > len) end_offset = len;
                
                const char* start_ptr = full_text + start_offset;
                const char* end_ptr = full_text + end_offset;

                int char_start = g_utf8_pointer_to_offset(full_text, start_ptr);
                int char_end = g_utf8_pointer_to_offset(full_text, end_ptr);
                g_free(full_text);

                gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, char_start);
                gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, char_end);
                bounds_valid = true;
            }
        }
    } else { // New highlight from selection
        bounds_valid = gtk_text_buffer_get_selection_bounds(buffer, &start_iter, &end_iter);
    }

    if (!bounds_valid) return;

    GdkRectangle rect;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(state->text_view), &start_iter, &rect);
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(state->text_view), GTK_TEXT_WINDOW_WIDGET, rect.x, rect.y, &rect.x, &rect.y);
    gtk_popover_set_pointing_to(GTK_POPOVER(state->highlight_popover), &rect);
    gtk_popover_popup(GTK_POPOVER(state->highlight_popover));
}

static void build_highlight_dialog(CualiAppState *state)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(box, 360, -1);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 300);
    gtk_scrolled_window_set_max_content_width(GTK_SCROLLED_WINDOW(scroll), 320);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    state->popover_tag_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->popover_tag_list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->popover_tag_list);
    gtk_box_append(GTK_BOX(box), scroll);
    
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);
    
    state->popover_delete_btn = gtk_button_new_with_label("Delete highlight");
    gtk_widget_add_css_class(state->popover_delete_btn, "destructive-action");
    gtk_widget_set_tooltip_text(state->popover_delete_btn, "Remove this highlight");
    gtk_widget_set_margin_start(state->popover_delete_btn, 8);
    gtk_widget_set_margin_end(state->popover_delete_btn, 8);
    gtk_widget_set_margin_top(state->popover_delete_btn, 8);
    gtk_widget_set_margin_bottom(state->popover_delete_btn, 8);
    g_signal_connect(state->popover_delete_btn, "clicked", G_CALLBACK(on_popover_delete_clicked), state);
    gtk_box_append(GTK_BOX(box), state->popover_delete_btn);

    /* ── Memo del investigador ── */
    GtkWidget *memo_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), memo_sep);

    GtkWidget *memo_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 80);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 160);
    gtk_widget_set_margin_start(memo_scroll, 8);
    gtk_widget_set_margin_end(memo_scroll, 8);
    gtk_widget_set_margin_bottom(memo_scroll, 8);

    state->popover_memo_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->popover_memo_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(state->popover_memo_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(state->popover_memo_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(state->popover_memo_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(state->popover_memo_view), 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memo_scroll), state->popover_memo_view);
    gtk_box_append(GTK_BOX(box), memo_scroll);

    gtk_popover_set_child(GTK_POPOVER(state->highlight_popover), box);
    g_signal_connect(state->highlight_popover, "closed",
                     G_CALLBACK(on_highlight_dialog_closed), state);
}

static void
on_selector_row_activated (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!row) return;
    
    int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "hl-id"));
    adw_dialog_force_close (ADW_DIALOG (state->highlight_selector));
    
    if (hl_id > 0) {
        show_highlight_dialog_at (state, hl_id);
    }
}

static void
show_highlight_selector_dialog (CualiAppState *state, GSList *hl_ids, int x, int y)
{
    GtkWidget *toolbar_view = adw_dialog_get_child (ADW_DIALOG (state->highlight_selector));
    GtkWidget *scroll = adw_toolbar_view_get_content (ADW_TOOLBAR_VIEW (toolbar_view));
    if (!GTK_IS_SCROLLED_WINDOW (scroll)) return;
    GtkWidget *list = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scroll));
    if (!GTK_IS_LIST_BOX (list)) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (list)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (list), child);
    
    for (GSList *l = hl_ids; l; l = l->next) {
        int hl_id = GPOINTER_TO_INT (l->data);
        
        char *tag_path = NULL;
        char *tag_color = NULL;
        sqlite3_stmt *stmt = db_tags_get_for_highlight (hl_id);
        if (stmt) {
            if (sqlite3_step (stmt) == SQLITE_ROW) {
                tag_path = g_strdup ((const char *)sqlite3_column_text (stmt, 1));
                tag_color = g_strdup ((const char *)sqlite3_column_text (stmt, 2));
            }
            sqlite3_finalize (stmt);
        }
        if (!tag_path) tag_path = g_strdup ("Unknown");
        if (!tag_color) tag_color = g_strdup ("#77767b");
        
        GtkWidget *row = gtk_list_box_row_new ();
        GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start (hbox, 10);
        gtk_widget_set_margin_end (hbox, 10);
        gtk_widget_set_margin_top (hbox, 6);
        gtk_widget_set_margin_bottom (hbox, 6);
        
        GtkWidget *dot = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_size_request (dot, 12, 12);
        char *css = g_strdup_printf ("box { background-color: %s; border-radius: 6px; }", tag_color);
        GtkCssProvider *provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_string (provider, css);
        gtk_style_context_add_provider (gtk_widget_get_style_context (dot),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
        g_free (css);
        gtk_widget_set_valign (dot, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (hbox), dot);
        
        GtkWidget *label = gtk_label_new (tag_path);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_box_append (GTK_BOX (hbox), label);
        
        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), hbox);
        g_object_set_data (G_OBJECT (row), "hl-id", GINT_TO_POINTER (hl_id));
        gtk_list_box_append (GTK_LIST_BOX (list), row);
        
        g_free (tag_path);
        g_free (tag_color);
    }
    
    adw_dialog_present (ADW_DIALOG (state->highlight_selector), state->window);
}

static void
build_highlight_selector_dialog (CualiAppState *state)
{
    adw_dialog_set_title (ADW_DIALOG (state->highlight_selector), "Select Highlight");
    adw_dialog_set_content_width (ADW_DIALOG (state->highlight_selector), 300);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header_bar = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_bar);

    GtkWidget *scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), 250);
    gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
    
    GtkWidget *list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list);
    
    g_signal_connect (list, "row-activated", G_CALLBACK (on_selector_row_activated), state);
    
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), scroll);
    adw_dialog_set_child (ADW_DIALOG (state->highlight_selector), toolbar_view);
}

static void
on_text_view_realized (GtkWidget *widget, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    state->highlight_popover = GTK_WIDGET(adw_dialog_new());
    state->highlight_selector = GTK_WIDGET(adw_dialog_new());
    build_highlight_dialog(state);
    build_highlight_selector_dialog(state);
}

static void show_highlight_selector_dialog(CualiAppState *state, GSList *hl_ids, int x, int y);
static void create_highlight_and_show_tags(CualiAppState *state);
static void update_vim_cursor(CualiAppState *state);
static void update_vim_status(CualiAppState *state);


static void
on_text_view_clicked (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing) return;
    
    GtkTextView *view = GTK_TEXT_VIEW (state->text_view);
    GtkTextIter iter;
    int buffer_x, buffer_y;
    
    gtk_text_view_window_to_buffer_coords (view, GTK_TEXT_WINDOW_WIDGET, (int)x, (int)y, &buffer_x, &buffer_y);
    gtk_text_view_get_iter_at_location (view, &iter, buffer_x, buffer_y);
    gtk_text_buffer_place_cursor (gtk_text_view_get_buffer (view), &iter);
    
    if (state->vim_enabled && state->vim_mode == VIM_NORMAL) {
        update_vim_cursor(state);
    }
    
    GSList *tags = gtk_text_iter_get_tags (&iter);
    GSList *hl_ids = NULL;
    int count = 0;
    for (GSList *l = tags; l; l = l->next) {
        int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "highlight-id"));
        if (hl_id > 0) {
            hl_ids = g_slist_append(hl_ids, GINT_TO_POINTER(hl_id));
            count++;
        }
    }
    g_slist_free (tags);
    
    if (count == 0) return;
    
    gtk_gesture_set_state (GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    if (count == 1) {
        show_tag_dialog(state, GPOINTER_TO_INT(hl_ids->data));
    } else {
        show_highlight_selector_dialog(state, hl_ids, (int)x, (int)y);
    }

    g_slist_free (hl_ids);
}

static void
on_text_view_released (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing) return;
    
    GtkTextView *view = GTK_TEXT_VIEW (state->text_view);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
    
    int buffer_x, buffer_y;
    gtk_text_view_window_to_buffer_coords (view, GTK_TEXT_WINDOW_WIDGET, (int)x, (int)y, &buffer_x, &buffer_y);
    GtkTextIter click_iter;
    gtk_text_view_get_iter_at_location (view, &click_iter, buffer_x, buffer_y);
    
    GSList *tags = gtk_text_iter_get_tags (&click_iter);
    gboolean clicked_on_highlight = FALSE;
    for (GSList *l = tags; l; l = l->next) {
        int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "highlight-id"));
        if (hl_id > 0) {
            clicked_on_highlight = TRUE;
            break;
        }
    }
    g_slist_free (tags);
    
    if (clicked_on_highlight) return;
    
    GtkTextIter start, end;
    
    if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end)) return;
    if (gtk_text_iter_equal(&start, &end)) return;
    
    gtk_text_buffer_place_cursor (buffer, &start);

    GtkTextIter doc_start;
    gtk_text_buffer_get_start_iter(buffer, &doc_start);
    
    char *text_before_start = gtk_text_buffer_get_text(buffer, &doc_start, &start, FALSE);
    char *text_before_end = gtk_text_buffer_get_text(buffer, &doc_start, &end, FALSE);
    
    state->pending_start = text_before_start ? strlen(text_before_start) : 0;
    state->pending_end = text_before_end ? strlen(text_before_end) : 0;
    
    g_free(text_before_start);
    g_free(text_before_end);
    create_highlight_and_show_tags(state);
}

static void
apply_highlight_tag (GtkTextBuffer *buffer, int hl_id, GtkTextIter *start, GtkTextIter *end, const char *color)
{
    char *hex_color = NULL;
    
    if (color) {
        hex_color = g_strdup (color);
    } else {
        sqlite3_stmt *stmt = db_tags_get_for_highlight(hl_id);
        if (stmt) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *db_color = (const char *)sqlite3_column_text(stmt, 2);
                if (db_color && db_color[0] != '\0') {
                    hex_color = g_strdup(db_color);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    
    GdkRGBA rgba;
    if (hex_color && gdk_rgba_parse(&rgba, hex_color)) {
        rgba.alpha = 0.3;
    } else {
        gdk_rgba_parse(&rgba, "#f9f06b");
        rgba.alpha = 0.3;
    }
    
    GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, NULL, 
                                                 "background-rgba", &rgba, 
                                                 NULL);
    g_object_set_data (G_OBJECT (tag), "highlight-id", GINT_TO_POINTER (hl_id));
    gtk_text_buffer_apply_tag (buffer, tag, start, end);
    
    g_free (hex_color);
}

static void
save_document (CualiAppState *state)
{
    if (state->current_document_id <= 0 || !state->is_editing) return;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    
    db_document_update_contents (state->current_document_id, text);
    g_free (text);
    
    state->has_unsaved_changes = false;
    update_save_indicator (state, FALSE);
}

static void
on_save_clicked (GtkButton *btn, gpointer user_data)
{
    save_document ((CualiAppState *)user_data);
}

static void
on_buffer_changed (GtkTextBuffer *buffer, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing) {
        state->has_unsaved_changes = true;
        update_save_indicator (state, TRUE);
    }
}

/* ── Busqueda en documento (Ctrl+F) ── */

static void
search_clear_matches (CualiAppState *state)
{
    if (!state->search_match_tag) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    gtk_text_buffer_remove_tag (buffer, state->search_match_tag, &start, &end);
    if (state->search_current_tag)
        gtk_text_buffer_remove_tag (buffer, state->search_current_tag, &start, &end);
    state->search_match_count = 0;
    state->search_current_match = 0;
}

static void
search_find (CualiAppState *state, bool forward)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
    const char *query = gtk_editable_get_text (GTK_EDITABLE (state->search_entry));

    search_clear_matches (state);
    if (!query || *query == '\0') return;

    GtkTextIter bound_start, bound_end;
    gtk_text_buffer_get_bounds (buffer, &bound_start, &bound_end);
    char *full = gtk_text_buffer_get_text (buffer, &bound_start, &bound_end, FALSE);
    if (!full) return;

    char *lower_full = g_utf8_strdown (full, -1);
    char *lower_q = g_utf8_strdown (query, -1);
    int q_len = strlen (lower_q);

    /* Build list of match character offsets */
    int *starts = NULL;
    int *ends = NULL;
    int count = 0;

    char *pos = lower_full;
    while ((pos = strstr (pos, lower_q)) != NULL) {
        int byte_off = pos - lower_full;
        int cstart = g_utf8_pointer_to_offset (lower_full, lower_full + byte_off);
        int cend = cstart + g_utf8_strlen (lower_q, -1);
        starts = g_renew (int, starts, count + 1);
        ends   = g_renew (int, ends, count + 1);
        starts[count] = cstart;
        ends[count] = cend;
        count++;
        pos += q_len;
    }

    /* Find cursor position to determine current match */
    GtkTextMark *mark = gtk_text_buffer_get_insert (buffer);
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor, mark);
    int cursor_off = gtk_text_iter_get_offset (&cursor);

    int start_idx = 0;
    if (forward) {
        /* Find first match AFTER cursor */
        for (int i = 0; i < count; i++) {
            if (starts[i] > cursor_off) { start_idx = i; break; }
        }
    } else {
        /* Find last match BEFORE cursor */
        for (int i = count - 1; i >= 0; i--) {
            if (ends[i] < cursor_off) { start_idx = i; break; }
        }
    }

    /* Highlight all matches */
    GtkTextTag *match_tag = state->search_match_tag;
    for (int i = 0; i < count; i++) {
        GtkTextIter s, e;
        gtk_text_buffer_get_iter_at_offset (buffer, &s, starts[i]);
        gtk_text_buffer_get_iter_at_offset (buffer, &e, ends[i]);
        gtk_text_buffer_apply_tag (buffer, match_tag, &s, &e);
    }

    /* Highlight and go to current match */
    if (count > 0) {
        GtkTextIter s, e;
        gtk_text_buffer_get_iter_at_offset (buffer, &s, starts[start_idx]);
        gtk_text_buffer_get_iter_at_offset (buffer, &e, ends[start_idx]);
        if (state->search_current_tag)
            gtk_text_buffer_apply_tag (buffer, state->search_current_tag, &s, &e);
        gtk_text_buffer_place_cursor (buffer, &s);
        gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (state->text_view), &s, 0.0, TRUE, 0.0, 0.0);
    }

    state->search_match_count = count;
    state->search_current_match = count > 0 ? start_idx + 1 : 0;

    g_free (starts);
    g_free (ends);
    g_free (lower_full);
    g_free (full);
    g_free (lower_q);
}

static gboolean
on_search_key_pressed (GtkEventControllerKey *controller,
                       guint keyval, guint keycode, GdkModifierType mod,
                       gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        search_find (state, !(mod & GDK_SHIFT_MASK));
        return GDK_EVENT_STOP;
    }
    if (keyval == GDK_KEY_Escape) {
        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (state->search_bar), FALSE);
        search_clear_matches (state);
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

static void
on_search_next_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    search_find (state, true);
}

static void
on_search_prev_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    search_find (state, false);
}

static gboolean
on_search_changed_idle (gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    search_find (state, true);
    return FALSE;
}

static void
on_search_entry_changed (GtkEditable *editable, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    g_idle_add (on_search_changed_idle, state);
}

/* ── Auto-guardado ── */

static gint get_byte_offset(GtkTextBuffer *buffer, GtkTextIter *iter) {
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    char *text = gtk_text_buffer_get_text(buffer, &start, iter, FALSE);
    gint offset = strlen(text);
    g_free(text);
    return offset;
}

static void on_insert_text(GtkTextBuffer *buffer, GtkTextIter *location, gchar *text, gint len, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->is_editing || state->current_document_id <= 0) return;

    int edit_pos_bytes = get_byte_offset(buffer, location);
    int delta_bytes = strlen(text);

    db_highlights_shift_offsets(state->current_document_id, edit_pos_bytes, delta_bytes);
}

static void on_delete_range(GtkTextBuffer *buffer, GtkTextIter *start, GtkTextIter *end, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->is_editing || state->current_document_id <= 0) return;

    int start_bytes = get_byte_offset(buffer, start);
    int end_bytes = get_byte_offset(buffer, end);
    int delta_bytes = -(end_bytes - start_bytes);
    
    db_highlights_shift_offsets(state->current_document_id, start_bytes, delta_bytes);
}

static gboolean
auto_save_cb (gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing && state->has_unsaved_changes)
        save_document (state);
    return G_SOURCE_CONTINUE;
}

static void
auto_save_start (CualiAppState *state)
{
    if (state->auto_save_id > 0) g_source_remove (state->auto_save_id);
    state->auto_save_id = g_timeout_add_seconds (30, auto_save_cb, state);
}

static void
auto_save_stop (CualiAppState *state)
{
    if (state->auto_save_id > 0) {
        g_source_remove (state->auto_save_id);
        state->auto_save_id = 0;
    }
}

/* ── Atajos de teclado ── */

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint keyval, guint keycode, GdkModifierType mod,
                gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;

    if (mod & GDK_CONTROL_MASK) {
        switch (keyval) {
        case GDK_KEY_f:
        case GDK_KEY_F:
            if (!state->is_editing && state->current_document_id > 0) {
                gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (state->search_bar), TRUE);
                gtk_widget_grab_focus (state->search_entry);
                /* Select all text for replacement */
                gtk_editable_select_region (GTK_EDITABLE (state->search_entry), 0, -1);
            }
            return GDK_EVENT_STOP;
        case GDK_KEY_e:
        case GDK_KEY_E:
            if (state->current_document_id > 0) {
                GtkWidget *btn = state->edit_toggle;
                g_signal_emit_by_name (btn, "clicked");
            }
            return GDK_EVENT_STOP;
        case GDK_KEY_z:
        case GDK_KEY_Z:
            if (state->is_editing) {
                GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
                if (gtk_text_buffer_get_can_undo (buf))
                    gtk_text_buffer_undo (buf);
                return GDK_EVENT_STOP;
            }
            break;
        case GDK_KEY_y:
        case GDK_KEY_Y:
            if (state->is_editing) {
                GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
                if (gtk_text_buffer_get_can_redo (buf))
                    gtk_text_buffer_redo (buf);
                return GDK_EVENT_STOP;
            }
            break;
        case GDK_KEY_s:
        case GDK_KEY_S:
            if (state->is_editing && state->has_unsaved_changes)
                save_document (state);
            return GDK_EVENT_STOP;
        case GDK_KEY_b:
        case GDK_KEY_B:
            if (!state->is_editing && state->current_document_id > 0) {
                GtkWidget *hl_btn = gtk_widget_get_first_child (gtk_widget_get_parent (state->edit_toggle));
                if (hl_btn) g_signal_emit_by_name (hl_btn, "clicked");
            }
            return GDK_EVENT_STOP;

        }
    }
    return GDK_EVENT_PROPAGATE;
}

/* ── Arrastrar y soltar archivos ── */

static gboolean
on_drop (GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->current_project_id <= 0) return FALSE;

    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) {
        GSList *files = (GSList *)g_value_get_boxed (value);
        if (!files) return FALSE;
        int count = 0;
        for (GSList *l = files; l; l = l->next) {
            GFile *f = G_FILE (l->data);
            char *path = g_file_get_path (f);
            char *name = g_file_get_basename (f);
            if (!path || !name) { g_free(path); g_free(name); continue; }

            char *html = NULL;
            char *lower = g_ascii_strdown (name, -1);
            if (g_str_has_suffix (lower, ".pdf"))
                html = importer_pdf_to_html (path);
            else {
                gsize length;
                char *raw = NULL;
                if (g_file_load_contents (f, NULL, &raw, &length, NULL, NULL)) {
                    if (raw && (g_strstr_len (raw, 200, "<p") || g_strstr_len (raw, 200, "<html")))
                        html = raw;
                    else {
                        html = importer_text_to_html (raw);
                        g_free (raw);
                    }
                }
            }
            g_free (lower);

            if (html && *html) {
                char *backup = g_strdup_printf ("%s.backup", db_get_path());
                GFile *src = g_file_new_for_path (db_get_path());
                GFile *dst = g_file_new_for_path (backup);
                g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
                g_object_unref (src); g_object_unref (dst);
                g_free (backup);
                db_document_add (state->current_project_id, name, html);
                count++;
            }
            g_free (html); g_free (path); g_free (name);
        }
        refresh_documents (state);
        if (count > 0) {
            char msg[64];
            snprintf (msg, sizeof (msg), "%d document(s) imported", count);
            adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                         adw_toast_new (msg));
        }
        return TRUE;
    }
    return FALSE;
}

/* ── Barra de estado ── */

static void
update_status_bar (CualiAppState *state)
{
    if (!state->status_label || state->current_document_id <= 0) {
        if (state->status_label)
            gtk_label_set_text (GTK_LABEL (state->status_label), "");
        return;
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

    if (!text) { gtk_label_set_text (GTK_LABEL (state->status_label), ""); return; }

    int char_count = g_utf8_strlen (text, -1);
    int word_count = 0;
    bool in_word = false;
    for (const char *p = text; *p; p = g_utf8_next_char (p)) {
        gunichar c = g_utf8_get_char (p);
        if (g_unichar_isalpha (c)) {
            if (!in_word) { word_count++; in_word = true; }
        } else {
            in_word = false;
        }
    }

    int hl_count = state->cached_highlight_count;

    char *markup = g_strdup_printf ("<span size='small'>%d words · %d characters · %d highlights · %d%%</span>",
                                    word_count, char_count, hl_count, (int)(state->zoom_level * 100.0));
    gtk_label_set_markup (GTK_LABEL (state->status_label), markup);
    g_free (markup);
    g_free (text);
}

static void
update_save_indicator (CualiAppState *state, gboolean dirty)
{
    if (!state->save_indicator) return;
    if (state->current_document_id <= 0) {
        gtk_widget_set_visible (state->save_indicator, FALSE);
        return;
    }
    
    gtk_widget_set_visible (state->save_indicator, TRUE);
    if (dirty) {
        gtk_label_set_markup (GTK_LABEL (state->save_indicator), "<span size='small' color='orange'>● Unsaved</span>");
    } else {
        gtk_label_set_markup (GTK_LABEL (state->save_indicator), "<span size='small' color='green'>✓ Saved</span>");
    }
}

static void
result_row_free (ResultRow *row)
{
    if (row) {
        g_free (row->snippet);
        g_free (row->doc_name);
        g_free (row->tags_str);
        g_free (row->memo);
        g_free (row);
    }
}

static GPtrArray*
fetch_results (CualiAppState *state)
{
    GPtrArray *arr = g_ptr_array_new_with_free_func ((GDestroyNotify)result_row_free);
    sqlite3_stmt *stmt = db_results_get_all (state->current_project_id);
    if (stmt) {
        while (sqlite3_step (stmt) == SQLITE_ROW) {
            ResultRow *row = g_new0 (ResultRow, 1);
            row->snippet = g_strdup ((const char *)sqlite3_column_text (stmt, 0));
            row->doc_name = g_strdup ((const char *)sqlite3_column_text (stmt, 1));
            row->tags_str = g_strdup ((const char *)sqlite3_column_text (stmt, 2));
            row->highlight_id = sqlite3_column_int (stmt, 3);
            row->memo = g_strdup ((const char *)sqlite3_column_text (stmt, 4));
            g_ptr_array_add (arr, row);
        }
        sqlite3_finalize (stmt);
    }
    return arr;
}

static void
mark_results_dirty (CualiAppState *state)
{
    state->results_dirty = TRUE;
    state->revision_dirty = TRUE;
    if (state->cached_results) {
        g_ptr_array_unref (state->cached_results);
        state->cached_results = NULL;
    }
}

static void
refresh_stats (CualiAppState *state)
{
    if (!state->stat_docs_row || state->current_project_id <= 0) return;

    /* Doc count */
    int n_docs = 0;
    sqlite3_stmt *s = db_documents_get_all (state->current_project_id);
    if (s) { while (sqlite3_step(s) == SQLITE_ROW) n_docs++; sqlite3_finalize(s); }

    /* Tag count */
    int n_tags = 0;
    s = db_tags_get_all (state->current_project_id);
    if (s) { while (sqlite3_step(s) == SQLITE_ROW) n_tags++; sqlite3_finalize(s); }

    /* Highlight count + total chars highlighted */
    int n_hl = 0; long hl_chars = 0;
    s = db_results_get_all (state->current_project_id);
    if (s) {
        while (sqlite3_step(s) == SQLITE_ROW) {
            n_hl++;
            const char *snip = (const char *)sqlite3_column_text(s, 0);
            if (snip) hl_chars += (long)strlen(snip);
        }
        sqlite3_finalize(s);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%d", n_docs);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(state->stat_docs_row), buf);
    snprintf(buf, sizeof(buf), "%d", n_tags);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(state->stat_tags_row), buf);
    snprintf(buf, sizeof(buf), "%d", n_hl);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(state->stat_highlights_row), buf);
    snprintf(buf, sizeof(buf), "%ld chars coded", hl_chars);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(state->stat_coverage_row), buf);
}

static void
on_alert_response (AdwAlertDialog *self, const char *response, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (strcmp (response, "save") == 0) {
        save_document (state);
        gtk_window_destroy (GTK_WINDOW (state->window));
    } else if (strcmp (response, "discard") == 0) {
        state->has_unsaved_changes = false;
        gtk_window_destroy (GTK_WINDOW (state->window));
    }
}

static void
on_window_destroy (GtkWidget *widget, gpointer user_data)
{
    db_close ();
    
    // Attempt to save last highlight id to a file so it persists
    CualiAppState *state = (CualiAppState *)user_data;
    if (state) {
        char *path = g_build_filename(g_get_home_dir(), ".cuali_last_hl", NULL);
        char *content = g_strdup_printf("%d", state->revision_highlight_id);
        g_file_set_contents(path, content, -1, NULL);
        g_free(content);
        g_free(path);
    }
    
    GApplication *app = g_application_get_default ();
    if (app) {
        g_application_quit (app);
    }
}

static gboolean
on_window_close_request (GtkWindow *window, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing && state->has_unsaved_changes) {
        AdwDialog *dialog = adw_alert_dialog_new ("Save changes?", 
                                                 "There are unsaved changes in the current document.");
        adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                       "cancel", "Cancel",
                                       "discard", "Discard",
                                       "save", "Save",
                                       NULL);
        adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "discard", ADW_RESPONSE_DESTRUCTIVE);
        adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "save", ADW_RESPONSE_SUGGESTED);
        adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "save");
        adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");
        
        g_signal_connect (dialog, "response", G_CALLBACK (on_alert_response), state);
        adw_dialog_present (dialog, state->window);
        return TRUE;
    }
    return FALSE;
}

static void
on_back_to_welcome_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    db_close ();
    state->current_project_id = -1;
    state->current_document_id = -1;
    if (state->css_provider_cache) {
        g_hash_table_remove_all (state->css_provider_cache);
    }
    if (state->cached_results) {
        g_ptr_array_unref (state->cached_results);
        state->cached_results = NULL;
    }
    adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (state->root_stack), "welcome");
    populate_recent_list (state);
}

static void
on_about_clicked (GtkButton *button, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *developers[] = { "Diego", NULL };
    
    adw_show_about_dialog (state->window,
                          "application-name", "Cuali",
                          "application-icon", "org.cuali.CualiGTK",
                          "version", "1.0",
                          "copyright", "© 2026 Diego",
                          "license-type", GTK_LICENSE_LGPL_2_1,
                          "developer-name", "Diego",
                          "developers", developers,
                          "website", "https://github.com/diegoveraniego/cuali-gtk",
                          "comments", "A fast, native qualitative data analysis tool for the Linux desktop.",
                          NULL);
}

static void
on_shortcuts_clicked (GtkButton *button, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *ui_xml = 
        "<interface>"
        "  <object class=\"GtkShortcutsWindow\" id=\"shortcuts_window\">"
        "    <property name=\"modal\">True</property>"
        "    <property name=\"title\">Keyboard Shortcuts</property>"
        "    <child>"
        "      <object class=\"GtkShortcutsSection\">"
        "        <property name=\"section-name\">editor</property>"
        "        <property name=\"max-height\">10</property>"
        "        <child>"
        "          <object class=\"GtkShortcutsGroup\">"
        "            <property name=\"title\">General</property>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Save changes</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;S</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Search</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;F</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Toggle Vim Navigation</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;&lt;alt&gt;V</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Toggle Vim Navigation (Alternate)</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;M</property>"
        "              </object>"
        "            </child>"
        "          </object>"
        "        </child>"
        "        <child>"
        "          <object class=\"GtkShortcutsGroup\">"
        "            <property name=\"title\">Highlights &amp; Tags</property>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Toggle edit mode</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;E</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class=\"GtkShortcutsShortcut\">"
        "                <property name=\"title\">Create highlight</property>"
        "                <property name=\"accelerator\">&lt;ctrl&gt;B</property>"
        "              </object>"
        "            </child>"
        "          </object>"
        "        </child>"
        "      </object>"
        "    </child>"
        "  </object>"
        "</interface>";

    GtkBuilder *builder = gtk_builder_new_from_string (ui_xml, -1);
    GtkWindow *shortcuts_window = GTK_WINDOW (gtk_builder_get_object (builder, "shortcuts_window"));
    gtk_window_set_transient_for (shortcuts_window, GTK_WINDOW (state->window));
    gtk_window_present (shortcuts_window);
    g_object_unref (builder);
}

static void
on_edit_toggle_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  if (state->current_document_id <= 0) return;

  state->is_editing = !state->is_editing;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  
  if (state->is_editing) {
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->text_view), TRUE);
    gtk_button_set_icon_name (GTK_BUTTON (state->edit_toggle), "document-save-symbolic");
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    gtk_text_buffer_remove_tag_by_name (buffer, "highlight", &start, &end);
    update_save_indicator (state, FALSE);
    auto_save_start (state);
    adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                 adw_toast_new ("Editing mode — auto-saving every 30s"));
  } else {
    auto_save_stop (state);
    save_document (state);
    
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->text_view), FALSE);
    gtk_button_set_icon_name (GTK_BUTTON (state->edit_toggle), "document-edit-symbolic");
    
    /* Load to refresh highlights */
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    load_document (state, state->current_document_id, "dummy", text);
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



/* ── Etiquetas jerarquicas ── */

typedef struct _TagNode {
    char *name;
    int tag_id;
    char *color;
    int count;
    GList *children;
} TagNode;

static TagNode*
tag_node_new (const char *name)
{
    TagNode *n = g_new0 (TagNode, 1);
    n->name = g_strdup (name);
    n->tag_id = -1;
    return n;
}

static void
tag_node_free (TagNode *n)
{
    if (!n) return;
    g_free (n->name);
    g_free (n->color);
    g_list_free_full (n->children, (GDestroyNotify) tag_node_free);
    g_free (n);
}

static int
tag_node_find_child (GList *children, const char *name)
{
    int i = 0;
    for (GList *l = children; l; l = l->next, i++) {
        TagNode *c = (TagNode *)l->data;
        if (g_strcmp0 (c->name, name) == 0) return i;
    }
    return -1;
}

static GtkWidget*
create_colored_dot (const char *color, int size)
{
    GtkWidget *dot = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request (dot, size, size);
    char *css = g_strdup_printf ("box { background-color: %s; border-radius: %dpx; min-width: %dpx; min-height: %dpx; }",
                                 color && *color ? color : "#77767b", size/2, size, size);
    GtkCssProvider *p = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (p, css);
    gtk_style_context_add_provider (gtk_widget_get_style_context (dot),
                                    GTK_STYLE_PROVIDER (p),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_free (css);
    gtk_widget_set_valign (dot, GTK_ALIGN_CENTER);
    return dot;
}

static void
on_sidebar_new_tag_activated (GtkEntry *entry, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
    if (text && *text != '\0') {
        int count = 0;
        sqlite3_stmt *stmt = db_tags_get_stats (state->current_project_id);
        if (stmt) {
            while (sqlite3_step (stmt) == SQLITE_ROW) count++;
            sqlite3_finalize (stmt);
        }
        const char *color = TAG_COLORS[count % TAG_COLORS_COUNT];
        db_tag_add (state->current_project_id, text, "", color);
        gtk_editable_set_text (GTK_EDITABLE (entry), "");
        refresh_tags (state);
        refresh_results (state);
    }
}

static void
flatten_tag_tree (TagNode *n, int depth, GtkListBox *list, CualiAppState *state)
{
    if (n->name) {
        bool is_leaf = (n->tag_id > 0);
        
        GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start (box, 12 + (depth * 20));
        gtk_widget_set_margin_end (box, 12);
        gtk_widget_set_margin_top (box, depth == 0 ? 10 : 6);
        gtk_widget_set_margin_bottom (box, depth == 0 ? 6 : 6);

        GtkWidget *dot = create_colored_dot (n->color, depth == 0 ? 8 : 10);
        gtk_box_append (GTK_BOX (box), dot);

        GtkWidget *label = gtk_label_new (n->name);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        if (depth == 0)
            gtk_widget_add_css_class (label, "heading");
        else
            gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
        gtk_box_append (GTK_BOX (box), label);

        if (is_leaf && n->count > 0) {
            char *cs = g_strdup_printf ("%d", n->count);
            GtkWidget *cl = gtk_label_new (cs);
            gtk_widget_add_css_class (cl, "numeric");
            gtk_widget_add_css_class (cl, "dim-label");
            gtk_box_append (GTK_BOX (box), cl);
            g_free (cs);
        }

        if (is_leaf) {
            GtkWidget *eb = gtk_button_new_from_icon_name ("document-edit-symbolic");
            gtk_widget_add_css_class (eb, "flat");
            gtk_widget_set_valign (eb, GTK_ALIGN_CENTER);
            gtk_widget_set_tooltip_text (eb, "Edit tag");
            gpointer *args = g_new (gpointer, 2);
            args[0] = state; args[1] = GINT_TO_POINTER (n->tag_id);
            g_signal_connect_data (eb, "clicked", G_CALLBACK (on_tag_edit_btn_clicked),
                                   args, (GClosureNotify) g_free, 0);
            gtk_box_append (GTK_BOX (box), eb);
        }

        if (depth > 0 || !is_leaf) {
            GtkWidget *arrow = gtk_image_new_from_icon_name (is_leaf ? NULL : "go-next-symbolic");
            if (!is_leaf) {
                gtk_widget_set_valign (arrow, GTK_ALIGN_CENTER);
                gtk_widget_set_opacity (arrow, 0.4);
                gtk_box_append (GTK_BOX (box), arrow);
            }
        }

        GtkListBoxRow *row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
        gtk_list_box_row_set_selectable (row, is_leaf);
        gtk_list_box_row_set_child (row, box);
        if (is_leaf)
            g_object_set_data (G_OBJECT (row), "tag-id", GINT_TO_POINTER (n->tag_id));
        gtk_list_box_append (list, GTK_WIDGET (row));
    }

    for (GList *l = n->children; l; l = l->next) {
        flatten_tag_tree ((TagNode *)l->data, n->name ? depth + 1 : 0, list, state);
    }
}


static void tag_tree_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    char *name = NULL, *color = NULL;
    int count = 0;
    gtk_tree_model_get(model, iter, 1, &name, 2, &color, 3, &count, -1);
    
    if (!color) color = g_strdup("#77767b");
    
    GdkRGBA rgba;
    char *hex_color = NULL;
    if (gdk_rgba_parse(&rgba, color)) {
        hex_color = g_strdup_printf("#%02X%02X%02X", 
                                    (int)(rgba.red * 255 + 0.5), 
                                    (int)(rgba.green * 255 + 0.5), 
                                    (int)(rgba.blue * 255 + 0.5));
    } else {
        hex_color = g_strdup("#77767b");
    }
    
    char *markup;
    if (count > 0) {
        markup = g_strdup_printf("<span foreground=\"%s\">●</span> %s <span foreground=\"#888888\" size=\"smaller\">(%d)</span>", hex_color, name ? name : "", count);
    } else {
        markup = g_strdup_printf("<span foreground=\"%s\">●</span> %s", hex_color, name ? name : "");
    }
    g_free(hex_color);
    
    g_object_set(renderer, "markup", markup, NULL);
    
    if (name) g_free(name);
    if (color) g_free(color);
    g_free(markup);
}

static void on_tag_tree_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        int tag_id = -1;
        gtk_tree_model_get(model, &iter, 0, &tag_id, -1);
        if (tag_id > 0) {
            show_tag_edit_dialog(state, tag_id);
        }
    }
}

static void populate_tag_store(TagNode *n, GtkTreeStore *store, GtkTreeIter *parent_iter) {
    GtkTreeIter iter;
    if (n->name) {
        gtk_tree_store_append(store, &iter, parent_iter);
        gtk_tree_store_set(store, &iter,
            0, n->tag_id,
            1, n->name,
            2, n->color,
            3, n->count,
            -1);
    }
    
    GtkTreeIter *new_parent = n->name ? &iter : parent_iter;
    for (GList *l = n->children; l; l = l->next) {
        populate_tag_store((TagNode *)l->data, store, new_parent);
    }
}

static void
refresh_tags (CualiAppState *state)
{
    if (!state->tag_tree_store) return;

    GtkWidget *child;
    //
        //

    sqlite3_stmt *stmt = db_tags_get_stats (state->current_project_id);
    if (!stmt) return;

    /* Build tree from tag paths */
    TagNode root = {0};
    root.name = NULL;

    while (sqlite3_step (stmt) == SQLITE_ROW) {
        int tag_id = sqlite3_column_int (stmt, 0);
        const char *path = (const char *)sqlite3_column_text (stmt, 1);
        const char *color = (const char *)sqlite3_column_text (stmt, 2);
        int count = sqlite3_column_int (stmt, 3);

        char **parts = g_strsplit (path, "/", -1);
        int n_parts = g_strv_length (parts);

        TagNode *parent = &root;
        for (int i = 0; i < n_parts; i++) {
            int idx = tag_node_find_child (parent->children, parts[i]);
            if (idx >= 0) {
                parent = (TagNode *)g_list_nth_data (parent->children, idx);
            } else {
                TagNode *n = tag_node_new (parts[i]);
                parent->children = g_list_append (parent->children, n);
                parent = n;
            }
        }
        /* Leaf: store tag data */
        parent->tag_id = tag_id;
        parent->color = g_strdup (color);
        parent->count = count;
        g_strfreev (parts);
    }
    sqlite3_finalize (stmt);

    if (state->tag_tree_store) {
        gtk_tree_store_clear(state->tag_tree_store);
        populate_tag_store(&root, state->tag_tree_store, NULL);
        gtk_tree_view_expand_all(GTK_TREE_VIEW(state->tag_tree_view));
        if (state->results_tag_tree_view) {
            gtk_tree_view_expand_all(GTK_TREE_VIEW(state->results_tag_tree_view));
        }
    }

    g_list_free_full (root.children, (GDestroyNotify) tag_node_free);
    refresh_stats (state);
    refresh_visualizations (state);
}

typedef struct {
    CualiAppState *state;
    int highlight_id;
    int document_id;
    char *doc_name;
    char *original_contents;
    char *clean_text;
    int *offset_map;
    int plain_text_len;
    int current_start;
    int current_end;
    char *highlight_color;
    GtkWidget *dialog;
    GtkWidget *text_view;
    GtkTextTag *context_tag;
    GtkTextTag *highlight_tag;
    GtkWidget *memo_view;
} ContextShifterState;

static int
get_adjacent_highlight_id (CualiAppState *state, int current_highlight_id, gboolean get_next);

static void
show_context_shifter_dialog (CualiAppState *state, int highlight_id);

static void
on_context_shifter_destroy (GtkWidget *widget, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    g_free (cstate->doc_name);
    g_free (cstate->clean_text);
    g_free (cstate->original_contents);
    g_free (cstate->highlight_color);
    if (cstate->offset_map) g_free (cstate->offset_map);
    g_free (cstate);
}

static void
context_shifter_redraw (ContextShifterState *cstate)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (cstate->text_view));
    
    // Clear all tags in buffer
    GtkTextIter start_iter, end_iter;
    gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    gtk_text_buffer_remove_all_tags (buffer, &start_iter, &end_iter);
    
    // Apply dimmed context tag to the whole text
    gtk_text_buffer_apply_tag (buffer, cstate->context_tag, &start_iter, &end_iter);
    
    // Convert start_byte and end_byte offsets to character offsets
    int char_start = (int)g_utf8_pointer_to_offset (cstate->clean_text, cstate->clean_text + cstate->current_start);
    int char_end   = (int)g_utf8_pointer_to_offset (cstate->clean_text, cstate->clean_text + cstate->current_end);
    
    GtkTextIter hl_start_iter, hl_end_iter;
    gtk_text_buffer_get_iter_at_offset (buffer, &hl_start_iter, char_start);
    gtk_text_buffer_get_iter_at_offset (buffer, &hl_end_iter, char_end);
    
    // Remove dimmed context tag from the highlight range
    gtk_text_buffer_remove_tag (buffer, cstate->context_tag, &hl_start_iter, &hl_end_iter);
    
    // Apply highlight tag
    gtk_text_buffer_apply_tag (buffer, cstate->highlight_tag, &hl_start_iter, &hl_end_iter);
}

static void
on_start_back_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    if (cstate->current_start <= 0) return;
    int p = cstate->current_start - 1;
    while (p > 0 && (cstate->clean_text[p] == ' ' || cstate->clean_text[p] == '\n' || cstate->clean_text[p] == '\t' || cstate->clean_text[p] == '\r')) {
        p--;
    }
    while (p > 0 && cstate->clean_text[p] != ' ' && cstate->clean_text[p] != '\n' && cstate->clean_text[p] != '\t' && cstate->clean_text[p] != '\r') {
        p--;
    }
    cstate->current_start = (p > 0) ? p + 1 : 0;
    context_shifter_redraw (cstate);
}

static void
on_start_forward_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int p = cstate->current_start;
    while (p < cstate->current_end && cstate->clean_text[p] != ' ' && cstate->clean_text[p] != '\n' && cstate->clean_text[p] != '\t' && cstate->clean_text[p] != '\r') {
        p++;
    }
    while (p < cstate->current_end && (cstate->clean_text[p] == ' ' || cstate->clean_text[p] == '\n' || cstate->clean_text[p] == '\t' || cstate->clean_text[p] == '\r')) {
        p++;
    }
    if (p < cstate->current_end) {
        cstate->current_start = p;
    }
    context_shifter_redraw (cstate);
}

static void
on_end_back_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int p = cstate->current_end - 1;
    while (p > cstate->current_start && cstate->clean_text[p] != ' ' && cstate->clean_text[p] != '\n' && cstate->clean_text[p] != '\t' && cstate->clean_text[p] != '\r') {
        p--;
    }
    while (p > cstate->current_start && (cstate->clean_text[p] == ' ' || cstate->clean_text[p] == '\n' || cstate->clean_text[p] == '\t' || cstate->clean_text[p] == '\r')) {
        p--;
    }
    if (p > cstate->current_start) {
        cstate->current_end = p + 1;
    }
    context_shifter_redraw (cstate);
}

static void
on_end_forward_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int p = cstate->current_end;
    int len = strlen(cstate->clean_text);
    while (p < len && (cstate->clean_text[p] == ' ' || cstate->clean_text[p] == '\n' || cstate->clean_text[p] == '\t' || cstate->clean_text[p] == '\r')) {
        p++;
    }
    while (p < len && cstate->clean_text[p] != ' ' && cstate->clean_text[p] != '\n' && cstate->clean_text[p] != '\t' && cstate->clean_text[p] != '\r') {
        p++;
    }
    cstate->current_end = p;
    context_shifter_redraw (cstate);
}

static void
on_cancel_context_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    gtk_window_destroy (GTK_WINDOW (cstate->dialog));
}

static void
on_save_context_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int len = cstate->current_end - cstate->current_start;
    if (len <= 0) {
        return;
    }

    /* Save memo if active */
    if (cstate->memo_view) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (cstate->memo_view));
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds (buf, &s, &e);
        char *memo = gtk_text_buffer_get_text (buf, &s, &e, FALSE);
        db_highlight_set_memo (cstate->highlight_id, memo ? memo : "");
        g_free (memo);
    }

    char *new_snippet = g_strndup (cstate->clean_text + cstate->current_start, len);
    if (db_highlight_update_bounds (cstate->highlight_id, cstate->current_start, cstate->current_end, new_snippet)) {
        adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (cstate->state->toast_overlay),
                                     adw_toast_new ("Límites y metadatos actualizados quirúrgicamente."));
        if (cstate->state->current_document_id == cstate->document_id) {
            load_document (cstate->state, cstate->document_id, cstate->doc_name, cstate->original_contents);
        }
        refresh_results (cstate->state);
        refresh_tags (cstate->state);
    }
    g_free (new_snippet);

    int next_id = get_adjacent_highlight_id (cstate->state, cstate->highlight_id, TRUE);
    CualiAppState *state = cstate->state;
    gtk_window_destroy (GTK_WINDOW (cstate->dialog));

    if (next_id > 0) {
        show_context_shifter_dialog (state, next_id);
    }
}

typedef struct {
    GtkWidget *text_view;
    int char_start;
} ContextScrollData;

static gboolean
context_scroll_idle (gpointer user_data)
{
    ContextScrollData *data = (ContextScrollData *)user_data;
    if (data && GTK_IS_WIDGET (data->text_view)) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (data->text_view));
        if (buffer) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_offset (buffer, &iter, data->char_start);
            gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (data->text_view), &iter, 0.0, TRUE, 0.5, 0.5);
        }
    }
    g_free (data);
    return G_SOURCE_REMOVE;
}

static int
get_adjacent_highlight_id (CualiAppState *state, int current_highlight_id, gboolean get_next)
{
    if (!state || !state->results_list) return 0;
    
    GtkWidget *curr = gtk_widget_get_first_child (GTK_WIDGET (state->results_list));
    GtkWidget *target_row = NULL;
    GtkWidget *prev_visible_row = NULL;
    gboolean found_current = FALSE;
    
    while (curr != NULL) {
        if (GTK_IS_LIST_BOX_ROW (curr) && gtk_widget_get_child_visible (curr) && gtk_widget_get_visible (curr)) {
            int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (curr), "highlight_id"));
            if (hl_id == current_highlight_id) {
                found_current = TRUE;
                if (!get_next) {
                    target_row = prev_visible_row;
                    break;
                }
            } else if (found_current && get_next) {
                target_row = curr;
                break;
            }
            prev_visible_row = curr;
        }
        curr = gtk_widget_get_next_sibling (curr);
    }
    
    if (target_row) {
        return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (target_row), "highlight_id"));
    }
    return 0;
}

static void
on_prev_highlight_clicked (GtkButton *btn, gpointer user_data);

static void
on_next_highlight_clicked (GtkButton *btn, gpointer user_data);

static void
show_context_shifter_dialog (CualiAppState *state, int highlight_id)
{
    int doc_id = 0;
    char *doc_name = NULL;
    char *contents_html = db_document_get_contents_by_highlight (highlight_id, &doc_id, &doc_name);
    if (!contents_html) return;
    
    int start_off = 0, end_off = 0;
    if (!db_highlight_get_offsets (highlight_id, &start_off, &end_off)) {
        g_free (contents_html);
        g_free (doc_name);
        return;
    }
    
    ContextShifterState *cstate = g_new0 (ContextShifterState, 1);
    cstate->state = state;
    cstate->highlight_id = highlight_id;
    cstate->document_id = doc_id;
    cstate->doc_name = doc_name;
    cstate->original_contents = contents_html;
    cstate->clean_text = map_html (contents_html, &cstate->offset_map, &cstate->plain_text_len);
    cstate->current_start = start_off;
    cstate->current_end = end_off;
    cstate->highlight_color = db_highlight_get_first_tag_color (highlight_id);
    cstate->dialog = gtk_window_new ();
    gtk_window_set_modal (GTK_WINDOW (cstate->dialog), TRUE);
    gtk_window_set_transient_for (GTK_WINDOW (cstate->dialog), GTK_WINDOW (state->window));
    gtk_window_set_title (GTK_WINDOW (cstate->dialog), "Ajustar Límites del Destaque");
    gtk_window_set_default_size (GTK_WINDOW (cstate->dialog), 820, 760);
    
    GtkWidget *main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (main_box, 16);
    gtk_widget_set_margin_end (main_box, 16);
    gtk_widget_set_margin_top (main_box, 16);
    gtk_widget_set_margin_bottom (main_box, 16);
    gtk_window_set_child (GTK_WINDOW (cstate->dialog), main_box);
    
    GtkWidget *title_label = gtk_label_new (NULL);
    char *title_markup = g_strdup_printf ("<span weight=\"bold\" size=\"large\">Ajustar contexto para: %s</span>", doc_name);
    gtk_label_set_markup (GTK_LABEL (title_label), title_markup);
    g_free (title_markup);
    gtk_widget_set_halign (title_label, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (main_box), title_label);
    
    GtkWidget *scrolled = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled, TRUE);
    gtk_widget_add_css_class (scrolled, "card");
    gtk_box_append (GTK_BOX (main_box), scrolled);
    
    cstate->text_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (cstate->text_view), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (cstate->text_view), GTK_WRAP_WORD);
    gtk_widget_set_margin_start (cstate->text_view, 8);
    gtk_widget_set_margin_end (cstate->text_view, 8);
    gtk_widget_set_margin_top (cstate->text_view, 8);
    gtk_widget_set_margin_bottom (cstate->text_view, 8);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), cstate->text_view);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (cstate->text_view));
    gtk_text_buffer_set_text (buffer, cstate->clean_text ? cstate->clean_text : "", -1);
    
    cstate->context_tag = gtk_text_buffer_create_tag (buffer, "context",
                                                      "foreground", "#77767b",
                                                      "style", PANGO_STYLE_ITALIC,
                                                      NULL);
    
    cstate->highlight_tag = gtk_text_buffer_create_tag (buffer, "highlight",
                                                         "background", cstate->highlight_color,
                                                         "foreground", "white",
                                                         "weight", PANGO_WEIGHT_BOLD,
                                                         NULL);
    
    GtkWidget *controls_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_halign (controls_box, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (main_box), controls_box);
    
    GtkWidget *start_control_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append (GTK_BOX (controls_box), start_control_box);
    
    GtkWidget *start_lbl = gtk_label_new ("Límite de Inicio");
    gtk_widget_add_css_class (start_lbl, "dim-label");
    gtk_box_append (GTK_BOX (start_control_box), start_lbl);
    
    GtkWidget *start_btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (start_control_box), start_btn_row);
    
    GtkWidget *btn_start_back = gtk_button_new_with_label ("◄ Expandir");
    g_signal_connect (btn_start_back, "clicked", G_CALLBACK (on_start_back_clicked), cstate);
    gtk_box_append (GTK_BOX (start_btn_row), btn_start_back);
    
    GtkWidget *btn_start_fwd = gtk_button_new_with_label ("Contraer ►");
    g_signal_connect (btn_start_fwd, "clicked", G_CALLBACK (on_start_forward_clicked), cstate);
    gtk_box_append (GTK_BOX (start_btn_row), btn_start_fwd);
    
    GtkWidget *end_control_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append (GTK_BOX (controls_box), end_control_box);
    
    GtkWidget *end_lbl = gtk_label_new ("Límite de Fin");
    gtk_widget_add_css_class (end_lbl, "dim-label");
    gtk_box_append (GTK_BOX (end_control_box), end_lbl);
    
    GtkWidget *end_btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append (GTK_BOX (end_control_box), end_btn_row);
    
    GtkWidget *btn_end_back = gtk_button_new_with_label ("◄ Contraer");
    g_signal_connect (btn_end_back, "clicked", G_CALLBACK (on_end_back_clicked), cstate);
    gtk_box_append (GTK_BOX (end_btn_row), btn_end_back);
    
    GtkWidget *btn_end_fwd = gtk_button_new_with_label ("Expandir ►");
    g_signal_connect (btn_end_fwd, "clicked", G_CALLBACK (on_end_forward_clicked), cstate);
    gtk_box_append (GTK_BOX (end_btn_row), btn_end_fwd);
    
    /* ── Separador y Subtítulo de Codificación ── */
    GtkWidget *sep1 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top (sep1, 8);
    gtk_widget_set_margin_bottom (sep1, 8);
    gtk_box_append (GTK_BOX (main_box), sep1);
    
    GtkWidget *heading_lbl = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (heading_lbl), "<span weight=\"bold\" size=\"medium\">Etiquetas y Notas</span>");
    gtk_widget_set_halign (heading_lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_bottom (heading_lbl, 4);
    gtk_box_append (GTK_BOX (main_box), heading_lbl);
    
    /* ── Buscador y Creación de Etiquetas ── */
    GtkWidget *entry_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append (GTK_BOX (main_box), entry_box);
    
    GtkWidget *search_entry = gtk_search_entry_new ();
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (search_entry), "Buscar etiqueta…");
    gtk_widget_set_hexpand (search_entry, TRUE);
    gtk_box_append (GTK_BOX (entry_box), search_entry);
    
    GtkWidget *tag_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (tag_entry), "Nueva etiqueta…");
    gtk_widget_set_hexpand (tag_entry, TRUE);
    gtk_box_append (GTK_BOX (entry_box), tag_entry);
    
    /* ── Scrolled FlowBox de Etiquetas ── */
    GtkWidget *tags_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (tags_scroll), 100);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (tags_scroll), 160);
    gtk_box_append (GTK_BOX (main_box), tags_scroll);
    
    GtkWidget *flow_box = gtk_flow_box_new ();
    gtk_widget_set_valign (flow_box, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (flow_box), 15);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (flow_box), 12);
    gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (flow_box), 8);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (tags_scroll), flow_box);
    
    /* ── Notas / Memos ── */
    GtkWidget *sep2 = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top (sep2, 4);
    gtk_widget_set_margin_bottom (sep2, 4);
    gtk_box_append (GTK_BOX (main_box), sep2);
    
    GtkWidget *memo_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (memo_scroll), 80);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (memo_scroll), 140);
    gtk_box_append (GTK_BOX (main_box), memo_scroll);
    
    GtkWidget *memo_view = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (memo_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (memo_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (memo_view), 8);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (memo_view), 8);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (memo_view), 8);
    
    char *memo_text = NULL;
    if (highlight_id > 0) {
        db_highlight_get_memo (highlight_id, &memo_text);
    }
    if (memo_text) {
        gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (memo_view)), memo_text, -1);
        g_free (memo_text);
    }
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (memo_scroll), memo_view);
    cstate->memo_view = memo_view;
    
    /* ── Señales y Poblamiento del Panel de Etiquetas ── */
    g_object_set_data (G_OBJECT (tag_entry), "highlight_id", GINT_TO_POINTER (highlight_id));
    g_object_set_data (G_OBJECT (tag_entry), "flow_box", flow_box);
    g_signal_connect (tag_entry, "activate", G_CALLBACK (on_dialog_new_tag_activated), state);
    
    g_signal_connect (search_entry, "search-changed", G_CALLBACK (on_dialog_tag_search_changed), flow_box);
    
    populate_tag_dialog_list (state, highlight_id, flow_box);
    
    int prev_id = get_adjacent_highlight_id (state, highlight_id, FALSE);
    int next_id = get_adjacent_highlight_id (state, highlight_id, TRUE);
 
    GtkWidget *bottom_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append (GTK_BOX (main_box), bottom_row);
    
    GtkWidget *nav_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign (nav_box, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (bottom_row), nav_box);
    
    GtkWidget *btn_prev = gtk_button_new_with_label ("◄ Previous");
    gtk_widget_set_tooltip_text (btn_prev, "Ir al destaque anterior (se descartarán cambios no guardados)");
    gtk_widget_set_sensitive (btn_prev, prev_id > 0);
    g_signal_connect (btn_prev, "clicked", G_CALLBACK (on_prev_highlight_clicked), cstate);
    gtk_box_append (GTK_BOX (nav_box), btn_prev);
    
    GtkWidget *btn_next = gtk_button_new_with_label ("Next ►");
    gtk_widget_set_tooltip_text (btn_next, "Ir al siguiente destaque (se descartarán cambios no guardados)");
    gtk_widget_set_sensitive (btn_next, next_id > 0);
    g_signal_connect (btn_next, "clicked", G_CALLBACK (on_next_highlight_clicked), cstate);
    gtk_box_append (GTK_BOX (nav_box), btn_next);
    
    GtkWidget *action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign (action_box, GTK_ALIGN_END);
    gtk_widget_set_hexpand (action_box, TRUE);
    gtk_box_append (GTK_BOX (bottom_row), action_box);
    
    GtkWidget *btn_cancel = gtk_button_new_with_label ("Cancelar");
    g_signal_connect (btn_cancel, "clicked", G_CALLBACK (on_cancel_context_clicked), cstate);
    gtk_box_append (GTK_BOX (action_box), btn_cancel);
    
    GtkWidget *btn_save = gtk_button_new_with_label ("Guardar");
    gtk_widget_add_css_class (btn_save, "suggested-action");
    g_signal_connect (btn_save, "clicked", G_CALLBACK (on_save_context_clicked), cstate);
    gtk_box_append (GTK_BOX (action_box), btn_save);
    
    g_signal_connect (cstate->dialog, "destroy", G_CALLBACK (on_context_shifter_destroy), cstate);
    
    context_shifter_redraw (cstate);
    
    gtk_window_present (GTK_WINDOW (cstate->dialog));

    ContextScrollData *sdata = g_new0 (ContextScrollData, 1);
    sdata->text_view = cstate->text_view;
    sdata->char_start = (int)g_utf8_pointer_to_offset (cstate->clean_text, cstate->clean_text + cstate->current_start);
    g_idle_add (context_scroll_idle, sdata);
}

static void
on_prev_highlight_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int prev_id = get_adjacent_highlight_id (cstate->state, cstate->highlight_id, FALSE);
    if (prev_id > 0) {
        CualiAppState *state = cstate->state;
        gtk_window_destroy (GTK_WINDOW (cstate->dialog));
        show_context_shifter_dialog (state, prev_id);
    }
}

static void
on_next_highlight_clicked (GtkButton *btn, gpointer user_data)
{
    ContextShifterState *cstate = (ContextShifterState *)user_data;
    int next_id = get_adjacent_highlight_id (cstate->state, cstate->highlight_id, TRUE);
    if (next_id > 0) {
        CualiAppState *state = cstate->state;
        gtk_window_destroy (GTK_WINDOW (cstate->dialog));
        show_context_shifter_dialog (state, next_id);
    }
}

/* =========================================================================
   REVISION TAB IMPLEMENTATION
   ========================================================================= */

static void
revision_shifter_redraw (CualiAppState *state)
{
    if (!state->revision_clean_text || state->revision_highlight_id <= 0) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->revision_text_view));
    
    // Clear all tags in buffer
    GtkTextIter start_iter, end_iter;
    gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    gtk_text_buffer_remove_all_tags (buffer, &start_iter, &end_iter);
    
    // Apply dimmed context tag to the whole text
    gtk_text_buffer_apply_tag (buffer, state->revision_context_tag, &start_iter, &end_iter);
    
    // Convert start_byte and end_byte offsets to character offsets
    int char_start = (int)g_utf8_pointer_to_offset (state->revision_clean_text, state->revision_clean_text + state->revision_current_start);
    int char_end   = (int)g_utf8_pointer_to_offset (state->revision_clean_text, state->revision_clean_text + state->revision_current_end);
    
    GtkTextIter hl_start_iter, hl_end_iter;
    gtk_text_buffer_get_iter_at_offset (buffer, &hl_start_iter, char_start);
    gtk_text_buffer_get_iter_at_offset (buffer, &hl_end_iter, char_end);
    
    // Remove dimmed context tag from the highlight range
    gtk_text_buffer_remove_tag (buffer, state->revision_context_tag, &hl_start_iter, &hl_end_iter);
    
    // Apply highlight tag
    if (state->revision_highlight_color) {
        g_object_set (state->revision_highlight_tag, "background", state->revision_highlight_color, NULL);
    } else {
        g_object_set (state->revision_highlight_tag, "background", "#77767b", NULL);
    }
    gtk_text_buffer_apply_tag (buffer, state->revision_highlight_tag, &hl_start_iter, &hl_end_iter);
}

static void
load_revision_highlight (CualiAppState *state, int highlight_id)
{
    if (highlight_id <= 0) return;

    // Free previous string buffers
    g_free (state->revision_doc_name);
    g_free (state->revision_original_contents);
    g_free (state->revision_clean_text);
    g_free (state->revision_highlight_color);
    if (state->revision_offset_map) {
        g_free (state->revision_offset_map);
        state->revision_offset_map = NULL;
    }

    state->revision_doc_name = NULL;
    state->revision_original_contents = NULL;
    state->revision_clean_text = NULL;
    state->revision_highlight_color = NULL;

    int doc_id = 0;
    char *doc_name = NULL;
    char *contents_html = db_document_get_contents_by_highlight (highlight_id, &doc_id, &doc_name);
    if (!contents_html) return;
    
    int start_off = 0, end_off = 0;
    if (!db_highlight_get_offsets (highlight_id, &start_off, &end_off)) {
        g_free (contents_html);
        g_free (doc_name);
        return;
    }

    state->revision_highlight_id = highlight_id;
    state->revision_document_id = doc_id;
    state->revision_doc_name = doc_name;
    state->revision_original_contents = contents_html;
    state->revision_clean_text = map_html (contents_html, &state->revision_offset_map, &state->revision_plain_text_len);
    state->revision_current_start = start_off;
    state->revision_current_end = end_off;
    state->revision_highlight_color = db_highlight_get_first_tag_color (highlight_id);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->revision_text_view));
    gtk_text_buffer_set_text (buffer, state->revision_clean_text ? state->revision_clean_text : "", -1);

    // Apply redraw/tagging
    revision_shifter_redraw (state);

    // Load memo text
    char *memo_text = NULL;
    db_highlight_get_memo (highlight_id, &memo_text);
    GtkTextBuffer *memo_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->revision_memo_view));
    gtk_text_buffer_set_text (memo_buffer, memo_text ? memo_text : "", -1);
    g_free (memo_text);

    // Set properties/flowbox highlight ID data
    g_object_set_data (G_OBJECT (state->revision_tag_new_entry), "highlight_id", GINT_TO_POINTER (highlight_id));
    
    // Repopulate tag flowbox
    populate_tag_dialog_list (state, highlight_id, state->revision_flow_box);

    // Scroll context view to target highlights position in idle
    ContextScrollData *sdata = g_new0 (ContextScrollData, 1);
    sdata->text_view = state->revision_text_view;
    sdata->char_start = (int)g_utf8_pointer_to_offset (state->revision_clean_text, state->revision_clean_text + state->revision_current_start);
    g_idle_add (context_scroll_idle, sdata);
}

static void
refresh_revision_list (CualiAppState *state)
{
    if (!state->revision_list) return;
    
    const char *visible_tab = adw_view_stack_get_visible_child_name (ADW_VIEW_STACK (state->view_stack));
    if (g_strcmp0 (visible_tab, "revision") != 0) {
        state->revision_dirty = TRUE;
        return;
    }

    if (!state->revision_dirty && state->current_project_id == state->revision_last_project_id) {
        return;
    }
    state->revision_last_project_id = state->current_project_id;
    state->revision_dirty = FALSE;

    refresh_revision_doc_filter_list (state);

    // Remember currently selected highlight_id to restore selection if possible
    int prev_selected_id = state->revision_highlight_id;
    if (prev_selected_id == 0) {
        // Try to load from file on first run
        char *path = g_build_filename(g_get_home_dir(), ".cuali_last_hl", NULL);
        char *content = NULL;
        if (g_file_get_contents(path, &content, NULL, NULL)) {
            prev_selected_id = atoi(content);
            state->revision_highlight_id = prev_selected_id;
            g_free(content);
        }
        g_free(path);
    }
    
    GtkListBoxRow *sel_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (state->revision_list));
    if (sel_row && prev_selected_id == 0) {
        prev_selected_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (sel_row), "highlight_id"));
    }

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (state->revision_list)) != NULL) {
        gtk_list_box_remove (GTK_LIST_BOX (state->revision_list), child);
    }

    GtkListBoxRow *to_select = NULL;

    if (!state->cached_results) {
        state->cached_results = fetch_results (state);
    }

    for (guint i = 0; i < state->cached_results->len; i++) {
        ResultRow *res = g_ptr_array_index (state->cached_results, i);
        const char *snippet = res->snippet;
        const char *doc_name = res->doc_name;
        const char *tags_str = res->tags_str;
        int hl_id = res->highlight_id;
        
        GtkWidget *row = gtk_list_box_row_new ();
        g_object_set_data (G_OBJECT (row), "highlight_id", GINT_TO_POINTER (hl_id));
        if (tags_str) {
            g_object_set_data_full (G_OBJECT (row), "tags_str", g_strdup(tags_str), g_free);
        }
        
        GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_start (card, 8);
        gtk_widget_set_margin_end (card, 8);
        gtk_widget_set_margin_top (card, 8);
        gtk_widget_set_margin_bottom (card, 8);

        GtkWidget *doc_lbl = gtk_label_new (doc_name);
        gtk_widget_add_css_class (doc_lbl, "dim-label");
        gtk_widget_set_halign (doc_lbl, GTK_ALIGN_START);
        gtk_label_set_wrap (GTK_LABEL (doc_lbl), TRUE);
        gtk_box_append (GTK_BOX (card), doc_lbl);

        GtkWidget *snip_lbl = gtk_label_new (NULL);
        char *clean_snippet = strip_html (snippet);
        gtk_label_set_markup (GTK_LABEL (snip_lbl), g_strdup_printf ("“%s”", clean_snippet));
        g_free (clean_snippet);
        gtk_label_set_wrap (GTK_LABEL (snip_lbl), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (snip_lbl), 30);
        gtk_label_set_ellipsize (GTK_LABEL (snip_lbl), PANGO_ELLIPSIZE_END);
        gtk_widget_set_halign (snip_lbl, GTK_ALIGN_START);
        gtk_box_append (GTK_BOX (card), snip_lbl);

        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), card);
        gtk_list_box_append (GTK_LIST_BOX (state->revision_list), row);

        if (hl_id == prev_selected_id) {
            to_select = GTK_LIST_BOX_ROW (row);
        }
        if (!to_select && prev_selected_id == 0) {
            to_select = GTK_LIST_BOX_ROW (row);
        }
    }

    if (to_select) {
        gtk_list_box_select_row (GTK_LIST_BOX (state->revision_list), to_select);
    }
}

static void
on_revision_row_selected (GtkListBox *list_box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!row) {
        state->revision_highlight_id = 0;
        return;
    }

    int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "highlight_id"));
    load_revision_highlight (state, hl_id);

    // Update sensitivity of Previous/Next buttons
    GtkWidget *prev_sib = gtk_widget_get_prev_sibling (GTK_WIDGET (row));
    GtkWidget *prev_row = NULL;
    while (prev_sib != NULL) {
        if (GTK_IS_LIST_BOX_ROW (prev_sib) && gtk_widget_get_child_visible (prev_sib) && gtk_widget_get_visible (prev_sib)) {
            prev_row = prev_sib;
            break;
        }
        prev_sib = gtk_widget_get_prev_sibling (prev_sib);
    }

    GtkWidget *next_sib = gtk_widget_get_next_sibling (GTK_WIDGET (row));
    GtkWidget *next_row = NULL;
    while (next_sib != NULL) {
        if (GTK_IS_LIST_BOX_ROW (next_sib) && gtk_widget_get_child_visible (next_sib) && gtk_widget_get_visible (next_sib)) {
            next_row = next_sib;
            break;
        }
        next_sib = gtk_widget_get_next_sibling (next_sib);
    }

    if (state->revision_btn_prev) {
        gtk_widget_set_sensitive (state->revision_btn_prev, prev_row != NULL);
    }
    if (state->revision_btn_next) {
        gtk_widget_set_sensitive (state->revision_btn_next, next_row != NULL);
    }
}

static void
on_revision_save_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->revision_highlight_id <= 0) return;

    int len = state->revision_current_end - state->revision_current_start;
    if (len <= 0) return;

    /* Save memo */
    GtkTextBuffer *memo_buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->revision_memo_view));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds (memo_buf, &s, &e);
    char *memo = gtk_text_buffer_get_text (memo_buf, &s, &e, FALSE);
    db_highlight_set_memo (state->revision_highlight_id, memo ? memo : "");
    g_free (memo);

    /* Save bounds & update snippet */
    char *new_snippet = g_strndup (state->revision_clean_text + state->revision_current_start, len);
    if (db_highlight_update_bounds (state->revision_highlight_id, state->revision_current_start, state->revision_current_end, new_snippet)) {
        adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                     adw_toast_new ("Límites y notas actualizados quirúrgicamente."));
        mark_results_dirty (state);
        if (state->current_document_id == state->revision_document_id) {
            load_document (state, state->revision_document_id, state->revision_doc_name, state->revision_original_contents);
        }
        refresh_results (state);
        refresh_tags (state);
    }
    g_free (new_snippet);

    /* Advance to NEXT highlight in the sidebar! */
    GtkListBox *list = GTK_LIST_BOX (state->revision_list);
    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (list);
    if (selected_row) {
        GtkWidget *curr = gtk_widget_get_next_sibling (GTK_WIDGET (selected_row));
        GtkWidget *next_row = NULL;
        while (curr != NULL) {
            if (GTK_IS_LIST_BOX_ROW (curr) && gtk_widget_get_child_visible (curr) && gtk_widget_get_visible (curr)) {
                next_row = curr;
                break;
            }
            curr = gtk_widget_get_next_sibling (curr);
        }
        if (next_row) {
            gtk_list_box_select_row (list, GTK_LIST_BOX_ROW (next_row));
        } else {
            adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                         adw_toast_new ("Último destaque guardado."));
        }
    }
}

static void
on_revision_prev_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    GtkListBox *list = GTK_LIST_BOX (state->revision_list);
    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (list);
    if (selected_row) {
        GtkWidget *curr = gtk_widget_get_prev_sibling (GTK_WIDGET (selected_row));
        GtkWidget *prev_row = NULL;
        while (curr != NULL) {
            if (GTK_IS_LIST_BOX_ROW (curr) && gtk_widget_get_child_visible (curr) && gtk_widget_get_visible (curr)) {
                prev_row = curr;
                break;
            }
            curr = gtk_widget_get_prev_sibling (curr);
        }
        if (prev_row) {
            gtk_list_box_select_row (list, GTK_LIST_BOX_ROW (prev_row));
        }
    }
}

static void
on_revision_next_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    GtkListBox *list = GTK_LIST_BOX (state->revision_list);
    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (list);
    if (selected_row) {
        GtkWidget *curr = gtk_widget_get_next_sibling (GTK_WIDGET (selected_row));
        GtkWidget *next_row = NULL;
        while (curr != NULL) {
            if (GTK_IS_LIST_BOX_ROW (curr) && gtk_widget_get_child_visible (curr) && gtk_widget_get_visible (curr)) {
                next_row = curr;
                break;
            }
            curr = gtk_widget_get_next_sibling (curr);
        }
        if (next_row) {
            gtk_list_box_select_row (list, GTK_LIST_BOX_ROW (next_row));
        }
    }
}

static void
on_revision_start_back_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->revision_current_start <= 0) return;
    int p = state->revision_current_start - 1;
    while (p > 0 && (state->revision_clean_text[p] == ' ' || state->revision_clean_text[p] == '\n' || state->revision_clean_text[p] == '\t' || state->revision_clean_text[p] == '\r')) {
        p--;
    }
    while (p > 0 && state->revision_clean_text[p] != ' ' && state->revision_clean_text[p] != '\n' && state->revision_clean_text[p] != '\t' && state->revision_clean_text[p] != '\r') {
        p--;
    }
    state->revision_current_start = (p > 0) ? p + 1 : 0;
    revision_shifter_redraw (state);
}

static void
on_revision_start_forward_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int p = state->revision_current_start;
    while (p < state->revision_current_end && state->revision_clean_text[p] != ' ' && state->revision_clean_text[p] != '\n' && state->revision_clean_text[p] != '\t' && state->revision_clean_text[p] != '\r') {
        p++;
    }
    while (p < state->revision_current_end && (state->revision_clean_text[p] == ' ' || state->revision_clean_text[p] == '\n' || state->revision_clean_text[p] == '\t' || state->revision_clean_text[p] == '\r')) {
        p++;
    }
    if (p < state->revision_current_end) {
        state->revision_current_start = p;
    }
    revision_shifter_redraw (state);
}

static void
on_revision_end_back_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int p = state->revision_current_end - 1;
    while (p > state->revision_current_start && state->revision_clean_text[p] != ' ' && state->revision_clean_text[p] != '\n' && state->revision_clean_text[p] != '\t' && state->revision_clean_text[p] != '\r') {
        p--;
    }
    while (p > state->revision_current_start && (state->revision_clean_text[p] == ' ' || state->revision_clean_text[p] == '\n' || state->revision_clean_text[p] == '\t' || state->revision_clean_text[p] == '\r')) {
        p--;
    }
    if (p > state->revision_current_start) {
        state->revision_current_end = p + 1;
    }
    revision_shifter_redraw (state);
}

static void
on_revision_end_forward_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int p = state->revision_current_end;
    int len = strlen(state->revision_clean_text);
    while (p < len && (state->revision_clean_text[p] == ' ' || state->revision_clean_text[p] == '\n' || state->revision_clean_text[p] == '\t' || state->revision_clean_text[p] == '\r')) {
        p++;
    }
    while (p < len && state->revision_clean_text[p] != ' ' && state->revision_clean_text[p] != '\n' && state->revision_clean_text[p] != '\t' && state->revision_clean_text[p] != '\r') {
        p++;
    }
    state->revision_current_end = p;
    revision_shifter_redraw (state);
}

static void
on_revision_new_tag_activated (GtkEntry *entry, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    int highlight_id = state->revision_highlight_id;
    const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
    
    if (text && *text != '\0') {
        int count = 0;
        sqlite3_stmt *stmt = db_tags_get_stats(state->current_project_id);
        if (stmt) {
            while (sqlite3_step(stmt) == SQLITE_ROW) count++;
            sqlite3_finalize(stmt);
        }
        const char *color = TAG_COLORS[count % TAG_COLORS_COUNT];
        
        int tag_id = db_tag_add (state->current_project_id, text, "", color);
        if (highlight_id > 0 && tag_id > 0) {
            db_highlight_link_tag (highlight_id, tag_id);
        }
        mark_results_dirty (state);
        populate_tag_dialog_list (state, highlight_id, state->revision_flow_box);
        refresh_results (state);
        refresh_tags (state);
    }
}

static void
on_revision_doc_filter_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!row) return;

    int doc_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "doc_id"));
    const char *doc_name = (const char *)g_object_get_data (G_OBJECT (row), "doc_name");
    
    state->revision_current_filter_doc_id = doc_id;
    
    if (state->revision_doc_filter_btn) {
        gtk_menu_button_set_label (GTK_MENU_BUTTON (state->revision_doc_filter_btn), doc_name ? doc_name : "All documents");
    }

    if (state->revision_list) {
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (state->revision_list));
    }

    GtkPopover *popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (state->revision_doc_filter_btn));
    if (popover) {
        gtk_popover_popdown (popover);
    }
}

static void
refresh_revision_doc_filter_list (CualiAppState *state)
{
    if (!state->revision_doc_filter_list) return;

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (state->revision_doc_filter_list))) {
        gtk_list_box_remove (GTK_LIST_BOX (state->revision_doc_filter_list), child);
    }

    GtkWidget *all_row = gtk_list_box_row_new ();
    GtkWidget *all_lbl = gtk_label_new ("All documents");
    gtk_widget_set_margin_start (all_lbl, 12);
    gtk_widget_set_margin_end (all_lbl, 12);
    gtk_widget_set_margin_top (all_lbl, 8);
    gtk_widget_set_margin_bottom (all_lbl, 8);
    gtk_widget_set_halign (all_lbl, GTK_ALIGN_START);
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (all_row), all_lbl);
    g_object_set_data (G_OBJECT (all_row), "doc_id", GINT_TO_POINTER (-1));
    g_object_set_data (G_OBJECT (all_row), "doc_name", NULL);
    gtk_list_box_append (GTK_LIST_BOX (state->revision_doc_filter_list), all_row);

    sqlite3_stmt *stmt = db_documents_get_all (state->current_project_id);
    if (!stmt) return;

    while (sqlite3_step (stmt) == SQLITE_ROW) {
        int doc_id = sqlite3_column_int (stmt, 0);
        const char *doc_name = (const char *)sqlite3_column_text (stmt, 1);
        
        GtkWidget *row = gtk_list_box_row_new ();
        g_object_set_data (G_OBJECT (row), "doc_id", GINT_TO_POINTER (doc_id));
        g_object_set_data_full (G_OBJECT (row), "doc_name", g_strdup (doc_name), g_free);

        GtkWidget *lbl = gtk_label_new (doc_name);
        gtk_widget_set_margin_start (lbl, 12);
        gtk_widget_set_margin_end (lbl, 12);
        gtk_widget_set_margin_top (lbl, 8);
        gtk_widget_set_margin_bottom (lbl, 8);
        gtk_widget_set_halign (lbl, GTK_ALIGN_START);

        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), lbl);
        gtk_list_box_append (GTK_LIST_BOX (state->revision_doc_filter_list), row);
    }
    sqlite3_finalize (stmt);
}

static gboolean
revision_sidebar_filter_func (GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    
    GtkWidget *card = gtk_list_box_row_get_child (row);
    if (!card) return TRUE;
    
    GtkWidget *doc_lbl = gtk_widget_get_first_child (card);
    if (!doc_lbl || !GTK_IS_LABEL (doc_lbl)) return TRUE;
    const char *doc_name = gtk_label_get_text (GTK_LABEL (doc_lbl));
    
    if (state->revision_current_filter_doc_id != -1) {
        const char *filter_doc_name = gtk_menu_button_get_label (GTK_MENU_BUTTON (state->revision_doc_filter_btn));
        if (g_strcmp0 (filter_doc_name, "All documents") != 0 && g_strcmp0 (doc_name, filter_doc_name) != 0) {
            return FALSE;
        }
    }
    
    if (!state->revision_sidebar_search_entry) return TRUE;
    
    const char *query = gtk_editable_get_text (GTK_EDITABLE (state->revision_sidebar_search_entry));
    if (!query || *query == '\0') return TRUE;
    
    GtkWidget *snip_lbl = gtk_widget_get_next_sibling (doc_lbl);
    if (!snip_lbl || !GTK_IS_LABEL (snip_lbl)) return TRUE;
    const char *snippet = gtk_label_get_text (GTK_LABEL (snip_lbl));
    
    const char *tags_str = g_object_get_data (G_OBJECT (row), "tags_str");
    
    gchar *query_folded = g_utf8_casefold (query, -1);
    
    gboolean match = FALSE;
    if (doc_name) {
        gchar *folded = g_utf8_casefold (doc_name, -1);
        if (g_strrstr (folded, query_folded)) match = TRUE;
        g_free (folded);
    }
    if (!match && snippet) {
        gchar *folded = g_utf8_casefold (snippet, -1);
        if (g_strrstr (folded, query_folded)) match = TRUE;
        g_free (folded);
    }
    if (!match && tags_str) {
        gchar *folded = g_utf8_casefold (tags_str, -1);
        if (g_strrstr (folded, query_folded)) match = TRUE;
        g_free (folded);
    }
    
    g_free (query_folded);
    return match;
}

static void
on_revision_sidebar_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->revision_list) {
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (state->revision_list));
    }
}

static void
on_adjust_context_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)g_object_get_data (G_OBJECT (btn), "state");
    int highlight_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (btn), "highlight_id"));
    show_context_shifter_dialog (state, highlight_id);
}

static void
on_load_more_results_clicked (CualiAppState *state)
{
    state->results_limit += 200;
    state->results_dirty = TRUE;
    refresh_results (state);
}

static void
on_copy_result_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *text = (const char *)g_object_get_data (G_OBJECT (btn), "copy_text");
    if (text) {
        GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (btn));
        gdk_clipboard_set_text (clipboard, text);
        if (state && state->toast_overlay) {
            adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
                                         adw_toast_new ("Cita copiada al portapapeles"));
        }
    }
}

static void
refresh_results (CualiAppState *state)
{
  const char *visible_tab = adw_view_stack_get_visible_child_name (ADW_VIEW_STACK (state->view_stack));
  if (g_strcmp0 (visible_tab, "results") != 0) {
      state->results_dirty = TRUE;
      return;
  }

  if (!state->results_dirty && state->current_project_id == state->results_last_project_id) {
      return;
  }
  state->results_last_project_id = state->current_project_id;
  state->results_dirty = FALSE;

  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_list), child);

  if (!state->cached_results) {
      state->cached_results = fetch_results (state);
  }

  guint len = state->cached_results->len;
  if (len == 0) {
      GtkWidget *empty_row = gtk_list_box_row_new ();
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (empty_row), FALSE);
      gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (empty_row), FALSE);
      GtkWidget *status_page = adw_status_page_new ();
      adw_status_page_set_title (ADW_STATUS_PAGE (status_page), "No coded segments");
      adw_status_page_set_icon_name (ADW_STATUS_PAGE (status_page), "tag-symbolic");
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (empty_row), status_page);
      gtk_list_box_append (GTK_LIST_BOX (state->results_list), empty_row);
  } else {
      int count = 0;
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
          
          if (match && state->results_search_entry) {
              const char *query = gtk_editable_get_text (GTK_EDITABLE (state->results_search_entry));
              if (query && *query != '\0') {
                  char *clean_snippet = strip_html (res->snippet);
                  char *folded_snippet = g_utf8_casefold (clean_snippet, -1);
                  char *folded_query = g_utf8_casefold (query, -1);
                  if (!g_strrstr (folded_snippet, folded_query)) {
                      match = FALSE;
                  }
                  g_free (folded_query);
                  g_free (folded_snippet);
                  g_free (clean_snippet);
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

          const char *snippet = res->snippet;
          const char *doc_name = res->doc_name;
          const char *tags_str = res->tags_str;
          int hl_id = res->highlight_id;
          const char *memo = res->memo;
          
          GtkWidget *row = gtk_list_box_row_new ();
          gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
          if (tags_str) {
              g_object_set_data_full (G_OBJECT (row), "tags_str", g_strdup(tags_str), g_free);
          }
          g_object_set_data (G_OBJECT (row), "highlight_id", GINT_TO_POINTER (hl_id));
          
          GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
          gtk_widget_add_css_class (card, "result-card");
          
          GtkWidget *snip_label = gtk_label_new (NULL);
          char *clean_snippet = strip_html (snippet);
          char *snip_markup = g_strdup_printf ("“%s”", clean_snippet);
          gtk_label_set_markup (GTK_LABEL (snip_label), snip_markup);
          g_free (snip_markup);
          gtk_label_set_wrap (GTK_LABEL (snip_label), TRUE);
          gtk_widget_set_halign (snip_label, GTK_ALIGN_START);
          gtk_widget_add_css_class (snip_label, "result-snippet");
          gtk_box_append (GTK_BOX (card), snip_label);

          if (memo && strlen(memo) > 0) {
              GtkWidget *memo_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
              gtk_widget_add_css_class (memo_box, "result-memo-box");
              
              GtkWidget *memo_icon = gtk_image_new_from_icon_name ("document-edit-symbolic");
              gtk_widget_add_css_class (memo_icon, "dim-label");
              gtk_widget_set_valign (memo_icon, GTK_ALIGN_START);
              gtk_box_append (GTK_BOX (memo_box), memo_icon);
              
              GtkWidget *memo_label = gtk_label_new (NULL);
              char *markup = g_strdup_printf ("<span style=\"italic\" size=\"small\">%s</span>", memo);
              gtk_label_set_markup (GTK_LABEL (memo_label), markup);
              g_free (markup);
              gtk_label_set_wrap (GTK_LABEL (memo_label), TRUE);
              gtk_widget_set_halign (memo_label, GTK_ALIGN_START);
              gtk_widget_set_hexpand (memo_label, TRUE);
              gtk_box_append (GTK_BOX (memo_box), memo_label);
              
              gtk_box_append (GTK_BOX (card), memo_box);
          }
          
          /* Tags Flow Grid View to prevent horizontal overflow */
          GtkWidget *tags_flow = gtk_flow_box_new ();
          gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (tags_flow), GTK_SELECTION_NONE);
          gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (tags_flow), 6);
          gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (tags_flow), 4);
          gtk_widget_set_halign (tags_flow, GTK_ALIGN_START);
          
          GString *tags_list = g_string_new("");
          if (tags_str) {
            char **tags = g_strsplit (tags_str, "@@@", -1);
            for (int j = 0; tags[j]; j++) {
                char **parts = g_strsplit (tags[j], "|||", 2);
                if (parts[0] && parts[1]) {
                    if (tags_list->len > 0) g_string_append(tags_list, ", ");
                    g_string_append(tags_list, parts[0]);

                    GtkWidget *tag_badge = gtk_label_new (parts[0]);
                    gtk_widget_add_css_class (tag_badge, "tag-badge");
                    
                    GtkCssProvider *p = g_hash_table_lookup (state->css_provider_cache, parts[1]);
                    if (!p) {
                        char *css = g_strdup_printf("label { background-color: %s; color: white; border: none; }", parts[1]);
                        p = gtk_css_provider_new();
                        gtk_css_provider_load_from_string(p, css);
                        g_free(css);
                        g_hash_table_insert (state->css_provider_cache, g_strdup (parts[1]), p);
                    }
                    gtk_style_context_add_provider(gtk_widget_get_style_context(tag_badge),
                                                   GTK_STYLE_PROVIDER(p),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
     
                    gtk_flow_box_append (GTK_FLOW_BOX (tags_flow), tag_badge);
                }
                g_strfreev(parts);
            }
            g_strfreev (tags);
          }
          gtk_box_append (GTK_BOX (card), tags_flow);
          
          char *copy_text = NULL;
          const char *tags_to_print = (tags_list->len > 0) ? tags_list->str : "Ninguno";
          
          if (memo && strlen(memo) > 0) {
              copy_text = g_strdup_printf("\"%s\"\n\n%s\n\nCódigos: %s\n\nMemo: %s", clean_snippet, doc_name, tags_to_print, memo);
          } else {
              copy_text = g_strdup_printf("\"%s\"\n\n%s\n\nCódigos: %s", clean_snippet, doc_name, tags_to_print);
          }
          
          g_string_free(tags_list, TRUE);
          g_free(clean_snippet);
     
          /* Footer Row containing Document label */
          GtkWidget *footer_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
          gtk_widget_set_margin_top (footer_box, 8);
          gtk_box_append (GTK_BOX (card), footer_box);

          GtkWidget *doc_icon = gtk_image_new_from_icon_name ("document-open-symbolic");
          gtk_widget_add_css_class (doc_icon, "dim-label");
          gtk_widget_set_valign (doc_icon, GTK_ALIGN_CENTER);
          gtk_box_append (GTK_BOX (footer_box), doc_icon);

          GtkWidget *doc_label = gtk_label_new (doc_name);
          gtk_widget_add_css_class (doc_label, "result-meta");
          gtk_widget_set_valign (doc_label, GTK_ALIGN_CENTER);
          gtk_widget_set_hexpand (doc_label, TRUE);
          gtk_box_append (GTK_BOX (footer_box), doc_label);
          
          GtkWidget *copy_btn = gtk_button_new_from_icon_name ("edit-copy-symbolic");
          gtk_widget_set_tooltip_text (copy_btn, "Copiar cita y códigos");
          gtk_widget_add_css_class (copy_btn, "flat");
          gtk_widget_set_valign (copy_btn, GTK_ALIGN_CENTER);
          g_object_set_data_full (G_OBJECT (copy_btn), "copy_text", copy_text, g_free);
          g_signal_connect (copy_btn, "clicked", G_CALLBACK (on_copy_result_clicked), state);
          gtk_box_append (GTK_BOX (footer_box), copy_btn);
          
          gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), card);
          gtk_list_box_append (GTK_LIST_BOX (state->results_list), row);
          count++;
      }
  }
  refresh_stats (state);
}



static void
update_zoom (CualiAppState *state)
{
    double font_size_pt = 12.0 * state->zoom_level;
    
    char *css = g_strdup_printf(
        ".document-view { font-size: %dpt; }\n"
        ".result-snippet { font-size: %dpt; }",
        (int)font_size_pt, (int)(font_size_pt * 1.1)
    );
    
    if (!state->zoom_provider) {
        state->zoom_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(state->zoom_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    
    gtk_css_provider_load_from_string(state->zoom_provider, css);
    g_free(css);
}

static void
on_zoom_in_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    state->zoom_level *= 1.1;
    if (state->zoom_level > 5.0) state->zoom_level = 5.0;
    update_zoom (state);
    update_status_bar (state);
}

static void
on_zoom_out_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    state->zoom_level /= 1.1;
    if (state->zoom_level < 0.2) state->zoom_level = 0.2;
    update_zoom (state);
    update_status_bar (state);
}

static void
on_doc_sidebar_toggle (GtkToggleButton *btn, gpointer user_data)
{
    AdwOverlaySplitView *sv = ADW_OVERLAY_SPLIT_VIEW (user_data);
    adw_overlay_split_view_set_show_sidebar (sv, gtk_toggle_button_get_active (btn));
}

static void
on_delete_doc_response (AdwAlertDialog *dialog, const char *response, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    int doc_id = GPOINTER_TO_INT (args[1]);
    g_free (args);

    if (g_strcmp0 (response, "delete") != 0) return;
    if (db_document_delete (doc_id)) {
        if (state->current_document_id == doc_id) {
            state->current_document_id = -1;
            gtk_text_buffer_set_text (
                gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view)), "", 0);
        }
        mark_results_dirty (state);
        refresh_documents (state);
        refresh_results (state);
        refresh_tags (state);
    }
}

static void
on_delete_doc_clicked (GtkButton *button, gpointer user_data)
{
  GtkWidget *row = GTK_WIDGET (user_data);
  CualiAppState *state = g_object_get_data (G_OBJECT (row), "app-state");
  int id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "doc-id"));
  if (id <= 0) return;

  gpointer *args = g_new (gpointer, 2);
  args[0] = state;
  args[1] = GINT_TO_POINTER (id);

  AdwAlertDialog *dlg = ADW_ALERT_DIALOG (adw_alert_dialog_new (
      "Delete document?",
      "All highlights and coded segments in this document will be permanently deleted."));
  adw_alert_dialog_add_responses (dlg, "cancel", "Cancel", "delete", "Delete", NULL);
  adw_alert_dialog_set_response_appearance (dlg, "delete", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (dlg, "cancel");
  g_signal_connect (dlg, "response", G_CALLBACK (on_delete_doc_response), args);
  adw_dialog_present (ADW_DIALOG (dlg), state->window);
}

static void
on_export_csv_save_response (GObject *source, GAsyncResult *res, gpointer user_data)
{
    gpointer *args = (gpointer *)user_data;
    CualiAppState *state = (CualiAppState *)args[0];
    gboolean codebook    = (gboolean)GPOINTER_TO_INT (args[1]);
    g_free (args);

    GFile *file = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), res, NULL);
    if (!file) return;

    char *path = g_file_get_path (file);
    g_object_unref (file);
    if (!path) return;

    bool ok = codebook
        ? export_codebook_csv   (state->current_project_id, path)
        : export_highlights_csv (state->current_project_id, path);

    adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (state->toast_overlay),
        adw_toast_new (ok ? "Export complete." : "Export failed."));
    g_free (path);
}

static void
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
static void on_export_thematic_clicked (GtkButton *b, gpointer u) { do_export_csv ((CualiAppState*)u, 2);  }

static void
on_doc_imported (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  CualiAppState *state = (CualiAppState *)user_data;
  GFile *file = gtk_file_dialog_open_finish (dialog, res, NULL);
  if (file == NULL) return;

  const char *db_path = db_get_path();
  if (db_path) {
      char *backup_path = g_strdup_printf("%s.backup", db_path);
      GFile *src = g_file_new_for_path(db_path);
      GFile *dst = g_file_new_for_path(backup_path);
      g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
      g_object_unref(src);
      g_object_unref(dst);
      g_free(backup_path);
  }

  char *path = g_file_get_path (file);
  char *name = g_file_get_basename (file);
  char *html = NULL;

  if (path) {
      /* Route by extension */
      char *lower = g_ascii_strdown (name, -1);
      if (g_str_has_suffix (lower, ".pdf")) {
          html = importer_pdf_to_html (path);
      } else {
          /* Try to load as text/HTML */
          gsize length;
          char *raw = NULL;
          if (g_file_load_contents (file, NULL, &raw, &length, NULL, NULL)) {
              /* Heuristic: if it looks like HTML keep it, else wrap as plain text */
              if (raw && (g_strstr_len (raw, 200, "<p") ||
                          g_strstr_len (raw, 200, "<P") ||
                          g_strstr_len (raw, 200, "<html"))) {
                  html = raw;
                  raw = NULL;
              } else {
                  html = importer_text_to_html (raw);
              }
              g_free (raw);
          }
      }
      g_free (lower);
  }

  if (html && *html) {
      db_document_add (state->current_project_id, name, html);
      mark_results_dirty (state);
      refresh_documents (state);
  }

  g_free (html);
  g_free (path);
  g_free (name);
  g_object_unref (file);
}

static void
refresh_documents (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->doc_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->doc_list), child);

  GtkListBoxRow *row_to_select = NULL;
  GtkListBoxRow *first_row = NULL;

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
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_box_append (GTK_BOX (box), label);
      
      GtkWidget *delete_btn = gtk_button_new_from_icon_name ("user-trash-symbolic");
      gtk_widget_add_css_class (delete_btn, "flat");
      gtk_widget_add_css_class (delete_btn, "destructive-action");
      gtk_widget_set_valign (delete_btn, GTK_ALIGN_CENTER);
      gtk_widget_set_tooltip_text (delete_btn, "Delete this document from project");
      g_object_set_data (G_OBJECT (row), "doc-id", GINT_TO_POINTER (id));
      g_object_set_data (G_OBJECT (row), "app-state", state);
      g_signal_connect (delete_btn, "clicked", G_CALLBACK (on_delete_doc_clicked), row);
      gtk_box_append (GTK_BOX (box), delete_btn);

      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data_full (G_OBJECT (row), "doc-name", g_strdup (name), g_free);
      gtk_list_box_append (GTK_LIST_BOX (state->doc_list), row);

      if (!first_row) first_row = GTK_LIST_BOX_ROW (row);
      if (id == state->last_document_id) row_to_select = GTK_LIST_BOX_ROW (row);
    }
    sqlite3_finalize (stmt);
  }

  if (row_to_select) {
      gtk_list_box_select_row(GTK_LIST_BOX(state->doc_list), row_to_select);
  } else if (first_row) {
      gtk_list_box_select_row(GTK_LIST_BOX(state->doc_list), first_row);
  } else {
      GtkWidget *empty_row = gtk_list_box_row_new ();
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (empty_row), FALSE);
      gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (empty_row), FALSE);
      GtkWidget *status_page = adw_status_page_new ();
      adw_status_page_set_title (ADW_STATUS_PAGE (status_page), "No documents");
      adw_status_page_set_icon_name (ADW_STATUS_PAGE (status_page), "document-open-symbolic");
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (empty_row), status_page);
      gtk_list_box_append (GTK_LIST_BOX (state->doc_list), empty_row);
  }
}

static void add_to_recent (const char *path, int doc_id);

static void
on_doc_row_selected (GtkListBox    *listbox,
                     GtkListBoxRow *row,
                     gpointer       user_data)
{
  if (!row) return;
  CualiAppState *state = (CualiAppState *)user_data;
  gpointer id_ptr = g_object_get_data (G_OBJECT (row), "doc-id");
  if (!id_ptr) return;
  int id = GPOINTER_TO_INT (id_ptr);
  const char *name = g_object_get_data (G_OBJECT (row), "doc-name");
  char *contents = db_document_get_contents (id);
  load_document (state, id, name, contents);
  g_free (contents);
  state->last_document_id = id;
  if (db_get_path()) {
      add_to_recent (db_get_path(), id);
  }
}


static void
refresh_all (CualiAppState *state)
{
    refresh_project_info (state);
    refresh_documents (state);
    refresh_results (state);
    refresh_tags (state);
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
add_to_recent (const char *path, int doc_id)
{
    if (!path) return;
    char *recent_file = get_recent_file_path ();
    GList *lines = NULL;
    char *contents = NULL;
    
    char *new_entry = g_strdup_printf("%s|%d", path, doc_id);
    
    if (g_file_get_contents (recent_file, &contents, NULL, NULL)) {
        char **split = g_strsplit (contents, "\n", -1);
        for (int i = 0; split[i]; i++) {
            if (strlen(split[i]) > 0) {
                char **parts = g_strsplit (split[i], "|", 2);
                if (parts[0] && g_strcmp0 (parts[0], path) != 0) {
                    lines = g_list_append (lines, g_strdup (split[i]));
                }
                g_strfreev(parts);
            }
        }
        g_strfreev (split);
        g_free (contents);
    }
    
    lines = g_list_prepend (lines, new_entry);
    
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

static void open_project_at_path (CualiAppState *state, const char *path, int doc_id);

static void
on_recent_row_selected (GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data)
{
    if (!row) return;
    CualiAppState *state = (CualiAppState *)user_data;
    const char *path = g_object_get_data (G_OBJECT (row), "project-path");
    int doc_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "last-doc-id"));
    open_project_at_path (state, path, doc_id);
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
            
            char **parts = g_strsplit (split[i], "|", 2);
            if (!parts[0] || !g_file_test (parts[0], G_FILE_TEST_EXISTS)) {
                g_strfreev(parts);
                continue;
            }
            
            empty = false;
            GtkWidget *row = gtk_list_box_row_new ();
            GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_start (box, 12);
            gtk_widget_set_margin_end (box, 12);
            gtk_widget_set_margin_top (box, 8);
            gtk_widget_set_margin_bottom (box, 8);
            
            GtkWidget *icon = gtk_image_new_from_icon_name ("document-open-symbolic");
            gtk_box_append (GTK_BOX (box), icon);
            
            char *basename = g_path_get_basename (parts[0]);
            GtkWidget *label = gtk_label_new (basename);
            gtk_widget_set_halign (label, GTK_ALIGN_START);
            gtk_box_append (GTK_BOX (box), label);
            g_free (basename);
            
            gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
            g_object_set_data_full (G_OBJECT (row), "project-path", g_strdup (parts[0]), g_free);
            int doc_id = parts[1] ? atoi(parts[1]) : -1;
            g_object_set_data (G_OBJECT (row), "last-doc-id", GINT_TO_POINTER (doc_id));
            gtk_list_box_append (GTK_LIST_BOX (state->recent_list), row);
            
            g_strfreev(parts);
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
open_project_at_path (CualiAppState *state, const char *path, int doc_id)
{
    if (db_init (path)) {
      state->current_project_id = db_project_get_first_id ();
      state->last_document_id = doc_id;
      state->current_document_id = -1;
      if (state->css_provider_cache) {
          g_hash_table_remove_all (state->css_provider_cache);
      }
      mark_results_dirty (state);
      state->results_limit = 200;
      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view)), "", -1);
      refresh_all (state);
      refresh_stats (state);
      adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (state->root_stack), "main");
      add_to_recent (path, doc_id);
    } else {
        g_printerr("Failed to initialize database at %s\n", path);
    }
}

static void
create_highlight_and_show_tags (CualiAppState *state)
{
  if (state->current_document_id <= 0) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  GtkTextIter start, end;
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end)) {
    GtkTextIter buf_start;
    gtk_text_buffer_get_start_iter(buffer, &buf_start);
    
    char *text_before_start = gtk_text_buffer_get_text(buffer, &buf_start, &start, FALSE);
    char *text_before_end = gtk_text_buffer_get_text(buffer, &buf_start, &end, FALSE);
    
    int html_start = text_before_start ? strlen(text_before_start) : 0;
    int html_end = text_before_end ? strlen(text_before_end) : 0;
    
    g_free(text_before_start);
    g_free(text_before_end);
    
    char *snippet = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    int hl_id = db_highlight_add (state->current_document_id, html_start, html_end, snippet);
    if (hl_id > 0) {
      mark_results_dirty (state);
      apply_highlight_tag (buffer, hl_id, &start, &end, NULL);
      show_tag_dialog (state, hl_id);
    }
    g_free (snippet);
  }
}

static void
on_highlight_button_clicked (GtkButton *button, gpointer user_data)
{
  CualiAppState *state = (CualiAppState *)user_data;
  create_highlight_and_show_tags (state);
}

static void
on_clear_tags_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->current_project_id > 0) {
        db_project_clear_tags(state->current_project_id);
        mark_results_dirty (state);
        refresh_all(state);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gtk_text_buffer_remove_tag_by_name(buffer, "highlight", &start, &end);
    }
}

static void
on_clear_project_clicked (GtkButton *btn, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->current_project_id > 0) {
        db_project_clear_data(state->current_project_id);
        mark_results_dirty (state);
        state->current_document_id = -1;
        gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view)), "", -1);
        refresh_all(state);
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
      open_project_at_path (state, path, -1);
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
    open_project_at_path (state, path, -1);
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
on_view_stack_visible_child_changed (GObject *object, GParamSpec *pspec, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    const char *name = adw_view_stack_get_visible_child_name (ADW_VIEW_STACK (state->view_stack));
    if (g_strcmp0 (name, "results") == 0) refresh_results (state);
    if (g_strcmp0 (name, "revision") == 0) refresh_revision_list (state);
    if (g_strcmp0 (name, "info") == 0) refresh_project_info (state);
}
static void update_vim_status(CualiAppState *state) {
    if (!state->vim_enabled || !state->vim_mode_label) {
        if (state->vim_mode_label) gtk_widget_set_visible(state->vim_mode_label, FALSE);
        return;
    }
    gtk_widget_set_visible(state->vim_mode_label, TRUE);
    gtk_widget_add_css_class(state->vim_mode_label, "vim-badge");
    if (state->vim_mode == VIM_NORMAL) {
        gtk_label_set_text(GTK_LABEL(state->vim_mode_label), " NORMAL ");
    } else {
        gtk_label_set_text(GTK_LABEL(state->vim_mode_label), " VISUAL ");
    }
}
static void on_cursor_moved(GtkTextBuffer *buffer, GtkTextIter *iter,
                             GtkTextMark *mark, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (mark == gtk_text_buffer_get_insert(buffer))
        gtk_widget_queue_draw(state->vim_cursor_area);
}

static void
draw_vim_cursor(GtkDrawingArea *area, cairo_t *cr, 
                int width, int height, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->vim_enabled) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

    GdkRectangle iter_rect;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(state->text_view), &iter, &iter_rect);

    int wx, wy;
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(state->text_view), GTK_TEXT_WINDOW_WIDGET, iter_rect.x, iter_rect.y, &wx, &wy);

    int char_width = iter_rect.width;
    if (char_width <= 0) char_width = iter_rect.height * 0.5;
    
    PangoLayout *layout = gtk_widget_create_pango_layout(state->text_view, NULL);
    
    PangoFontDescription *desc = pango_font_description_from_string("Inter");
    pango_font_description_set_size(desc, 12.0 * state->zoom_level * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    char buf[8] = {0};
    gunichar ch = gtk_text_iter_get_char(&iter);
    
    if (ch && ch != '\n' && ch != '\r' && ch != 0xFFFC) {
        g_unichar_to_utf8(ch, buf);
        pango_layout_set_text(layout, buf, -1);
    }

    gboolean is_dark = adw_style_manager_get_dark(adw_style_manager_get_default());

    if (is_dark) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); /* White cursor */
    } else {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0); /* Black cursor */
    }

    cairo_rectangle(cr, wx, wy, char_width, iter_rect.height);
    cairo_fill(cr);

    if (buf[0] != '\0') {
        if (is_dark) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0); /* Black text */
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0); /* White text */
        }
        
        // Offset by pixels_above_lines to perfectly align with GTK's text baseline
        int pixels_above = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(state->text_view));
        cairo_move_to(cr, wx, wy + pixels_above);
        pango_cairo_show_layout(cr, layout);
    }
    g_object_unref(layout);
}

static void ensure_cursor_visible(CualiAppState *state) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
    
    GdkRectangle rect;
    gtk_text_view_get_cursor_locations(GTK_TEXT_VIEW(state->text_view), &iter, &rect, NULL);
    
    int wx, wy;
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(state->text_view), GTK_TEXT_WINDOW_WIDGET, rect.x, rect.y, &wx, &wy);
    
    GtkWidget *scroll = gtk_widget_get_ancestor(state->text_view, GTK_TYPE_SCROLLED_WINDOW);
    if (!scroll) return;
    
    GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    double v_val = gtk_adjustment_get_value(vadjustment);
    double v_page = gtk_adjustment_get_page_size(vadjustment);
    double v_upper = gtk_adjustment_get_upper(vadjustment);
    
    double padding = 60.0;
    
    if (wy < v_val + padding) {
        gtk_adjustment_set_value(vadjustment, MAX(0, wy - padding));
    } else if (wy + rect.height > v_val + v_page - padding) {
        gtk_adjustment_set_value(vadjustment, MIN(v_upper - v_page, wy + rect.height - v_page + padding));
    }
}

static void update_vim_cursor(CualiAppState *state) {
    gtk_widget_remove_css_class(state->text_view, "vim-normal");
    gtk_widget_remove_css_class(state->text_view, "vim-visual");
    if (!state->vim_enabled) return;
    
    if (state->vim_mode == VIM_NORMAL) {
        gtk_widget_add_css_class(state->text_view, "vim-normal");
    } else {
        gtk_widget_add_css_class(state->text_view, "vim-visual");
    }
    if (state->vim_cursor_area) gtk_widget_queue_draw(state->vim_cursor_area);
}

static gboolean open_highlight_dialog_at_cursor(CualiAppState *state) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
    
    GSList *tags = gtk_text_iter_get_tags(&iter);
    GSList *hl_ids = NULL;
    int count = 0;
    for (GSList *l = tags; l; l = l->next) {
        int hl_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l->data), "highlight-id"));
        if (hl_id > 0) {
            hl_ids = g_slist_append(hl_ids, GINT_TO_POINTER(hl_id));
            count++;
        }
    }
    g_slist_free(tags);
    
    if (count == 0) return FALSE;
    
    if (count == 1) {
        show_tag_dialog(state, GPOINTER_TO_INT(hl_ids->data));
    } else {
        show_highlight_selector_dialog(state, hl_ids, 0, 0);
    }
    g_slist_free(hl_ids);
    return TRUE;
}


static gboolean
on_vim_key_pressed(GtkEventControllerKey *controller,
                   guint keyval, guint keycode, GdkModifierType mod,
                   gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->vim_enabled || state->is_editing) return GDK_EVENT_PROPAGATE;
    
    GtkWidget *focus = gtk_root_get_focus(GTK_ROOT(state->window));
    if (GTK_IS_EDITABLE(focus)) return GDK_EVENT_PROPAGATE;

    /* \u2500\u2500 f/F/t/T: capture the target character \u2500\u2500 */
    if (g_object_get_data(G_OBJECT(state->window), "vim-awaiting-find")) {
        g_object_set_data(G_OBJECT(state->window), "vim-awaiting-find", NULL);
        if (keyval < 0x20 || keyval > 0x10FFFF) return GDK_EVENT_STOP; /* skip non-printable */
        gunichar target_ch = gdk_keyval_to_unicode(keyval);
        if (!target_ch) return GDK_EVENT_STOP;
        state->vim_find_char = target_ch;

        GtkTextBuffer *fbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
        GtkTextMark *fmark = gtk_text_buffer_get_insert(fbuf);
        GtkTextIter fiter;
        gtk_text_buffer_get_iter_at_mark(fbuf, &fiter, fmark);

        int find_repeat = (state->vim_count > 0) ? state->vim_count : 1;
        state->vim_count = 0;

        gchar needle[8] = {0};
        g_unichar_to_utf8(target_ch, needle);

        for (int r = 0; r < find_repeat; r++) {
            GtkTextIter found;
            if (state->vim_find_forward) {
                GtkTextIter search = fiter; gtk_text_iter_forward_char(&search);
                GtkTextIter end; gtk_text_buffer_get_end_iter(fbuf, &end);
                if (gtk_text_iter_forward_search(&search, needle, GTK_TEXT_SEARCH_VISIBLE_ONLY, &found, NULL, &end)) {
                    fiter = found;
                    if (state->vim_find_till) gtk_text_iter_backward_char(&fiter);
                }
            } else {
                GtkTextIter start; gtk_text_buffer_get_start_iter(fbuf, &start);
                if (gtk_text_iter_backward_search(&fiter, needle, GTK_TEXT_SEARCH_VISIBLE_ONLY, &found, NULL, &start)) {
                    fiter = found;
                    if (state->vim_find_till) gtk_text_iter_forward_char(&fiter);
                }
            }
        }
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(fbuf, "insert", &fiter);
        else gtk_text_buffer_place_cursor(fbuf, &fiter);
        ensure_cursor_visible(state);
        update_vim_cursor(state);
        return GDK_EVENT_STOP;
    }
    
    // Interceptar Ctrl+B explícitamente y dejar pasar otros atajos globales
    if (mod & GDK_CONTROL_MASK) {
        if (keyval == GDK_KEY_b || keyval == GDK_KEY_B) {
            create_highlight_and_show_tags(state);
            state->vim_mode = VIM_NORMAL;
            update_vim_status(state);
            return GDK_EVENT_STOP;
        }
        if (keyval != GDK_KEY_d && keyval != GDK_KEY_u && keyval != GDK_KEY_f && keyval != GDK_KEY_v) {
            return GDK_EVENT_PROPAGATE;
        }
    }
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->text_view));
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

    gboolean handled = TRUE;

    /* ── Numeric count accumulator ── */
    if (keyval >= GDK_KEY_1 && keyval <= GDK_KEY_9 && state->vim_count == 0) {
        state->vim_count = keyval - GDK_KEY_0;
        return GDK_EVENT_STOP;
    }
    if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9 && state->vim_count > 0) {
        state->vim_count = state->vim_count * 10 + (keyval - GDK_KEY_0);
        return GDK_EVENT_STOP;
    }
    int repeat = (state->vim_count > 0) ? state->vim_count : 1;
    state->vim_count = 0; /* consume */

    switch (keyval) {
    
    case GDK_KEY_w: {
        for (int i = 0; i < repeat; i++) {
            gtk_text_iter_forward_word_end(&iter);
            gtk_text_iter_forward_word_end(&iter);
            gtk_text_iter_backward_word_start(&iter);
        }
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_e: {
        for (int i = 0; i < repeat; i++) gtk_text_iter_forward_word_end(&iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_b: {
        for (int i = 0; i < repeat; i++) gtk_text_iter_backward_word_start(&iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_h: {
        for (int i = 0; i < repeat; i++) gtk_text_iter_backward_char(&iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_l: {
        for (int i = 0; i < repeat; i++) gtk_text_iter_forward_char(&iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_j: {
        for (int i = 0; i < repeat; i++) gtk_text_view_forward_display_line(GTK_TEXT_VIEW(state->text_view), &iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_k: {
        for (int i = 0; i < repeat; i++) gtk_text_view_backward_display_line(GTK_TEXT_VIEW(state->text_view), &iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_0: {
        gtk_text_iter_set_line_offset(&iter, 0);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_dollar: {
        gtk_text_iter_forward_to_line_end(&iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_asciicircum: { // ^ key
        gtk_text_iter_set_line_offset(&iter, 0);
        while (gtk_text_iter_get_char(&iter) == ' ' || gtk_text_iter_get_char(&iter) == '\t') {
            if (!gtk_text_iter_forward_char(&iter)) break;
        }
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_g: {
        guint32 now = gtk_event_controller_get_current_event_time(GTK_EVENT_CONTROLLER(controller));
        if (now - state->last_g_time < 500) {
            gtk_text_buffer_get_start_iter(buffer, &iter);
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
            state->last_g_time = 0;
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(state->text_view), mark, 0.0, TRUE, 0.0, 0.0);
        } else {
            state->last_g_time = now;
            return GDK_EVENT_STOP; // Wait for second g
        }
        break;
    }
    case GDK_KEY_G: {
        gtk_text_buffer_get_end_iter(buffer, &iter);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(state->text_view), mark, 0.0, TRUE, 0.0, 0.0);
        break;
    }
    case GDK_KEY_d: {
        if (mod & GDK_CONTROL_MASK) {
            for (int i = 0; i < 15 * repeat; i++) gtk_text_view_forward_display_line(GTK_TEXT_VIEW(state->text_view), &iter);
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
        } else handled = FALSE;
        break;
    }
    case GDK_KEY_u: {
        if (mod & GDK_CONTROL_MASK) {
            for (int i = 0; i < 15 * repeat; i++) gtk_text_view_backward_display_line(GTK_TEXT_VIEW(state->text_view), &iter);
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
        } else handled = FALSE;
        break;
    }
    case GDK_KEY_f:
    case GDK_KEY_F:
    case GDK_KEY_t:
    case GDK_KEY_T: {
        if (mod & GDK_CONTROL_MASK) {
            /* Ctrl+f = page down */
            for (int i = 0; i < 30 * repeat; i++) gtk_text_view_forward_display_line(GTK_TEXT_VIEW(state->text_view), &iter);
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
        } else {
            /* f/F/t/T: wait for next char — store intent, consume event */
            state->vim_find_forward = (keyval == GDK_KEY_f || keyval == GDK_KEY_t);
            state->vim_find_till   = (keyval == GDK_KEY_t || keyval == GDK_KEY_T);
            state->vim_find_char   = 0; /* signal: waiting for char */
            /* We mark handled=FALSE so key propagates, but we need capture:
               instead use a one-shot flag: next printable key is the target */
            state->vim_count = repeat; /* reuse: carry repeat into find */
            g_object_set_data(G_OBJECT(state->window), "vim-awaiting-find", GINT_TO_POINTER(1));
            return GDK_EVENT_STOP;
        }
        break;
    }
    case GDK_KEY_semicolon: { /* ; = repeat last f/F/t/T */
        if (state->vim_find_char != 0) {
            GtkTextIter search = iter;
            for (int r = 0; r < repeat; r++) {
                GtkTextIter found;
                gchar needle[8] = {0};
                g_unichar_to_utf8(state->vim_find_char, needle);
                if (state->vim_find_forward) {
                    gtk_text_iter_forward_char(&search);
                    GtkTextIter end; gtk_text_buffer_get_end_iter(buffer, &end);
                    if (gtk_text_iter_forward_search(&search, needle, GTK_TEXT_SEARCH_VISIBLE_ONLY, &found, NULL, &end)) {
                        search = found;
                        if (state->vim_find_till && gtk_text_iter_backward_char(&search)) {}
                    }
                } else {
                    GtkTextIter start; gtk_text_buffer_get_start_iter(buffer, &start);
                    if (gtk_text_iter_backward_search(&search, needle, GTK_TEXT_SEARCH_VISIBLE_ONLY, &found, NULL, &start)) {
                        search = found;
                        if (state->vim_find_till) gtk_text_iter_forward_char(&search);
                    }
                }
            }
            iter = search;
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
        } else handled = FALSE;
        break;
    }
    case GDK_KEY_percent: { /* % = jump to matching bracket */
        static const char open_brackets[]  = "({[";
        static const char close_brackets[] = ")}]";
        gunichar ch = gtk_text_iter_get_char(&iter);
        char *open_pos  = strchr(open_brackets,  (char)ch);
        char *close_pos = strchr(close_brackets, (char)ch);
        if (open_pos) {
            int idx = open_pos - open_brackets;
            char open_c = open_brackets[idx], close_c = close_brackets[idx];
            GtkTextIter search = iter; gtk_text_iter_forward_char(&search);
            int depth = 1;
            while (!gtk_text_iter_is_end(&search)) {
                gunichar c = gtk_text_iter_get_char(&search);
                if (c == (gunichar)open_c) depth++;
                else if (c == (gunichar)close_c) { depth--; if (depth == 0) { iter = search; break; } }
                gtk_text_iter_forward_char(&search);
            }
        } else if (close_pos) {
            int idx = close_pos - close_brackets;
            char open_c = open_brackets[idx], close_c = close_brackets[idx];
            GtkTextIter search = iter; gtk_text_iter_backward_char(&search);
            int depth = 1;
            while (!gtk_text_iter_is_start(&search)) {
                gunichar c = gtk_text_iter_get_char(&search);
                if (c == (gunichar)close_c) depth++;
                else if (c == (gunichar)open_c) { depth--; if (depth == 0) { iter = search; break; } }
                gtk_text_iter_backward_char(&search);
            }
        } else handled = FALSE;
        if (handled) {
            if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
            else gtk_text_buffer_place_cursor(buffer, &iter);
        }
        break;
    }
    case GDK_KEY_z: {
        guint32 now = gtk_event_controller_get_current_event_time(GTK_EVENT_CONTROLLER(controller));
        if (now - state->last_z_time < 500) {
            /* zz = center */
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(state->text_view), mark, 0.0, TRUE, 0.5, 0.5);
            state->last_z_time = 0;
        } else {
            state->last_z_time = now;
            return GDK_EVENT_STOP;
        }
        break;
    }
    case GDK_KEY_Z: {
        /* Shift+z pressed after z was stored? Use last_z_time trick differently.
           Actually zb/zt come as separate chars; simplest: treat Z as zt and
           use z+Return for zb via last_z_time. For simplicity: Z = center (zz). */
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(state->text_view), mark, 0.0, TRUE, 0.5, 0.0);
        break;
    }
    case GDK_KEY_braceleft: { // {
        gtk_text_iter_backward_search(&iter, "\n\n", GTK_TEXT_SEARCH_VISIBLE_ONLY, &iter, NULL, NULL);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_braceright: { // }
        gtk_text_iter_forward_search(&iter, "\n\n", GTK_TEXT_SEARCH_VISIBLE_ONLY, &iter, NULL, NULL);
        if (state->vim_mode == VIM_VISUAL) gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
        else gtk_text_buffer_place_cursor(buffer, &iter);
        break;
    }
    case GDK_KEY_slash: { // /
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(state->search_bar), TRUE);
        gtk_widget_grab_focus(state->search_entry);
        break;
    }
    case GDK_KEY_n: {
        search_find(state, TRUE);
        break;
    }
    case GDK_KEY_N: {
        search_find(state, FALSE);
        break;
    }
    case GDK_KEY_asterisk: { // *
        GtkTextIter word_start = iter, word_end = iter;
        if (!gtk_text_iter_starts_word(&word_start)) gtk_text_iter_backward_word_start(&word_start);
        if (!gtk_text_iter_ends_word(&word_end)) gtk_text_iter_forward_word_end(&word_end);
        char *word = gtk_text_iter_get_text(&word_start, &word_end);
        if (word && strlen(word) > 0) {
            gtk_editable_set_text(GTK_EDITABLE(state->search_entry), word);
            search_find(state, TRUE);
        }
        g_free(word);
        break;
    }
    case GDK_KEY_v: {
        if (state->vim_mode == VIM_NORMAL) {
            state->vim_mode = VIM_VISUAL;
            GtkTextMark *sel = gtk_text_buffer_get_selection_bound(buffer);
            gtk_text_buffer_move_mark(buffer, sel, &iter);
        } else {
            state->vim_mode = VIM_NORMAL;
            gtk_text_buffer_place_cursor(buffer, &iter);
        }
        update_vim_status(state);
        break;
    }
    case GDK_KEY_Escape: {
        if (state->vim_mode == VIM_VISUAL) {
            state->vim_mode = VIM_NORMAL;
            gtk_text_buffer_place_cursor(buffer, &iter);
            update_vim_status(state);
            break;
        }
        return GDK_EVENT_PROPAGATE;
    }
    case GDK_KEY_Return: {
        if (state->vim_mode == VIM_VISUAL) {
            create_highlight_and_show_tags(state);
            state->vim_mode = VIM_NORMAL;
            update_vim_status(state);
            break;
        } else if (state->vim_mode == VIM_NORMAL) {
            if (open_highlight_dialog_at_cursor(state)) {
                return GDK_EVENT_STOP;
            }
        }
        return GDK_EVENT_PROPAGATE;
    }
    default:
        handled = FALSE;
        break;
    }
    
    if (handled) {
        ensure_cursor_visible(state);
        update_vim_cursor(state);
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

void window_init_with_file(GtkApplication *app, const char *path) {
    CualiAppState *state = g_new0 (CualiAppState, 1);
    state->current_document_id = -1;
    state->zoom_level = 1.0;
    state->map_selected_tag_id = -1;
    state->results_last_project_id = -1;
    state->results_dirty = TRUE;
    state->revision_last_project_id = -1;
    state->revision_dirty = TRUE;
    state->results_limit = 200;
    state->css_provider_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    state->cached_results = NULL;
    state->revision_current_filter_doc_id = -1;
    state->vim_enabled = g_getenv("CUALI_VIM_MODE") != NULL;

    // Inter es la fuente seleccionada para el programa

    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (provider, style_css);
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);

    /* Register bundled icons so AdwViewSwitcher can find them by name */
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    gtk_icon_theme_add_resource_path (icon_theme, "/org/cuali/icons");

    GtkWidget *window = adw_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Cuali GTK");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 800);
    g_signal_connect (window, "close-request", G_CALLBACK (on_window_close_request), state);
    g_signal_connect (window, "destroy", G_CALLBACK (on_window_destroy), state);

    state->root_stack = adw_view_stack_new ();
    gtk_widget_set_vexpand (state->root_stack, TRUE);
    gtk_widget_set_hexpand (state->root_stack, TRUE);
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (window), state->root_stack);

    /* --- Welcome Screen --- */
    GtkWidget *welcome_view = adw_toolbar_view_new ();
    adw_view_stack_add_named (ADW_VIEW_STACK (state->root_stack), welcome_view, "welcome");
    
    GtkWidget *welcome_header = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (welcome_view), welcome_header);

    GtkWidget *status_page = adw_status_page_new ();
    adw_status_page_set_title (ADW_STATUS_PAGE (status_page), "Welcome to Cuali");
    adw_status_page_set_icon_name (ADW_STATUS_PAGE (status_page), "org.cuali.CualiGTK");
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (welcome_view), status_page);

    GtkWidget *btns_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    adw_status_page_set_child (ADW_STATUS_PAGE (status_page), btns_box);

    GtkWidget *open_btn = gtk_button_new_with_label ("Open existing project");
    gtk_widget_add_css_class (open_btn, "suggested-action");
    gtk_widget_add_css_class (open_btn, "pill");
    gtk_widget_set_halign (open_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (open_btn, "Open an existing .cuali project file");
    g_signal_connect (open_btn, "clicked", G_CALLBACK (on_open_project_clicked), state);
    gtk_box_append (GTK_BOX (btns_box), open_btn);

    GtkWidget *new_btn = gtk_button_new_with_label ("Create new project");
    gtk_widget_add_css_class (new_btn, "pill");
    gtk_widget_set_halign (new_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (new_btn, "Start a new qualitative research project");
    g_signal_connect (new_btn, "clicked", G_CALLBACK (on_new_project_clicked), state);
    gtk_box_append (GTK_BOX (btns_box), new_btn);

    GtkWidget *recent_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top (recent_box, 30);
    gtk_box_append (GTK_BOX (btns_box), recent_box);
    
    GtkWidget *recent_label = gtk_label_new ("Recent projects");
    gtk_widget_add_css_class (recent_label, "sidebar-title");
    gtk_box_append (GTK_BOX (recent_box), recent_label);
    
    GtkWidget *recent_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_size_request (recent_scroll, 400, 200);
    gtk_widget_set_halign (recent_scroll, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (recent_box), recent_scroll);

    state->recent_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->recent_list, "sidebar-list");
    gtk_widget_add_css_class (state->recent_list, "boxed-list");
    g_signal_connect (state->recent_list, "row-selected", G_CALLBACK (on_recent_row_selected), state);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (recent_scroll), state->recent_list);

    populate_recent_list (state);

    /* --- Main App Content --- */
    GtkWidget *main_toolbar_view = adw_toolbar_view_new ();
    adw_view_stack_add_named (ADW_VIEW_STACK (state->root_stack), main_toolbar_view, "main");

    GtkWidget *toast_overlay = adw_toast_overlay_new ();
    state->toast_overlay = toast_overlay;
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (main_toolbar_view), toast_overlay);

    GtkWidget *main_view_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adw_toast_overlay_set_child (ADW_TOAST_OVERLAY (toast_overlay), main_view_box);

    GtkWidget *header_bar = adw_header_bar_new();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (main_toolbar_view), header_bar);
    
    GtkWidget *back_btn = create_resource_icon_button ("resource:///org/cuali/icons/scalable/actions/go-previous-symbolic.svg");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), back_btn);
    gtk_widget_set_tooltip_text (back_btn, "Go back to welcome screen");
    g_signal_connect (back_btn, "clicked", G_CALLBACK (on_back_to_welcome_clicked), state);

    GtkWidget *open_button = create_resource_icon_button ("resource:///org/cuali/icons/scalable/status/folder-open-symbolic.svg");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), open_button);
    gtk_widget_set_tooltip_text (open_button, "Open another project");
    g_signal_connect (open_button, "clicked", G_CALLBACK (on_open_project_clicked), state);

    state->vim_mode_label = gtk_label_new("");
    gtk_widget_set_valign(state->vim_mode_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(state->vim_mode_label, 12);
    gtk_widget_set_margin_end(state->vim_mode_label, 12);
    gtk_widget_set_visible(state->vim_mode_label, FALSE);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), state->vim_mode_label);

    GtkWidget *add_button = create_resource_icon_button ("resource:///org/cuali/icons/scalable/actions/list-add-symbolic.svg");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), add_button);
    gtk_widget_set_tooltip_text (add_button, "Import a new document (PDF or Text)");
    g_signal_connect (add_button, "clicked", G_CALLBACK (on_add_button_clicked), state);

    state->view_stack = adw_view_stack_new ();
    g_signal_connect (state->view_stack, "notify::visible-child", G_CALLBACK (on_view_stack_visible_child_changed), state);
    gtk_box_append (GTK_BOX (main_view_box), state->view_stack);
    gtk_widget_set_vexpand (state->view_stack, TRUE);
    
    GtkWidget *view_switcher_title = adw_view_switcher_title_new ();
    adw_view_switcher_title_set_stack (ADW_VIEW_SWITCHER_TITLE (view_switcher_title), ADW_VIEW_STACK (state->view_stack));
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), view_switcher_title);
    
    GtkWidget *switcher_bar = adw_view_switcher_bar_new ();
    adw_view_switcher_bar_set_stack (ADW_VIEW_SWITCHER_BAR (switcher_bar), ADW_VIEW_STACK (state->view_stack));
    gtk_box_append (GTK_BOX (main_view_box), switcher_bar);
    g_object_bind_property (view_switcher_title, "title-visible", switcher_bar, "reveal", G_BINDING_SYNC_CREATE);

    GtkWidget *hl_button = create_resource_icon_button ("resource:///org/cuali/icons/scalable/actions/format-text-underline-symbolic.svg");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), hl_button);
    gtk_widget_set_tooltip_text (hl_button, "Highlight selected text (Ctrl+B)");
    g_signal_connect (hl_button, "clicked", G_CALLBACK (on_highlight_button_clicked), state);
    
    state->edit_toggle = create_resource_icon_button ("resource:///org/cuali/icons/scalable/actions/document-edit-symbolic.svg");
    adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), state->edit_toggle);
    gtk_widget_set_tooltip_text (state->edit_toggle, "Toggle edit mode (Ctrl+E)");
    g_signal_connect (state->edit_toggle, "clicked", G_CALLBACK (on_edit_toggle_clicked), state);
    gtk_widget_set_visible (state->edit_toggle, TRUE);

    state->save_indicator = gtk_label_new (NULL);
    gtk_widget_set_visible (state->save_indicator, FALSE);
    gtk_widget_set_margin_start (state->save_indicator, 6);
    gtk_widget_set_margin_end (state->save_indicator, 6);
    // adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), state->save_indicator);
    /* Primary menu (gear) */
    GtkWidget *menu_btn = gtk_menu_button_new ();
    gtk_widget_add_css_class (menu_btn, "flat");
    
    GFile *menu_file = g_file_new_for_uri ("resource:///org/cuali/icons/scalable/actions/open-menu-symbolic.svg");
    GIcon *menu_gicon = g_file_icon_new (menu_file);
    g_object_unref (menu_file);
    GtkWidget *menu_img = gtk_image_new_from_gicon (menu_gicon);
    g_object_unref (menu_gicon);
    gtk_menu_button_set_child (GTK_MENU_BUTTON (menu_btn), menu_img);
    
    gtk_widget_set_tooltip_text (menu_btn, "Main menu");

    GtkWidget *menu_popover = gtk_popover_new ();
    gtk_popover_set_autohide(GTK_POPOVER(menu_popover), TRUE);
    GtkWidget *menu_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *about_item = gtk_button_new_with_label ("About Cuali");
    gtk_widget_set_halign (about_item, GTK_ALIGN_START);
    gtk_widget_add_css_class(about_item, "flat");
    gtk_widget_set_tooltip_text (about_item, "View application information");
    g_signal_connect (about_item, "clicked", G_CALLBACK (on_about_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), about_item);

    GtkWidget *shortcuts_item = gtk_button_new_with_label ("Keyboard Shortcuts");
    gtk_widget_set_halign (shortcuts_item, GTK_ALIGN_START);
    gtk_widget_add_css_class(shortcuts_item, "flat");
    gtk_widget_set_tooltip_text (shortcuts_item, "View keyboard shortcuts");
    g_signal_connect (shortcuts_item, "clicked", G_CALLBACK (on_shortcuts_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), shortcuts_item);

    GtkWidget *export_item = gtk_button_new_with_label ("Export highlights (CSV)");
    gtk_widget_set_halign (export_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_item, "flat");
    g_signal_connect (export_item, "clicked", G_CALLBACK (on_export_csv_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_item);

    GtkWidget *export_cb_item = gtk_button_new_with_label ("Export codebook (CSV)");
    gtk_widget_set_halign (export_cb_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_cb_item, "flat");
    g_signal_connect (export_cb_item, "clicked", G_CALLBACK (on_export_codebook_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_cb_item);

    GtkWidget *export_thm_item = gtk_button_new_with_label ("Export thematic table (HTML) 🛈");
    gtk_widget_set_tooltip_text(export_thm_item, "Calcula la frecuencia real de citas únicas por tema, evitando contar un mismo párrafo dos veces si tiene múltiples etiquetas del mismo tema.");
    gtk_widget_set_halign (export_thm_item, GTK_ALIGN_START);
    gtk_widget_add_css_class (export_thm_item, "flat");
    g_signal_connect (export_thm_item, "clicked", G_CALLBACK (on_export_thematic_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), export_thm_item);

    GtkWidget *clear_tags_item = gtk_button_new_with_label ("Clear all highlights & tags");
    gtk_widget_set_halign (clear_tags_item, GTK_ALIGN_START);
    gtk_widget_add_css_class(clear_tags_item, "destructive-action");
    gtk_widget_add_css_class(clear_tags_item, "flat");
    g_signal_connect (clear_tags_item, "clicked", G_CALLBACK (on_clear_tags_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), clear_tags_item);

    GtkWidget *clear_proj_item = gtk_button_new_with_label ("Clear project completely");
    gtk_widget_set_halign (clear_proj_item, GTK_ALIGN_START);
    gtk_widget_add_css_class(clear_proj_item, "destructive-action");
    gtk_widget_add_css_class(clear_proj_item, "flat");
    g_signal_connect (clear_proj_item, "clicked", G_CALLBACK (on_clear_project_clicked), state);
    gtk_box_append (GTK_BOX (menu_box), clear_proj_item);

    gtk_popover_set_child (GTK_POPOVER (menu_popover), menu_box);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (menu_btn), menu_popover);
    adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), menu_btn);

    /* --- Pestaña 1: Información --- */
    GtkWidget *info_page = adw_preferences_page_new ();
    AdwViewStackPage *page;
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), info_page, "info", "Information");
    adw_view_stack_page_set_icon_name (page, "cuali-info-symbolic");
    
    GtkWidget *info_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (info_group), "Project metadata");
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (info_page), ADW_PREFERENCES_GROUP (info_group));
    
    GtkWidget *name_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Project name");
    state->project_name_entry = name_row;
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (info_group), name_row);
    g_signal_connect (name_row, "changed", G_CALLBACK (on_project_info_changed), state);

    GtkWidget *desc_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (desc_row), "Description");
    state->project_desc_entry = desc_row;
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (info_group), desc_row);
    g_signal_connect (desc_row, "changed", G_CALLBACK (on_project_info_changed), state);


    GtkWidget *stats_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (stats_group), "Statistics");
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (info_page), ADW_PREFERENCES_GROUP (stats_group));

    state->stat_docs_row       = adw_action_row_new ();
    state->stat_tags_row       = adw_action_row_new ();
    state->stat_highlights_row = adw_action_row_new ();
    state->stat_coverage_row   = adw_action_row_new ();

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (state->stat_docs_row),       "Documents");
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (state->stat_tags_row),       "Tags");
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (state->stat_highlights_row), "Coded segments");
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (state->stat_coverage_row),   "Coverage");

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), state->stat_docs_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), state->stat_tags_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), state->stat_highlights_row);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (stats_group), state->stat_coverage_row);

    /* --- Pestaña 2: Documentos --- */
    GtkWidget *analysis_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), analysis_box, "analysis", "Documents");
    adw_view_stack_page_set_icon_name (page, "cuali-docs-symbolic");
    
    GtkWidget *split_view = adw_overlay_split_view_new();
    gtk_box_append(GTK_BOX(analysis_box), split_view);
    gtk_widget_set_hexpand(split_view, TRUE);

    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (sidebar_box, "sidebar");
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_view), sidebar_box);
    
    /* ── Document filter ── */
    state->doc_filter_entry = gtk_search_entry_new ();
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (state->doc_filter_entry), "Filter documents…");
    gtk_widget_set_margin_start (state->doc_filter_entry, 8);
    gtk_widget_set_margin_end (state->doc_filter_entry, 8);
    gtk_widget_set_margin_top (state->doc_filter_entry, 8);
    gtk_widget_set_margin_bottom (state->doc_filter_entry, 4);
    gtk_box_append (GTK_BOX (sidebar_box), state->doc_filter_entry);

    GtkWidget *scrolled_docs = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled_docs, TRUE);
    gtk_box_append (GTK_BOX (sidebar_box), scrolled_docs);
    state->doc_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->doc_list, "sidebar-list");
    gtk_widget_add_css_class (state->doc_list, "boxed-list");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_docs), state->doc_list);
    g_signal_connect (state->doc_list, "row-selected", G_CALLBACK (on_doc_row_selected), state);

    /* [REORGANIZACIÓN ETIQUETAS] */
    GtkWidget *tag_entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(tag_entry_box, 20);
    gtk_widget_set_margin_bottom(tag_entry_box, 8);
    gtk_widget_set_margin_start(tag_entry_box, 12);
    gtk_widget_set_margin_end(tag_entry_box, 12);
    gtk_box_append(GTK_BOX(sidebar_box), tag_entry_box);

    GtkWidget *tag_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(tag_entry), "+ New tag…");
    gtk_widget_set_hexpand(tag_entry, TRUE);
    g_signal_connect(tag_entry, "activate", G_CALLBACK(on_sidebar_new_tag_activated), state);
    gtk_box_append(GTK_BOX(tag_entry_box), tag_entry);

    GtkWidget *scrolled_tags = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled_tags, TRUE);
    gtk_box_append (GTK_BOX (sidebar_box), scrolled_tags);

    state->tag_tree_store = gtk_tree_store_new(4, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    state->tag_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->tag_tree_store));
    g_object_unref(state->tag_tree_store);
    
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(state->tag_tree_view), FALSE);
    gtk_widget_add_css_class(state->tag_tree_view, "sidebar-list");
    
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, renderer, tag_tree_cell_data_func, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tag_tree_view), col);
    
    g_signal_connect(state->tag_tree_view, "row-activated", G_CALLBACK(on_tag_tree_row_activated), state);
    
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_tags), state->tag_tree_view);

    /* Content view with toolbar */
    GtkWidget *doc_content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_view), doc_content_vbox);

    GtkWidget *banner = adw_banner_new("Selecciona texto para etiquetar un segmento");
    adw_banner_set_revealed(ADW_BANNER(banner), TRUE);
    gtk_box_append(GTK_BOX(doc_content_vbox), banner);

    GtkWidget *doc_toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (doc_toolbar, "toolbar");
    gtk_widget_set_margin_start (doc_toolbar, 12);
    gtk_widget_set_margin_end (doc_toolbar, 12);
    gtk_widget_set_margin_top (doc_toolbar, 8);
    gtk_widget_set_margin_bottom (doc_toolbar, 8);
    gtk_box_append (GTK_BOX (doc_content_vbox), doc_toolbar);

    GtkWidget *doc_toggle_btn = gtk_toggle_button_new ();
    GtkWidget *doc_toggle_icon = gtk_image_new_from_icon_name ("sidebar-show-symbolic");
    gtk_button_set_child (GTK_BUTTON (doc_toggle_btn), doc_toggle_icon);
    gtk_widget_add_css_class (doc_toggle_btn, "flat");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (doc_toggle_btn), TRUE);
    gtk_widget_set_tooltip_text (doc_toggle_btn, "Show/hide documents sidebar");
    gtk_box_append (GTK_BOX (doc_toolbar), doc_toggle_btn);
    g_signal_connect (doc_toggle_btn, "toggled", G_CALLBACK (on_doc_sidebar_toggle), split_view);

    GtkWidget *zoom_out_btn = gtk_button_new_from_icon_name ("zoom-out-symbolic");
    gtk_widget_add_css_class (zoom_out_btn, "flat");
    gtk_widget_set_margin_start (zoom_out_btn, 12);
    gtk_widget_set_tooltip_text (zoom_out_btn, "Zoom out");
    gtk_box_append (GTK_BOX (doc_toolbar), zoom_out_btn);
    g_signal_connect (zoom_out_btn, "clicked", G_CALLBACK (on_zoom_out_clicked), state);

    GtkWidget *zoom_in_btn = gtk_button_new_from_icon_name ("zoom-in-symbolic");
    gtk_widget_add_css_class (zoom_in_btn, "flat");
    gtk_widget_set_tooltip_text (zoom_in_btn, "Zoom in");
    gtk_box_append (GTK_BOX (doc_toolbar), zoom_in_btn);
    g_signal_connect (zoom_in_btn, "clicked", G_CALLBACK (on_zoom_in_clicked), state);

    GtkWidget *doc_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (doc_content_vbox), doc_sep);

    /* ── Search bar (Ctrl+F) ── */
    state->search_bar = gtk_search_bar_new ();
    gtk_widget_set_child_visible (state->search_bar, FALSE);
    GtkWidget *search_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start (search_box, 12);
    gtk_widget_set_margin_end (search_box, 12);
    gtk_widget_set_margin_top (search_box, 6);
    gtk_widget_set_margin_bottom (search_box, 6);

    state->search_entry = gtk_search_entry_new ();
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (state->search_entry), "Search in document…");
    gtk_widget_set_hexpand (state->search_entry, TRUE);
    gtk_search_bar_set_child (GTK_SEARCH_BAR (state->search_bar), search_box);
    gtk_search_bar_connect_entry (GTK_SEARCH_BAR (state->search_bar), GTK_EDITABLE (state->search_entry));
    gtk_box_append (GTK_BOX (search_box), state->search_entry);

    GtkWidget *search_prev = gtk_button_new_from_icon_name ("go-up-symbolic");
    gtk_widget_add_css_class (search_prev, "flat");
    gtk_widget_set_tooltip_text (search_prev, "Previous match (Shift+Enter)");
    g_signal_connect (search_prev, "clicked", G_CALLBACK (on_search_prev_clicked), state);
    gtk_box_append (GTK_BOX (search_box), search_prev);

    GtkWidget *search_next = gtk_button_new_from_icon_name ("go-down-symbolic");
    gtk_widget_add_css_class (search_next, "flat");
    gtk_widget_set_tooltip_text (search_next, "Next match (Enter)");
    g_signal_connect (search_next, "clicked", G_CALLBACK (on_search_next_clicked), state);
    gtk_box_append (GTK_BOX (search_box), search_next);

    gtk_box_append (GTK_BOX (doc_content_vbox), state->search_bar);

    /* Search key controller */
    GtkEventController *search_key = gtk_event_controller_key_new ();
    gtk_widget_add_controller (state->search_entry, search_key);
    g_signal_connect (search_key, "key-pressed", G_CALLBACK (on_search_key_pressed), state);

    g_signal_connect (state->search_entry, "search-changed", G_CALLBACK (on_search_entry_changed), state);

    /* ── Content scroll with paper ── */
    GtkWidget *content_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (content_scroll, TRUE);
    gtk_box_append (GTK_BOX (doc_content_vbox), content_scroll);

    /* Drop target for files */
    GtkDropTarget *drop_target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect (drop_target, "drop", G_CALLBACK (on_drop), state);
    gtk_widget_add_controller (content_scroll, GTK_EVENT_CONTROLLER (drop_target));

    GtkWidget *paper_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (paper_box, "paper-sheet");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_scroll), paper_box);
    GtkWidget *text_view = gtk_text_view_new();
    gtk_widget_add_css_class (text_view, "document-view");
    state->text_view = text_view;
    gtk_widget_set_focusable(text_view, FALSE);
    update_zoom(state);
    
    GtkEventController *vim_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(vim_keys, GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(state->window, vim_keys);
    g_signal_connect(vim_keys, "key-pressed", G_CALLBACK(on_vim_key_pressed), state);
    state->vim_mode = VIM_NORMAL;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_enable_undo (buffer, TRUE);
    g_signal_connect (buffer, "changed",      G_CALLBACK (on_buffer_changed), state);
    g_signal_connect (buffer, "mark-set",     G_CALLBACK (on_cursor_moved),  state);
    /* Offset tracking for edits AND undo/redo — must fire on both */
    g_signal_connect (buffer, "insert-text",  G_CALLBACK (on_insert_text),   state);
    g_signal_connect (buffer, "delete-range", G_CALLBACK (on_delete_range),  state);
    
    /* Create dialogs once */
    state->highlight_popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(state->highlight_popover), TRUE);
    gtk_popover_set_position(GTK_POPOVER(state->highlight_popover), GTK_POS_BOTTOM);
    gtk_text_view_add_overlay(GTK_TEXT_VIEW(text_view), state->highlight_popover, 0, 0);
    build_highlight_dialog(state);

    state->highlight_selector = GTK_WIDGET(adw_dialog_new());
    build_highlight_selector_dialog(state);

    GtkGesture *click_gesture = gtk_gesture_click_new ();
    g_signal_connect (click_gesture, "pressed", G_CALLBACK (on_text_view_clicked), state);
    g_signal_connect (click_gesture, "released", G_CALLBACK (on_text_view_released), state);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click_gesture), GTK_PHASE_BUBBLE);
    gtk_widget_add_controller (text_view, GTK_EVENT_CONTROLLER (click_gesture));

    /* Create search match tags */
    {
        GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        GdkRGBA sb; gdk_rgba_parse (&sb, "#f9f06b"); sb.alpha = 0.3;
        state->search_match_tag = gtk_text_buffer_create_tag (buf, NULL, "background-rgba", &sb, NULL);
        GdkRGBA cb; gdk_rgba_parse (&cb, "#ff7800"); cb.alpha = 0.5;
        state->search_current_tag = gtk_text_buffer_create_tag (buf, NULL, "background-rgba", &cb, NULL);
    }

    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 100);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 100);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);
    
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *cursor_area = gtk_drawing_area_new();
    gtk_widget_set_can_target(cursor_area, FALSE); /* no intercepta clicks */
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(cursor_area), draw_vim_cursor, state, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), text_view);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), cursor_area);
    state->vim_cursor_area = cursor_area;
    
    gtk_box_append (GTK_BOX (paper_box), overlay);

    /* ── Status bar ── */
    GtkWidget *status_bar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start (status_bar_box, 12);
    gtk_widget_set_margin_end (status_bar_box, 12);
    gtk_widget_set_margin_top (status_bar_box, 2);
    gtk_widget_set_margin_bottom (status_bar_box, 4);

    state->status_label = gtk_label_new ("");
    gtk_widget_set_halign (state->status_label, GTK_ALIGN_END);
    gtk_widget_set_hexpand (state->status_label, TRUE);
    gtk_widget_set_opacity (state->status_label, 0.6);
    gtk_label_set_xalign (GTK_LABEL (state->status_label), 1.0f);
    gtk_box_append (GTK_BOX (status_bar_box), state->status_label);
    gtk_box_append (GTK_BOX (doc_content_vbox), status_bar_box);

    /* --- Pestaña: Revisión --- */
    GtkWidget *revision_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), revision_box, "revision", "Revision");
    adw_view_stack_page_set_icon_name (page, "document-edit-symbolic");

    GtkWidget *rev_split_view = adw_overlay_split_view_new();
    gtk_box_append(GTK_BOX(revision_box), rev_split_view);
    gtk_widget_set_hexpand(rev_split_view, TRUE);

    // Sidebar: list of highlights to review
    GtkWidget *rev_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (rev_sidebar, "sidebar");
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(rev_split_view), rev_sidebar);

    GtkWidget *rev_sidebar_title = gtk_label_new("Citas a Revisar");
    gtk_widget_add_css_class (rev_sidebar_title, "sidebar-title");
    gtk_widget_set_halign (rev_sidebar_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start (rev_sidebar_title, 12);
    gtk_box_append(GTK_BOX(rev_sidebar), rev_sidebar_title);

    GtkWidget *filter_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start (filter_hbox, 8);
    gtk_widget_set_margin_end (filter_hbox, 8);
    gtk_widget_set_margin_top (filter_hbox, 8);
    gtk_widget_set_margin_bottom (filter_hbox, 4);
    gtk_box_append (GTK_BOX (rev_sidebar), filter_hbox);

    state->revision_doc_filter_btn = gtk_menu_button_new ();
    gtk_menu_button_set_label (GTK_MENU_BUTTON (state->revision_doc_filter_btn), "All documents");
    gtk_widget_set_hexpand (state->revision_doc_filter_btn, TRUE);
    gtk_box_append (GTK_BOX (filter_hbox), state->revision_doc_filter_btn);

    GtkWidget *filter_popover = gtk_popover_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (state->revision_doc_filter_btn), filter_popover);
    
    GtkWidget *filter_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (filter_scroll), 300);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (filter_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_popover_set_child (GTK_POPOVER (filter_popover), filter_scroll);

    state->revision_doc_filter_list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->revision_doc_filter_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class (state->revision_doc_filter_list, "navigation-sidebar");
    g_signal_connect (state->revision_doc_filter_list, "row-activated", G_CALLBACK (on_revision_doc_filter_selected), state);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (filter_scroll), state->revision_doc_filter_list);

    state->revision_sidebar_search_entry = GTK_WIDGET (gtk_search_entry_new ());
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (state->revision_sidebar_search_entry), "Filtrar citas…");
    gtk_widget_set_margin_start (state->revision_sidebar_search_entry, 8);
    gtk_widget_set_margin_end (state->revision_sidebar_search_entry, 8);
    gtk_widget_set_margin_top (state->revision_sidebar_search_entry, 4);
    gtk_widget_set_margin_bottom (state->revision_sidebar_search_entry, 4);
    g_signal_connect (state->revision_sidebar_search_entry, "search-changed", G_CALLBACK (on_revision_sidebar_search_changed), state);
    gtk_box_append (GTK_BOX (rev_sidebar), state->revision_sidebar_search_entry);

    GtkWidget *rev_sidebar_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (rev_sidebar_scroll, TRUE);
    gtk_box_append (GTK_BOX (rev_sidebar), rev_sidebar_scroll);

    state->revision_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->revision_list, "sidebar-list");
    gtk_widget_add_css_class (state->revision_list, "boxed-list");
    gtk_list_box_set_filter_func (GTK_LIST_BOX (state->revision_list), revision_sidebar_filter_func, state, NULL);
    g_signal_connect (state->revision_list, "row-selected", G_CALLBACK (on_revision_row_selected), state);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (rev_sidebar_scroll), state->revision_list);

    // Right Pane Content Box
    GtkWidget *rev_content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(rev_split_view), rev_content_vbox);

    // Content Mini Toolbar
    GtkWidget *rev_toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (rev_toolbar, "toolbar");
    gtk_widget_set_margin_start (rev_toolbar, 12);
    gtk_widget_set_margin_end (rev_toolbar, 12);
    gtk_widget_set_margin_top (rev_toolbar, 8);
    gtk_widget_set_margin_bottom (rev_toolbar, 8);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_toolbar);

    GtkWidget *rev_toggle_btn = gtk_toggle_button_new ();
    GtkWidget *rev_toggle_icon = gtk_image_new_from_icon_name ("sidebar-show-symbolic");
    gtk_button_set_child (GTK_BUTTON (rev_toggle_btn), rev_toggle_icon);
    gtk_widget_add_css_class (rev_toggle_btn, "flat");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rev_toggle_btn), TRUE);
    gtk_widget_set_tooltip_text (rev_toggle_btn, "Mostrar/Ocultar barra lateral");
    g_signal_connect (rev_toggle_btn, "toggled", G_CALLBACK (on_doc_sidebar_toggle), rev_split_view);
    gtk_box_append (GTK_BOX (rev_toolbar), rev_toggle_btn);

    GtkWidget *rev_title_label = gtk_label_new ("Espacio de Trabajo de Revisión");
    gtk_widget_add_css_class (rev_title_label, "heading");
    gtk_widget_set_margin_start (rev_title_label, 12);
    gtk_box_append (GTK_BOX (rev_toolbar), rev_title_label);

    GtkWidget *rev_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_sep);

    // 1. Scrolled context text view at the top (with white/paper sheet card background)
    GtkWidget *rev_scroll_text = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (rev_scroll_text, TRUE);
    gtk_widget_set_margin_start (rev_scroll_text, 16);
    gtk_widget_set_margin_end (rev_scroll_text, 16);
    gtk_widget_set_margin_top (rev_scroll_text, 12);
    gtk_widget_set_margin_bottom (rev_scroll_text, 12);
    gtk_widget_add_css_class (rev_scroll_text, "card");
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_scroll_text);

    state->revision_text_view = gtk_text_view_new ();
    gtk_widget_add_css_class (state->revision_text_view, "document-view");
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->revision_text_view), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (state->revision_text_view), GTK_WRAP_WORD);
    gtk_widget_set_margin_start (state->revision_text_view, 12);
    gtk_widget_set_margin_end (state->revision_text_view, 12);
    gtk_widget_set_margin_top (state->revision_text_view, 12);
    gtk_widget_set_margin_bottom (state->revision_text_view, 12);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (rev_scroll_text), state->revision_text_view);

    GtkTextBuffer *rev_buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->revision_text_view));
    state->revision_context_tag = gtk_text_buffer_create_tag (rev_buf, "revision_context",
                                                              "foreground", "#77767b",
                                                              "style", PANGO_STYLE_ITALIC,
                                                              NULL);
    state->revision_highlight_tag = gtk_text_buffer_create_tag (rev_buf, "revision_highlight",
                                                                "foreground", "white",
                                                                "weight", PANGO_WEIGHT_BOLD,
                                                                NULL);

    GtkWidget *rev_sep_mid = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_sep_mid);

    // 2. Horizontal container for: Shifter control & Tags (Left), Memo / Notas (Right)
    GtkWidget *rev_middle_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_start (rev_middle_hbox, 16);
    gtk_widget_set_margin_end (rev_middle_hbox, 16);
    gtk_widget_set_margin_top (rev_middle_hbox, 8);
    gtk_widget_set_margin_bottom (rev_middle_hbox, 8);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_middle_hbox);

    // Left Column: Shift buttons & tags list
    GtkWidget *rev_left_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand (rev_left_vbox, TRUE);
    gtk_box_append (GTK_BOX (rev_middle_hbox), rev_left_vbox);

    // Shifter Bounds Control Box
    GtkWidget *rev_bounds_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_halign (rev_bounds_box, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (rev_left_vbox), rev_bounds_box);

    // Start boundary box
    GtkWidget *rev_start_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_start = gtk_label_new ("Límite de Inicio");
    gtk_widget_add_css_class (lbl_start, "dim-label");
    gtk_box_append (GTK_BOX (rev_start_vbox), lbl_start);
    GtkWidget *start_btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *btn_start_b = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_tooltip_text (btn_start_b, "Expandir límite de inicio");
    g_signal_connect (btn_start_b, "clicked", G_CALLBACK (on_revision_start_back_clicked), state);
    GtkWidget *btn_start_f = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_tooltip_text (btn_start_f, "Contraer límite de inicio");
    g_signal_connect (btn_start_f, "clicked", G_CALLBACK (on_revision_start_forward_clicked), state);
    gtk_box_append (GTK_BOX (start_btn_row), btn_start_b);
    gtk_box_append (GTK_BOX (start_btn_row), btn_start_f);
    gtk_box_append (GTK_BOX (rev_start_vbox), start_btn_row);
    gtk_box_append (GTK_BOX (rev_bounds_box), rev_start_vbox);

    // End boundary box
    GtkWidget *rev_end_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_end = gtk_label_new ("Límite de Fin");
    gtk_widget_add_css_class (lbl_end, "dim-label");
    gtk_box_append (GTK_BOX (rev_end_vbox), lbl_end);
    GtkWidget *end_btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *btn_end_b = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_tooltip_text (btn_end_b, "Contraer límite de fin");
    g_signal_connect (btn_end_b, "clicked", G_CALLBACK (on_revision_end_back_clicked), state);
    GtkWidget *btn_end_f = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_tooltip_text (btn_end_f, "Expandir límite de fin");
    g_signal_connect (btn_end_f, "clicked", G_CALLBACK (on_revision_end_forward_clicked), state);
    gtk_box_append (GTK_BOX (end_btn_row), btn_end_b);
    gtk_box_append (GTK_BOX (end_btn_row), btn_end_f);
    gtk_box_append (GTK_BOX (rev_end_vbox), end_btn_row);
    gtk_box_append (GTK_BOX (rev_bounds_box), rev_end_vbox);

    // Tags Section Heading
    GtkWidget *lbl_tags_header = gtk_label_new ("Etiquetas asociadas");
    gtk_widget_add_css_class (lbl_tags_header, "dim-label");
    gtk_widget_set_halign (lbl_tags_header, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (rev_left_vbox), lbl_tags_header);

    // Tag search/create box
    GtkWidget *rev_tag_entry_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append (GTK_BOX (rev_left_vbox), rev_tag_entry_hbox);

    state->revision_tag_search_entry = GTK_WIDGET (gtk_search_entry_new ());
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (state->revision_tag_search_entry), "Buscar etiqueta…");
    gtk_widget_set_hexpand (state->revision_tag_search_entry, TRUE);
    gtk_box_append (GTK_BOX (rev_tag_entry_hbox), state->revision_tag_search_entry);

    state->revision_tag_new_entry = GTK_WIDGET (gtk_entry_new ());
    gtk_entry_set_placeholder_text (GTK_ENTRY (state->revision_tag_new_entry), "+ Nueva etiqueta…");
    gtk_widget_set_hexpand (state->revision_tag_new_entry, TRUE);
    g_signal_connect (state->revision_tag_new_entry, "activate", G_CALLBACK (on_revision_new_tag_activated), state);
    gtk_box_append (GTK_BOX (rev_tag_entry_hbox), state->revision_tag_new_entry);

    // Tag list scrolled FlowBox
    GtkWidget *rev_tags_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (rev_tags_scroll, TRUE);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (rev_tags_scroll), 120);
    gtk_widget_add_css_class (rev_tags_scroll, "card");
    gtk_box_append (GTK_BOX (rev_left_vbox), rev_tags_scroll);

    state->revision_flow_box = gtk_flow_box_new ();
    gtk_widget_set_valign (state->revision_flow_box, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (state->revision_flow_box), 12);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (state->revision_flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (state->revision_flow_box), 12);
    gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (state->revision_flow_box), 8);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (rev_tags_scroll), state->revision_flow_box);

    g_signal_connect (state->revision_tag_search_entry, "search-changed", G_CALLBACK (on_dialog_tag_search_changed), state->revision_flow_box);

    // Right Column: Memo editor
    GtkWidget *rev_right_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand (rev_right_vbox, TRUE);
    gtk_box_append (GTK_BOX (rev_middle_hbox), rev_right_vbox);

    GtkWidget *lbl_memo_header = gtk_label_new ("Notas / Memos de investigación");
    gtk_widget_add_css_class (lbl_memo_header, "dim-label");
    gtk_widget_set_halign (lbl_memo_header, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (rev_right_vbox), lbl_memo_header);

    GtkWidget *rev_memo_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (rev_memo_scroll, TRUE);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (rev_memo_scroll), 160);
    gtk_widget_add_css_class (rev_memo_scroll, "card");
    gtk_box_append (GTK_BOX (rev_right_vbox), rev_memo_scroll);

    state->revision_memo_view = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (state->revision_memo_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (state->revision_memo_view), 12);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (state->revision_memo_view), 12);
    gtk_text_view_set_top_margin (GTK_TEXT_VIEW (state->revision_memo_view), 12);
    gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (state->revision_memo_view), 12);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (rev_memo_scroll), state->revision_memo_view);

    GtkWidget *rev_sep_bot = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_sep_bot);

    // 3. Action bar at the bottom
    GtkWidget *rev_action_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start (rev_action_hbox, 16);
    gtk_widget_set_margin_end (rev_action_hbox, 16);
    gtk_widget_set_margin_top (rev_action_hbox, 8);
    gtk_widget_set_margin_bottom (rev_action_hbox, 8);
    gtk_box_append (GTK_BOX (rev_content_vbox), rev_action_hbox);

    // Previous / Next highlights buttons
    GtkWidget *rev_nav_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append (GTK_BOX (rev_action_hbox), rev_nav_box);

    state->revision_btn_prev = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_tooltip_text (state->revision_btn_prev, "Ir al destaque anterior");
    gtk_widget_set_sensitive (state->revision_btn_prev, FALSE);
    g_signal_connect (state->revision_btn_prev, "clicked", G_CALLBACK (on_revision_prev_clicked), state);
    gtk_box_append (GTK_BOX (rev_nav_box), state->revision_btn_prev);

    state->revision_btn_next = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_tooltip_text (state->revision_btn_next, "Ir al siguiente destaque");
    gtk_widget_set_sensitive (state->revision_btn_next, FALSE);
    g_signal_connect (state->revision_btn_next, "clicked", G_CALLBACK (on_revision_next_clicked), state);
    gtk_box_append (GTK_BOX (rev_nav_box), state->revision_btn_next);

    // Action right (Guardar)
    GtkWidget *rev_save_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand (rev_save_box, TRUE);
    gtk_widget_set_halign (rev_save_box, GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (rev_action_hbox), rev_save_box);

    GtkWidget *rev_save_btn = gtk_button_new_with_label ("Guardar");
    gtk_widget_add_css_class (rev_save_btn, "suggested-action");
    gtk_widget_set_size_request (rev_save_btn, 140, -1);
    g_signal_connect (rev_save_btn, "clicked", G_CALLBACK (on_revision_save_clicked), state);
    gtk_box_append (GTK_BOX (rev_save_box), rev_save_btn);

    /* --- Pestaña 3: Resultados --- */
    GtkWidget *results_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), results_box, "results", "Results");
    adw_view_stack_page_set_icon_name (page, "cuali-results-symbolic");

    /* 5. Visualizations View */
    GtkWidget *viz_view = create_visualizations_view(state);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), viz_view, "visualizations", "Visualizations");
    adw_view_stack_page_set_icon_name (page, "view-grid-symbolic");

    GtkWidget *res_split_view = adw_overlay_split_view_new();
    gtk_box_append(GTK_BOX(results_box), res_split_view);
    gtk_widget_set_hexpand(res_split_view, TRUE);

    GtkWidget *res_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (res_sidebar, "sidebar");
    gtk_widget_add_css_class (res_sidebar, "background");
    gtk_widget_set_size_request (res_sidebar, 250, -1);
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_sidebar);

    GtkWidget *stats_title = gtk_label_new("Statistics");
    gtk_widget_add_css_class (stats_title, "sidebar-title");
    gtk_widget_set_halign (stats_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start (stats_title, 12);
    gtk_box_append(GTK_BOX(res_sidebar), stats_title);

    GtkWidget *res_tag_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (res_tag_scroll, TRUE);
    gtk_box_append (GTK_BOX (res_sidebar), res_tag_scroll);
    state->results_tag_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->tag_tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(state->results_tag_tree_view), FALSE);
    gtk_widget_add_css_class(state->results_tag_tree_view, "sidebar-list");
    
    GtkTreeViewColumn *col_res = gtk_tree_view_column_new();
    GtkCellRenderer *renderer_res = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col_res, renderer_res, TRUE);
    gtk_tree_view_column_set_cell_data_func(col_res, renderer_res, tag_tree_cell_data_func, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->results_tag_tree_view), col_res);
    
    g_signal_connect(state->results_tag_tree_view, "cursor-changed", G_CALLBACK(on_results_tag_tree_cursor_changed), state);
    
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (res_tag_scroll), state->results_tag_tree_view);

    GtkGesture *res_click = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (res_click), GDK_BUTTON_SECONDARY);
    g_signal_connect (res_click, "pressed", G_CALLBACK (on_results_tag_tree_pressed), state);
    gtk_widget_add_controller (state->results_tag_tree_view, GTK_EVENT_CONTROLLER (res_click));

    state->results_tag_popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(state->results_tag_popover), FALSE);
    gtk_widget_set_parent(state->results_tag_popover, state->results_tag_tree_view);
    
    GtkWidget *res_pop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *btn_edit = gtk_button_new_with_label("Editar etiqueta...");
    gtk_widget_add_css_class(btn_edit, "flat");
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_results_tag_edit_clicked), state);
    gtk_box_append(GTK_BOX(res_pop_box), btn_edit);
    
    GtkWidget *btn_merge = gtk_button_new_with_label("Fusionar etiqueta...");
    gtk_widget_add_css_class(btn_merge, "flat");
    g_signal_connect(btn_merge, "clicked", G_CALLBACK(on_results_tag_merge_clicked), state);
    gtk_box_append(GTK_BOX(res_pop_box), btn_merge);
    
    gtk_popover_set_child(GTK_POPOVER(state->results_tag_popover), res_pop_box);

    /* Contenido: caja vertical con toolbar + lista */
    GtkWidget *res_content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    /* Mini toolbar con toggle del sidebar */
    GtkWidget *res_toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (res_toolbar, "toolbar");
    gtk_widget_set_margin_start (res_toolbar, 12);
    gtk_widget_set_margin_end (res_toolbar, 12);
    gtk_widget_set_margin_top (res_toolbar, 8);
    gtk_widget_set_margin_bottom (res_toolbar, 8);
    gtk_box_append (GTK_BOX (res_content_vbox), res_toolbar);

    GtkWidget *res_toggle_btn = gtk_toggle_button_new ();
    GtkWidget *toggle_icon = gtk_image_new_from_icon_name ("sidebar-show-symbolic");
    gtk_button_set_child (GTK_BUTTON (res_toggle_btn), toggle_icon);
    gtk_widget_add_css_class (res_toggle_btn, "flat");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (res_toggle_btn), TRUE);
    gtk_widget_set_tooltip_text (res_toggle_btn, "Show/hide filters sidebar");
    gtk_box_append (GTK_BOX (res_toolbar), res_toggle_btn);
    g_signal_connect (res_toggle_btn, "toggled",
                      G_CALLBACK (on_res_sidebar_toggle),
                      res_split_view);

    state->results_search_entry = gtk_search_entry_new ();
    gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (state->results_search_entry), "Search in results...");
    gtk_widget_set_margin_start (state->results_search_entry, 12);
    gtk_widget_set_hexpand (state->results_search_entry, TRUE);
    g_signal_connect (state->results_search_entry, "search-changed", G_CALLBACK (on_results_search_changed), state);
    gtk_box_append (GTK_BOX (res_toolbar), state->results_search_entry);
                      
    GtkWidget *res_zoom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (res_zoom_box, GTK_ALIGN_END);
    gtk_widget_set_hexpand (res_zoom_box, TRUE);
    
    GtkWidget *res_zoom_out = gtk_button_new_from_icon_name ("zoom-out-symbolic");
    gtk_widget_add_css_class (res_zoom_out, "flat");
    gtk_widget_set_tooltip_text (res_zoom_out, "Zoom out");
    g_signal_connect (res_zoom_out, "clicked", G_CALLBACK (on_zoom_out_clicked), state);
    gtk_box_append (GTK_BOX (res_zoom_box), res_zoom_out);
    
    GtkWidget *res_zoom_in = gtk_button_new_from_icon_name ("zoom-in-symbolic");
    gtk_widget_add_css_class (res_zoom_in, "flat");
    gtk_widget_set_tooltip_text (res_zoom_in, "Zoom in");
    g_signal_connect (res_zoom_in, "clicked", G_CALLBACK (on_zoom_in_clicked), state);
    gtk_box_append (GTK_BOX (res_zoom_box), res_zoom_in);

    gtk_box_append (GTK_BOX (res_toolbar), res_zoom_box);

    GtkWidget *res_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (res_content_vbox), res_sep);

    GtkWidget *res_content_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (res_content_scroll, TRUE);
    gtk_box_append (GTK_BOX (res_content_vbox), res_content_scroll);

    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_content_vbox);
    
    state->results_list = gtk_list_box_new ();
    gtk_list_box_set_filter_func (GTK_LIST_BOX (state->results_list), results_filter_func, state, NULL);
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->results_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class (state->results_list, "results-list");
    gtk_widget_set_margin_start (state->results_list, 24);
    gtk_widget_set_margin_end (state->results_list, 24);
    gtk_widget_set_margin_top (state->results_list, 20);
    gtk_widget_set_margin_bottom (state->results_list, 20);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (res_content_scroll), state->results_list);



    /* ── Global key shortcuts ── */
    GtkEventController *window_keys = gtk_event_controller_key_new ();
    gtk_widget_add_controller (window, window_keys);
    g_signal_connect (window_keys, "key-pressed", G_CALLBACK (on_key_pressed), state);

    gtk_window_present(GTK_WINDOW(window));

    if (path) {
        open_project_at_path (state, path, -1);
    }
}

void window_init(GtkApplication *app) {
    window_init_with_file(app, NULL);
}
