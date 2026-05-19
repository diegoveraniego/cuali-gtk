#ifndef WINDOW_H
#define WINDOW_H

#include <adwaita.h>

typedef struct {
    GtkWidget *text_view;
    GtkWidget *window;
    GtkWidget *doc_list;
    GtkWidget *toast_overlay;
    GtkWidget *view_stack;
    GtkWidget *tag_list;
    GtkWidget *results_list;
    GtkWidget *results_tag_list;
    GtkWidget *project_name_entry;
    GtkWidget *project_desc_entry;
    GtkWidget *root_stack;
    GtkWidget *edit_toggle;
    GtkWidget *recent_list;
    int current_document_id;
    int current_project_id;
    bool is_editing;
    int *offset_map;
    int plain_text_len;
    char *selected_result_tag;
    /* [NUEVO] */
    GtkWidget *highlight_popover;
    GtkWidget *popover_tag_list;
    GtkWidget *popover_delete_btn;
    GtkWidget *popover_memo_view;   /* GtkTextView for researcher memos */
    GtkWidget *highlight_selector;  /* Popover for overlapping highlights */
    int active_highlight_id;
    int pending_start;
    int pending_end;
    double zoom_level;
    bool has_unsaved_changes;
    GtkWidget *save_btn;

    /* Busqueda en documento (Ctrl+F) */
    GtkWidget *search_bar;
    GtkWidget *search_entry;
    GtkTextTag *search_match_tag;
    GtkTextTag *search_current_tag;
    int search_match_count;
    int search_current_match;

    /* Auto-guardado */
    guint auto_save_id;

    /* Barra de estado */
    GtkWidget *status_label;

    /* Filtro de documentos */
    GtkWidget *doc_filter_entry;
} CualiAppState;

void window_init(GtkApplication *app);

#endif
