#include "window.h"
#include "importer.h"
#include "database.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>

const char *style_css = 
  "textview { font-family: 'Cantarell', sans-serif; font-size: 12pt; background-color: transparent; }"
  ".sidebar-list { background-color: transparent; }"
  ".sidebar-title { font-weight: bold; opacity: 0.5; font-size: 0.8rem; margin-top: 18px; margin-bottom: 6px; margin-left: 12px; }"
  ".document-view { background-color: @window_bg_color; border-radius: 12px; }"
  ".paper-sheet { background-color: transparent; border-radius: 0px; margin: 0px; transition: all 0.3s; }"
  ".tag-badge { background-color: @accent_bg_color; color: white; border-radius: 6px; padding: 2px 10px; font-size: 0.85rem; font-weight: 600; }"
  ".tag-count-badge { background-color: rgba(0,0,0,0.1); border-radius: 12px; padding: 1px 8px; font-size: 0.8rem; margin-right: 8px; }"
  ".result-card { background-color: @view_bg_color; border-radius: 12px; padding: 20px; border: 1px solid rgba(0,0,0,0.05); margin-bottom: 12px; }"
  ".result-snippet { font-style: italic; font-family: serif; font-size: 1.1rem; line-height: 1.6; margin-bottom: 12px; }"
  ".result-meta { font-size: 0.85rem; opacity: 0.6; margin-top: 8px; }";

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
static void refresh_results (CualiAppState *state);
static void refresh_tags (CualiAppState *state);

static const char *TAG_COLORS[] = {
    "#854d0e",  /* ámbar oscuro  */
    "#1e40af",  /* azul          */
    "#166534",  /* verde         */
    "#6b21a8",  /* violeta       */
    "#9f1239",  /* rojo oscuro   */
    "#0f6e56",  /* teal          */
    "#7c3aed",  /* púrpura       */
    "#b45309",  /* naranja       */
};
#define TAG_COLORS_COUNT 8

static void load_document (CualiAppState *state, int document_id, const char *name, const char *contents);
static void apply_highlight_tag (GtkTextBuffer *buffer, int hl_id, GtkTextIter *start, GtkTextIter *end);

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
    CualiAppState *state = (CualiAppState *)user_data;
    if (!state->selected_result_tag) return TRUE;

    const char *tags_str = g_object_get_data (G_OBJECT (row), "tags_str");
    if (!tags_str) return FALSE;

    char **tags = g_strsplit (tags_str, ", ", -1);
    gboolean match = FALSE;
    for (int i = 0; tags[i]; i++) {
        if (strcmp (tags[i], state->selected_result_tag) == 0) {
            match = TRUE;
            break;
        }
    }
    g_strfreev (tags);
    return match;
}

static void
on_res_sidebar_toggle (GtkToggleButton *btn, gpointer user_data)
{
    AdwOverlaySplitView *sv = ADW_OVERLAY_SPLIT_VIEW (user_data);
    adw_overlay_split_view_set_show_sidebar (sv, gtk_toggle_button_get_active (btn));
}

static void
on_results_tag_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->selected_result_tag) {
        g_free (state->selected_result_tag);
        state->selected_result_tag = NULL;
    }
    
    if (row) {
        GtkWidget *hbox = gtk_list_box_row_get_child (row);
        GtkWidget *child = gtk_widget_get_first_child (hbox);
        while (child) {
            if (GTK_IS_LABEL (child)) {
                state->selected_result_tag = g_strdup (gtk_label_get_text (GTK_LABEL (child)));
                break;
            }
            child = gtk_widget_get_next_sibling (child);
        }
    }
    
    gtk_list_box_invalidate_filter (GTK_LIST_BOX (state->results_list));
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

static void
populate_tag_dialog_list (CualiAppState *state, int highlight_id, GtkWidget *list_box)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (list_box)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (list_box), child);

    sqlite3_stmt *stmt_all = db_tags_get_all (state->current_project_id);
    if (stmt_all) {
        while (sqlite3_step (stmt_all) == SQLITE_ROW) {
            int tag_id = sqlite3_column_int (stmt_all, 0);
            const char *path = (const char *)sqlite3_column_text (stmt_all, 1);
            
            GtkWidget *row = adw_action_row_new ();
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), path);
            
            GtkWidget *check = gtk_check_button_new ();
            gtk_widget_set_valign (check, GTK_ALIGN_CENTER);
            adw_action_row_add_suffix (ADW_ACTION_ROW (row), check);
            adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), check);
            
            sqlite3_stmt *stmt_check = db_tags_get_for_highlight (highlight_id);
            if (stmt_check) {
                while (sqlite3_step (stmt_check) == SQLITE_ROW) {
                    if (tag_id == sqlite3_column_int (stmt_check, 0)) {
                        gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
                        break;
                    }
                }
                sqlite3_finalize (stmt_check);
            }
            
            gpointer *args = g_new (gpointer, 3);
            args[0] = state;
            args[1] = GINT_TO_POINTER (highlight_id);
            args[2] = GINT_TO_POINTER (tag_id);
            g_signal_connect_data (check, "toggled", G_CALLBACK (on_tag_check_toggled), args, (GClosureNotify)g_free, 0);
            
            gtk_list_box_append (GTK_LIST_BOX (list_box), row);
        }
        sqlite3_finalize (stmt_all);
    }
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
        
        GtkWidget *list_box = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "list_box"));
        populate_tag_dialog_list (state, highlight_id, list_box);
        refresh_results (state);
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
    GtkWidget *dialog = GTK_WIDGET (args[4]);

    const char *name = gtk_editable_get_text (GTK_EDITABLE (name_entry));
    const char *desc = gtk_editable_get_text (GTK_EDITABLE (desc_entry));

    if (name && *name) {
        db_tag_update (tag_id, name, desc ? desc : "");
        refresh_tags (state);
        refresh_results (state);
    }
    adw_dialog_force_close (ADW_DIALOG (dialog));
}

static void
show_tag_edit_dialog (CualiAppState *state, int tag_id)
{
    char *cur_path = NULL, *cur_desc = NULL, *cur_color = NULL;
    db_tag_get_info (tag_id, &cur_path, &cur_desc, &cur_color);

    GtkWidget *dialog = GTK_WIDGET (adw_dialog_new ());
    adw_dialog_set_title (ADW_DIALOG (dialog), "Editar Etiqueta");
    adw_dialog_set_content_width (ADW_DIALOG (dialog), 400);

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
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Nombre");
    if (cur_path) gtk_editable_set_text (GTK_EDITABLE (name_row), cur_path);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), name_row);

    GtkWidget *desc_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (desc_row), "Descripción");
    if (cur_desc) gtk_editable_set_text (GTK_EDITABLE (desc_row), cur_desc);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), desc_row);

    gtk_box_append (GTK_BOX (content_box), group);

    GtkWidget *save_btn = gtk_button_new_with_label ("Guardar");
    gtk_widget_add_css_class (save_btn, "suggested-action");
    gtk_widget_add_css_class (save_btn, "pill");
    gtk_widget_set_halign (save_btn, GTK_ALIGN_END);
    gtk_box_append (GTK_BOX (content_box), save_btn);

    gpointer *args = g_new (gpointer, 5);
    args[0] = state;
    args[1] = GINT_TO_POINTER (tag_id);
    args[2] = name_row;
    args[3] = desc_row;
    args[4] = dialog;
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

static void
show_tag_dialog (CualiAppState *state, int highlight_id)
{
    GtkWidget *dialog = GTK_WIDGET (adw_dialog_new ());
    adw_dialog_set_title (ADW_DIALOG (dialog), "Etiquetas");
    adw_dialog_set_content_width (ADW_DIALOG (dialog), 400);
    adw_dialog_set_content_height (ADW_DIALOG (dialog), 500);

    GtkWidget *toolbar_view = adw_toolbar_view_new ();
    GtkWidget *header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);
    
    GtkWidget *content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (content_box, 12);
    gtk_widget_set_margin_end (content_box, 12);
    gtk_widget_set_margin_top (content_box, 12);
    gtk_widget_set_margin_bottom (content_box, 12);

    GtkWidget *tag_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (tag_entry), "Nueva etiqueta...");
    gtk_box_append (GTK_BOX (content_box), tag_entry);

    GtkWidget *scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_box_append (GTK_BOX (content_box), scroll);

    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), content_box);
    
    GtkWidget *list_box = gtk_list_box_new ();
    gtk_widget_add_css_class (list_box, "boxed-list");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list_box);

    g_object_set_data (G_OBJECT (tag_entry), "highlight_id", GINT_TO_POINTER (highlight_id));
    g_object_set_data (G_OBJECT (tag_entry), "list_box", list_box);
    g_signal_connect (tag_entry, "activate", G_CALLBACK (on_dialog_new_tag_activated), state);

    populate_tag_dialog_list (state, highlight_id, list_box);

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
        const char *contents = g_object_get_data(G_OBJECT(row), "doc-contents");
        load_document(state, state->current_document_id, name, contents);
    }
    refresh_results(state);
}

static void on_popover_tag_toggled(GtkCheckButton *check, gpointer user_data)
{
    CualiAppState *state = g_object_get_data(G_OBJECT(check), "app-state");
    int tag_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(check), "tag-id"));
    bool active = gtk_check_button_get_active(check);

    if (state->active_highlight_id > 0) {
        if (active) db_highlight_link_tag(state->active_highlight_id, tag_id);
        else        db_highlight_unlink_tag(state->active_highlight_id, tag_id);
        refresh_results(state);
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
            apply_highlight_tag(buffer, hl_id, &start_iter, &end_iter);
            state->active_highlight_id = hl_id;
            refresh_results(state);
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

static void on_popover_closed (GtkPopover *popover, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->active_highlight_id <= 0) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->popover_memo_view));
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    char *memo = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
    db_highlight_set_memo(state->active_highlight_id, memo ? memo : "");
    g_free(memo);
}

static void show_popover_at(CualiAppState *state, double x, double y, int highlight_id)
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

    GdkRectangle rect = { (int)x, (int)y, 1, 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(state->highlight_popover), &rect);
    gtk_popover_popup(GTK_POPOVER(state->highlight_popover));
}

static void build_highlight_popover(CualiAppState *state)
{
    state->highlight_popover = gtk_popover_new();
    gtk_widget_set_parent(state->highlight_popover, state->text_view);
    gtk_popover_set_position(GTK_POPOVER(state->highlight_popover), GTK_POS_BOTTOM);
    /* GTK4 auto-flips position when popover would go off-screen */
    gtk_popover_set_has_arrow(GTK_POPOVER(state->highlight_popover), TRUE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(box, 340, -1);  /* min width */
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 300);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroll), TRUE);
    
    state->popover_tag_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->popover_tag_list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->popover_tag_list);
    gtk_box_append(GTK_BOX(box), scroll);
    
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);
    
    state->popover_delete_btn = gtk_button_new_with_label("Eliminar highlight");
    gtk_widget_add_css_class(state->popover_delete_btn, "destructive-action");
    gtk_widget_set_margin_start(state->popover_delete_btn, 8);
    gtk_widget_set_margin_end(state->popover_delete_btn, 8);
    gtk_widget_set_margin_top(state->popover_delete_btn, 8);
    gtk_widget_set_margin_bottom(state->popover_delete_btn, 8);
    g_signal_connect(state->popover_delete_btn, "clicked", G_CALLBACK(on_popover_delete_clicked), state);
    gtk_box_append(GTK_BOX(box), state->popover_delete_btn);

    /* ── Memo del investigador ── */
    GtkWidget *memo_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), memo_sep);

    GtkWidget *memo_label = gtk_label_new("Memo del investigador");
    gtk_widget_add_css_class(memo_label, "sidebar-title");
    gtk_widget_set_halign(memo_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(memo_label, 10);
    gtk_widget_set_margin_top(memo_label, 6);
    gtk_box_append(GTK_BOX(box), memo_label);

    GtkWidget *memo_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 80);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(memo_scroll), 160);
    gtk_widget_set_margin_start(memo_scroll, 8);
    gtk_widget_set_margin_end(memo_scroll, 8);
    gtk_widget_set_margin_bottom(memo_scroll, 8);

    state->popover_memo_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->popover_memo_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(state->popover_memo_view), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(state->popover_memo_view), 6);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(state->popover_memo_view), 4);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(state->popover_memo_view), 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memo_scroll), state->popover_memo_view);
    gtk_box_append(GTK_BOX(box), memo_scroll);

    gtk_popover_set_child(GTK_POPOVER(state->highlight_popover), box);
    g_signal_connect(state->highlight_popover, "closed",
                     G_CALLBACK(on_popover_closed), state);
}

static void
on_text_view_realized (GtkWidget *widget, gpointer user_data)
{
    build_highlight_popover((CualiAppState *)user_data);
}

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
    
    GSList *tags = gtk_text_iter_get_tags (&iter);
    for (GSList *l = tags; l; l = l->next) {
        int hl_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "highlight-id"));
        if (hl_id > 0) {
            show_popover_at(state, x, y, hl_id);
            break;
        }
    }
    g_slist_free (tags);
}

static void
on_text_view_released (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    CualiAppState *state = (CualiAppState *)user_data;
    if (state->is_editing) return;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
    GtkTextIter start, end;
    
    if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end)) return;
    
    GtkTextIter doc_start;
    gtk_text_buffer_get_start_iter(buffer, &doc_start);
    
    char *text_before_start = gtk_text_buffer_get_text(buffer, &doc_start, &start, FALSE);
    char *text_before_end = gtk_text_buffer_get_text(buffer, &doc_start, &end, FALSE);
    
    state->pending_start = text_before_start ? strlen(text_before_start) : 0;
    state->pending_end = text_before_end ? strlen(text_before_end) : 0;
    
    g_free(text_before_start);
    g_free(text_before_end);
                         
    show_popover_at(state, x, y, -1);
}

static void
apply_highlight_tag (GtkTextBuffer *buffer, int hl_id, GtkTextIter *start, GtkTextIter *end)
{
    char *hex_color = NULL;
    
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
    
    GdkRGBA rgba;
    if (hex_color && gdk_rgba_parse(&rgba, hex_color)) {
        rgba.alpha = 0.3;
    } else {
        gdk_rgba_parse(&rgba, "#f9f06b");
        rgba.alpha = 0.3;
    }
    
    GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, NULL, 
                                                 "background-rgba", &rgba, 
                                                 "foreground", "#1a1a1a", 
                                                 NULL);
    g_object_set_data (G_OBJECT (tag), "highlight-id", GINT_TO_POINTER (hl_id));
    gtk_text_buffer_apply_tag (buffer, tag, start, end);
    
    if (hex_color) g_free(hex_color);
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
    gtk_text_buffer_remove_tag_by_name (buffer, "highlight", NULL, NULL);
  } else {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
    
    db_document_update_contents (state->current_document_id, text);
    
    gtk_text_view_set_editable (GTK_TEXT_VIEW (state->text_view), FALSE);
    gtk_button_set_icon_name (GTK_BUTTON (state->edit_toggle), "document-edit-symbolic");
    
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

static void
refresh_results_tags (CualiAppState *state)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (state->results_tag_list)) != NULL)
    gtk_list_box_remove (GTK_LIST_BOX (state->results_tag_list), child);

  sqlite3_stmt *stmt = db_tags_get_stats (state->current_project_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
      int tag_id = sqlite3_column_int(stmt, 0);
      const char *path = (const char *)sqlite3_column_text (stmt, 1);
      const char *color = (const char *)sqlite3_column_text(stmt, 2);
      int count = sqlite3_column_int (stmt, 3);
      
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_widget_set_margin_start(box, 12);
      gtk_widget_set_margin_end(box, 12);
      gtk_widget_set_margin_top(box, 8);
      gtk_widget_set_margin_bottom(box, 8);
      
      GtkWidget *dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request(dot, 10, 10);
      char *css = g_strdup_printf("box { background-color: %s; border-radius: 5px; }", color ? color : "#888888");
      GtkCssProvider *provider = gtk_css_provider_new();
      gtk_css_provider_load_from_string(provider, css);
      gtk_style_context_add_provider(gtk_widget_get_style_context(dot),
                                     GTK_STYLE_PROVIDER(provider),
                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_free(css);
      gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
      gtk_box_append(GTK_BOX(box), dot);
      
      GtkWidget *label = gtk_label_new (path);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), label);
      
      if (count > 0) {
          char count_str[16];
          snprintf (count_str, sizeof(count_str), "%d", count);
          GtkWidget *count_label = gtk_label_new (count_str);
          gtk_widget_add_css_class (count_label, "numeric");
          gtk_widget_add_css_class (count_label, "dim-label");
          gtk_box_append (GTK_BOX (box), count_label);
      }
      
      GtkWidget *edit_btn = gtk_button_new_from_icon_name ("document-edit-symbolic");
      gtk_widget_add_css_class (edit_btn, "flat");
      gtk_widget_set_valign (edit_btn, GTK_ALIGN_CENTER);
      gtk_widget_set_tooltip_text (edit_btn, "Editar etiqueta");
      gpointer *ebargs = g_new (gpointer, 2);
      ebargs[0] = state; ebargs[1] = GINT_TO_POINTER(tag_id);
      g_signal_connect_data (edit_btn, "clicked", G_CALLBACK (on_tag_edit_btn_clicked),
                             ebargs, (GClosureNotify)g_free, 0);
      gtk_box_append (GTK_BOX (box), edit_btn);
      
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data(G_OBJECT(row), "tag-id", GINT_TO_POINTER(tag_id));
      gtk_list_box_append (GTK_LIST_BOX (state->results_tag_list), row);
    }
    sqlite3_finalize (stmt);
  }
}

static void on_sidebar_new_tag_activated(GtkEntry *entry, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (text && *text != '\0') {
        int count = 0;
        sqlite3_stmt *stmt = db_tags_get_stats(state->current_project_id);
        if (stmt) {
            while (sqlite3_step(stmt) == SQLITE_ROW) count++;
            sqlite3_finalize(stmt);
        }
        const char *color = TAG_COLORS[count % TAG_COLORS_COUNT];
        
        db_tag_add(state->current_project_id, text, "", color);
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
        refresh_tags(state);
    }
}

static void refresh_tags(CualiAppState *state) {
    if (!state->tag_list) return;

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->tag_list))) {
        gtk_list_box_remove(GTK_LIST_BOX(state->tag_list), child);
    }

    sqlite3_stmt *stmt = db_tags_get_stats(state->current_project_id);
    if (!stmt) return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int tag_id = sqlite3_column_int(stmt, 0);
        const char *path = (const char *)sqlite3_column_text(stmt, 1);
        const char *color = (const char *)sqlite3_column_text(stmt, 2);
        int count = sqlite3_column_int(stmt, 3);

        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_widget_set_margin_top(box, 8);
        gtk_widget_set_margin_bottom(box, 8);

        GtkWidget *dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_size_request(dot, 10, 10);
        char *css = g_strdup_printf("box { background-color: %s; border-radius: 5px; }", color ? color : "#888888");
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(provider, css);
        gtk_style_context_add_provider(gtk_widget_get_style_context(dot),
                                       GTK_STYLE_PROVIDER(provider),
                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_free(css);
        gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(box), dot);

        GtkWidget *label = gtk_label_new(path);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(box), label);

        if (count > 0) {
            char *count_str = g_strdup_printf("%d", count);
            GtkWidget *count_label = gtk_label_new(count_str);
            gtk_widget_add_css_class(count_label, "numeric");
            gtk_widget_add_css_class(count_label, "dim-label");
            gtk_box_append(GTK_BOX(box), count_label);
            g_free(count_str);
        }

        GtkWidget *edit_btn2 = gtk_button_new_from_icon_name ("document-edit-symbolic");
        gtk_widget_add_css_class (edit_btn2, "flat");
        gtk_widget_set_valign (edit_btn2, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text (edit_btn2, "Editar etiqueta");
        gpointer *ebargs2 = g_new (gpointer, 2);
        ebargs2[0] = state; ebargs2[1] = GINT_TO_POINTER(tag_id);
        g_signal_connect_data (edit_btn2, "clicked", G_CALLBACK (on_tag_edit_btn_clicked),
                               ebargs2, (GClosureNotify)g_free, 0);
        gtk_box_append (GTK_BOX(box), edit_btn2);

        GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(row, box);
        g_object_set_data(G_OBJECT(row), "tag-id", GINT_TO_POINTER(tag_id));
        gtk_list_box_append(GTK_LIST_BOX(state->tag_list), GTK_WIDGET(row));
    }
    sqlite3_finalize(stmt);
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
      if (tags_str) {
          g_object_set_data_full (G_OBJECT (row), "tags_str", g_strdup(tags_str), g_free);
      }
      
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
load_document (CualiAppState *state, int document_id, const char *name, const char *contents)
{
  state->current_document_id = document_id;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->text_view));
  
  gtk_text_buffer_set_text (buffer, "", 0);
  
  if (state->offset_map) g_free (state->offset_map);
  char *clean_text = map_html (contents, &state->offset_map, &state->plain_text_len);
  
  gtk_text_buffer_set_text (buffer, clean_text ? clean_text : "", -1);
  /* NOTE: do NOT free clean_text here — used below for offset calculations */
  
  sqlite3_stmt *stmt = db_highlights_get_for_document (document_id);
  if (stmt) {
    while (sqlite3_step (stmt) == SQLITE_ROW) {
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
          apply_highlight_tag (buffer, hl_id, &start_iter, &end_iter);
      }
    }
    sqlite3_finalize (stmt);
  }
  
  g_free (clean_text);
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
  if (file == NULL) return;

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
      apply_highlight_tag (buffer, hl_id, &start, &end);
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
    gtk_widget_set_vexpand (state->root_stack, TRUE);
    gtk_widget_set_hexpand (state->root_stack, TRUE);
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (window), state->root_stack);

    /* --- Welcome Screen --- */
    GtkWidget *welcome_view = adw_toolbar_view_new ();
    adw_view_stack_add_named (ADW_VIEW_STACK (state->root_stack), welcome_view, "welcome");
    
    GtkWidget *welcome_header = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (welcome_view), welcome_header);

    GtkWidget *welcome_content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign (welcome_content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (welcome_content, GTK_ALIGN_CENTER);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (welcome_view), welcome_content);

    GtkWidget *logo = gtk_image_new_from_icon_name ("org.gnome.TextEditor-symbolic");
    gtk_image_set_pixel_size (GTK_IMAGE (logo), 128);
    gtk_box_append (GTK_BOX (welcome_content), logo);

    GtkWidget *title = gtk_label_new ("Bienvenido a Cuali");
    gtk_widget_add_css_class (title, "title-1");
    gtk_box_append (GTK_BOX (welcome_content), title);

    GtkWidget *btns_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append (GTK_BOX (welcome_content), btns_box);

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
    gtk_box_append (GTK_BOX (welcome_content), recent_box);
    
    GtkWidget *recent_label = gtk_label_new ("Proyectos Recientes");
    gtk_widget_add_css_class (recent_label, "sidebar-title");
    gtk_box_append (GTK_BOX (recent_box), recent_label);
    
    GtkWidget *recent_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_size_request (recent_scroll, 400, 200);
    gtk_box_append (GTK_BOX (recent_box), recent_scroll);

    state->recent_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->recent_list, "sidebar-list");
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
    gtk_box_append (GTK_BOX (main_view_box), state->view_stack);
    gtk_widget_set_vexpand (state->view_stack, TRUE);
    
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
    
    GtkWidget *docs_title = gtk_label_new("Documentos");
    gtk_widget_add_css_class (docs_title, "sidebar-title");
    gtk_widget_set_halign (docs_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar_box), docs_title);

    GtkWidget *scrolled_docs = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled_docs, TRUE);
    gtk_box_append (GTK_BOX (sidebar_box), scrolled_docs);
    state->doc_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->doc_list, "sidebar-list");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_docs), state->doc_list);
    g_signal_connect (state->doc_list, "row-selected", G_CALLBACK (on_doc_row_selected), state);

    GtkWidget *tags_title = gtk_label_new("Etiquetas");
    gtk_widget_add_css_class (tags_title, "sidebar-title");
    gtk_widget_set_halign (tags_title, GTK_ALIGN_START);
    gtk_widget_set_margin_top (tags_title, 20);
    gtk_box_append(GTK_BOX(sidebar_box), tags_title);

    GtkWidget *scrolled_tags = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolled_tags, TRUE);
    gtk_box_append (GTK_BOX (sidebar_box), scrolled_tags);

    state->tag_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->tag_list, "sidebar-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->tag_list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_tags), state->tag_list);

    GtkWidget *tag_entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(tag_entry_box, 10);
    gtk_box_append(GTK_BOX(sidebar_box), tag_entry_box);

    GtkWidget *tag_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(tag_entry), "+ Nueva etiqueta...");
    gtk_widget_set_hexpand(tag_entry, TRUE);
    g_signal_connect(tag_entry, "activate", G_CALLBACK(on_sidebar_new_tag_activated), state);
    gtk_box_append(GTK_BOX(tag_entry_box), tag_entry);

    GtkWidget *content_scroll = gtk_scrolled_window_new ();
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_view), content_scroll);
    GtkWidget *paper_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class (paper_box, "paper-sheet");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_scroll), paper_box);
    GtkWidget *text_view = gtk_text_view_new();
    state->text_view = text_view;
    
    GtkGesture *click_gesture = gtk_gesture_click_new ();
    g_signal_connect (click_gesture, "pressed", G_CALLBACK (on_text_view_clicked), state);
    g_signal_connect (click_gesture, "released", G_CALLBACK (on_text_view_released), state);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click_gesture), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller (text_view, GTK_EVENT_CONTROLLER (click_gesture));
    
    /* Defer popover creation until text_view is realized so it has
       a native surface — avoids "GtkPopover is not a child" warning */
    g_signal_connect(text_view, "realize",
                     G_CALLBACK(on_text_view_realized), state);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 100);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 100);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 60);
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);
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
    gtk_widget_add_css_class (res_sidebar, "background");
    gtk_widget_set_size_request (res_sidebar, 250, -1);
    adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_sidebar);

    GtkWidget *stats_title = gtk_label_new("Estadísticas");
    gtk_widget_add_css_class (stats_title, "sidebar-title");
    gtk_widget_set_halign (stats_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start (stats_title, 12);
    gtk_box_append(GTK_BOX(res_sidebar), stats_title);

    GtkWidget *res_tag_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (res_tag_scroll, TRUE);
    gtk_box_append (GTK_BOX (res_sidebar), res_tag_scroll);
    state->results_tag_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->results_tag_list, "sidebar-list");
    g_signal_connect (state->results_tag_list, "row-selected", G_CALLBACK (on_results_tag_selected), state);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (res_tag_scroll), state->results_tag_list);

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
    gtk_widget_set_tooltip_text (res_toggle_btn, "Mostrar/ocultar filtros");
    gtk_box_append (GTK_BOX (res_toolbar), res_toggle_btn);
    g_signal_connect (res_toggle_btn, "toggled",
                      G_CALLBACK (on_res_sidebar_toggle),
                      res_split_view);

    GtkWidget *res_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append (GTK_BOX (res_content_vbox), res_sep);

    GtkWidget *res_content_scroll = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (res_content_scroll, TRUE);
    gtk_box_append (GTK_BOX (res_content_vbox), res_content_scroll);

    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(res_split_view), res_content_vbox);
    
    state->results_list = gtk_list_box_new ();
    gtk_list_box_set_filter_func (GTK_LIST_BOX (state->results_list), results_filter_func, state, NULL);
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->results_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class (state->results_list, "sidebar-list");
    gtk_widget_set_margin_start (state->results_list, 60);
    gtk_widget_set_margin_end (state->results_list, 60);
    gtk_widget_set_margin_top (state->results_list, 40);
    gtk_widget_set_margin_bottom (state->results_list, 40);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (res_content_scroll), state->results_list);

    gtk_window_present(GTK_WINDOW(window));
}
