#ifndef WINDOW_H
#define WINDOW_H

#include <adwaita.h>

typedef enum {
    VIM_NORMAL,
    VIM_VISUAL
} VimMode;

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
    bool is_loading_document;
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

    /* Tag Map State */
    GtkWidget *map_drawing_area;
    int map_selected_tag_id;   /* -1 = ninguno */
    double *map_node_x;
    double *map_node_y;
    int    *map_node_tag_id;
    int     map_node_count;

    /* Vim Mode */
    gboolean vim_enabled;
    VimMode  vim_mode;
    guint32  last_g_time;
    guint32  last_z_time;
    int      vim_count;          /* digit accumulator for counts like 5j */
    gchar    vim_find_char;      /* char to find with f/F/t/T */
    gboolean vim_find_forward;   /* TRUE = f/t, FALSE = F/T */
    gboolean vim_find_till;      /* TRUE = t/T (stop before), FALSE = f/F (land on) */
    GtkWidget *vim_mode_label;
    GtkWidget *vim_cursor_area;

    /* Last active document per project (restored on reopen) */
    int      last_document_id;
} CualiAppState;

void window_init(GtkApplication *app);

#endif
