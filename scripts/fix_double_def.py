import re

with open("src/window.c", "r") as f:
    src = f.read()

# remove everything between "// ------------------ TAG MANAGER ------------------" and "// -------------------------------------------------"
# and replace it cleanly.

impl = """// ------------------ TAG MANAGER ------------------

static void traverse_and_copy_model(GtkTreeModel *src_model, GtkTreeIter *src_iter, GtkTreeStore *dest_store, GtkTreeIter *dest_parent) {
    GtkTreeIter dest_iter;
    gtk_tree_store_append(dest_store, &dest_iter, dest_parent);
    
    int tag_id, count;
    char *name, *color, *path;
    
    gtk_tree_model_get(src_model, src_iter, 
                       0, &tag_id, 1, &name, 2, &color, 3, &count, 4, &path, -1);
                       
    gtk_tree_store_set(dest_store, &dest_iter,
                       0, tag_id, 1, name, 2, color, 3, count, 4, path, 5, FALSE, -1);
                       
    g_free(name); g_free(color); g_free(path);
    
    GtkTreeIter src_child;
    if (gtk_tree_model_iter_children(src_model, &src_child, src_iter)) {
        do {
            traverse_and_copy_model(src_model, &src_child, dest_store, &dest_iter);
        } while (gtk_tree_model_iter_next(src_model, &src_child));
    }
}

static void refresh_tag_manager(CualiAppState *state) {
    if (!state->manager_tag_tree_store) return;
    gtk_tree_store_clear(state->manager_tag_tree_store);
    
    GtkTreeModel *src_model = GTK_TREE_MODEL(state->tag_tree_store);
    GtkTreeIter src_iter;
    if (gtk_tree_model_get_iter_first(src_model, &src_iter)) {
        do {
            traverse_and_copy_model(src_model, &src_iter, state->manager_tag_tree_store, NULL);
        } while (gtk_tree_model_iter_next(src_model, &src_iter));
    }
}

static gboolean apply_draft_changes_func(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    CualiAppState *state = (CualiAppState *)data;
    gboolean modified;
    int tag_id;
    char *new_full_path;
    
    gtk_tree_model_get(model, iter, 0, &tag_id, 4, &new_full_path, 5, &modified, -1);
    
    if (modified && tag_id > 0 && new_full_path) {
        char *desc = NULL, *color = NULL, *old_path = NULL;
        if (db_tag_get_info(tag_id, &old_path, &desc, &color)) {
            db_tag_update(tag_id, new_full_path, desc);
            g_free(old_path); g_free(desc); g_free(color);
        }
    }
    
    g_free(new_full_path);
    return FALSE; // continue traversing
}

static void on_tag_manager_apply_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    
    GtkTreeModel *model = GTK_TREE_MODEL(state->manager_tag_tree_store);
    gtk_tree_model_foreach(model, apply_draft_changes_func, state);
    
    refresh_tags(state);
    refresh_tag_manager(state);
}

static void on_tag_manager_undo_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    refresh_tag_manager(state);
}

static void on_manager_move_save_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "dialog"));
    GtkEntry *entry = GTK_ENTRY(g_object_get_data(G_OBJECT(btn), "entry"));
    GtkTreeIter *iter = (GtkTreeIter *)g_object_get_data(G_OBJECT(btn), "iter");
    CualiAppState *state = (CualiAppState *)g_object_get_data(G_OBJECT(btn), "state");
    
    const char *new_path = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (strlen(new_path) > 0) {
        gtk_tree_store_set(state->manager_tag_tree_store, iter,
                           4, new_path,
                           5, TRUE,
                           -1);
    }
    
    adw_dialog_close(ADW_DIALOG(dialog));
    g_free(iter);
}

static void on_tag_manager_move_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(state->manager_tag_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *current_path;
        gtk_tree_model_get(model, &iter, 4, &current_path, -1);
        
        GtkWidget *dialog = adw_dialog_new();
        adw_dialog_set_title(ADW_DIALOG(dialog), "Mover Etiqueta (Borrador)");
        
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_margin_start(box, 20);
        gtk_widget_set_margin_end(box, 20);
        gtk_widget_set_margin_top(box, 20);
        gtk_widget_set_margin_bottom(box, 20);
        
        GtkWidget *lbl = gtk_label_new("Edita la ruta completa de la etiqueta. Usa '/' para crear carpetas.");
        gtk_box_append(GTK_BOX(box), lbl);
        
        GtkWidget *entry = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(entry), current_path ? current_path : "");
        gtk_box_append(GTK_BOX(box), entry);
        
        GtkWidget *save_btn = gtk_button_new_with_label("Guardar en Borrador");
        gtk_widget_add_css_class(save_btn, "suggested-action");
        
        GtkTreeIter *iter_copy = g_new0(GtkTreeIter, 1);
        *iter_copy = iter;
        
        g_object_set_data(G_OBJECT(save_btn), "dialog", dialog);
        g_object_set_data(G_OBJECT(save_btn), "entry", entry);
        g_object_set_data(G_OBJECT(save_btn), "iter", iter_copy);
        g_object_set_data(G_OBJECT(save_btn), "state", state);
        
        g_signal_connect(save_btn, "clicked", G_CALLBACK(on_manager_move_save_clicked), NULL);
        gtk_box_append(GTK_BOX(box), save_btn);
        
        adw_dialog_set_child(ADW_DIALOG(dialog), box);
        adw_dialog_present(ADW_DIALOG(dialog), state->window);
        
        g_free(current_path);
    }
}

static GtkWidget* create_tag_manager_view(CualiAppState *state) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(toolbar, "toolbar");
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    gtk_widget_set_margin_top(toolbar, 12);
    gtk_widget_set_margin_bottom(toolbar, 12);
    gtk_box_append(GTK_BOX(box), toolbar);
    
    GtkWidget *lbl = gtk_label_new("Gestor de Etiquetas");
    gtk_widget_add_css_class(lbl, "title-2");
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(toolbar), lbl);

    GtkWidget *move_btn = gtk_button_new_with_label("Mover Seleccionado...");
    g_signal_connect(move_btn, "clicked", G_CALLBACK(on_tag_manager_move_clicked), state);
    gtk_box_append(GTK_BOX(toolbar), move_btn);
    
    state->manager_undo_btn = gtk_button_new_with_label("Deshacer");
    g_signal_connect(state->manager_undo_btn, "clicked", G_CALLBACK(on_tag_manager_undo_clicked), state);
    gtk_box_append(GTK_BOX(toolbar), state->manager_undo_btn);
    
    state->manager_apply_btn = gtk_button_new_with_label("Aplicar a BD");
    gtk_widget_add_css_class(state->manager_apply_btn, "suggested-action");
    g_signal_connect(state->manager_apply_btn, "clicked", G_CALLBACK(on_tag_manager_apply_clicked), state);
    gtk_box_append(GTK_BOX(toolbar), state->manager_apply_btn);
    
    state->manager_tag_tree_store = gtk_tree_store_new(6, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN);
    state->manager_tag_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->manager_tag_tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(state->manager_tag_tree_view), TRUE);
    
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Etiqueta");
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer, "text", 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->manager_tag_tree_view), col);

    GtkTreeViewColumn *col_path = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col_path, "Ruta Completa");
    GtkCellRenderer *renderer_path = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col_path, renderer_path, TRUE);
    gtk_tree_view_column_add_attribute(col_path, renderer_path, "text", 4);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->manager_tag_tree_view), col_path);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->manager_tag_tree_view);
    gtk_box_append(GTK_BOX(box), scroll);
    
    return box;
}
// -------------------------------------------------"""

start_idx = src.find("// ------------------ TAG MANAGER ------------------")
end_idx = src.find("// -------------------------------------------------") + len("// -------------------------------------------------")

if start_idx != -1 and end_idx != -1:
    src = src[:start_idx] + impl + src[end_idx:]
    with open("src/window.c", "w") as f:
        f.write(src)
print("Fixed double definitions")
