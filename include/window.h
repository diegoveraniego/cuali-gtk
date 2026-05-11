#ifndef WINDOW_H
#define WINDOW_H

#include <adwaita.h>

typedef struct {
    GtkWidget *text_view;
    GtkWidget *window;
    GtkWidget *tag_list;
    GtkWidget *doc_list;
    GtkWidget *toast_overlay;
    GtkWidget *view_stack;
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
} CualiAppState;

void window_init(GtkApplication *app);

#endif
