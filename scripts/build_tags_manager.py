import re

with open("include/window.h", "r") as f:
    hdr = f.read()

if "GtkTreeStore *manager_tag_tree_store;" not in hdr:
    hdr = hdr.replace("GtkWidget *revision_btn_next;", "GtkWidget *revision_btn_next;\n    GtkTreeStore *manager_tag_tree_store;\n    GtkWidget *manager_tag_tree_view;\n    GtkWidget *manager_apply_btn;\n    GtkWidget *manager_undo_btn;")
    with open("include/window.h", "w") as f:
        f.write(hdr)

with open("src/window.c", "r") as f:
    src = f.read()

impl = """// ------------------ TAG MANAGER ------------------

static void on_tag_manager_apply_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    // We will implement apply later
    refresh_tags(state);
}

static void on_tag_manager_undo_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    refresh_tag_manager(state);
}

static void refresh_tag_manager(CualiAppState *state) {
    if (!state->manager_tag_tree_store) return;
    gtk_tree_store_clear(state->manager_tag_tree_store);
    
    // Simplest way: reuse populate_tag_tree_store logic or just copy state->tag_tree_store
    // But we need to traverse state->tag_tree_store and copy to manager_tag_tree_store
    // For now we will just re-query or use populate_tag_store.
    // Assuming root_tag_node is available, but it's not global. We can just use the tree store copy.
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
    
    GtkWidget *lbl = gtk_label_new("Borrador de Etiquetas");
    gtk_widget_add_css_class(lbl, "title-2");
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(toolbar), lbl);
    
    state->manager_undo_btn = gtk_button_new_with_label("Deshacer");
    g_signal_connect(state->manager_undo_btn, "clicked", G_CALLBACK(on_tag_manager_undo_clicked), state);
    gtk_box_append(GTK_BOX(toolbar), state->manager_undo_btn);
    
    state->manager_apply_btn = gtk_button_new_with_label("Aplicar a BD");
    gtk_widget_add_css_class(state->manager_apply_btn, "suggested-action");
    g_signal_connect(state->manager_apply_btn, "clicked", G_CALLBACK(on_tag_manager_apply_clicked), state);
    gtk_box_append(GTK_BOX(toolbar), state->manager_apply_btn);
    
    // ...
    return box;
}
// -------------------------------------------------
"""

# Replace the block from // ------------------ TAG MANAGER ------------------ to // -------------------------------------------------
start_idx = src.find("// ------------------ TAG MANAGER ------------------")
end_idx = src.find("// -------------------------------------------------") + len("// -------------------------------------------------")

if start_idx != -1 and end_idx != -1:
    src = src[:start_idx] + impl + src[end_idx:]
    with open("src/window.c", "w") as f:
        f.write(src)
print("Updated window.c and window.h")
