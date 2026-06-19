import sys

with open('src/window.c', 'r') as f:
    content = f.read()

# 1. Add tag_tree_cell_data_func and on_tag_tree_row_activated before refresh_tags
new_funcs = """
static void tag_tree_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    char *name = NULL, *color = NULL;
    int count = 0;
    gtk_tree_model_get(model, iter, 1, &name, 2, &color, 3, &count, -1);
    
    if (!color) color = g_strdup("#77767b");
    
    char *markup;
    if (count > 0) {
        markup = g_strdup_printf("<span foreground=\\"%s\\">●</span> %s <span foreground=\\"#888888\\" size=\\"smaller\\">(%d)</span>", color, name ? name : "", count);
    } else {
        markup = g_strdup_printf("<span foreground=\\"%s\\">●</span> %s", color, name ? name : "");
    }
    
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
"""

if "tag_tree_cell_data_func" not in content:
    content = content.replace("static void\nrefresh_tags", new_funcs + "\nstatic void\nrefresh_tags")

# 2. Update refresh_tags
old_refresh = """    /* Flatten tree into list box rows via recursive helper */
    flatten_tag_tree (&root, 0, GTK_LIST_BOX (state->tag_list), state);"""

new_refresh = """    if (state->tag_tree_store) {
        gtk_tree_store_clear(state->tag_tree_store);
        populate_tag_store(&root, state->tag_tree_store, NULL);
        gtk_tree_view_expand_all(GTK_TREE_VIEW(state->tag_tree_view));
    }"""

if old_refresh in content:
    content = content.replace(old_refresh, new_refresh)

content = content.replace("if (!state->tag_list) return;", "if (!state->tag_tree_store) return;")
content = content.replace("while ((child = gtk_widget_get_first_child (state->tag_list)))", "//")
content = content.replace("gtk_list_box_remove (GTK_LIST_BOX (state->tag_list), child);", "//")


# 3. Update view_switcher to view_switcher_title
old_switcher = """    GtkWidget *view_switcher = adw_view_switcher_new ();
    adw_view_switcher_set_stack (ADW_VIEW_SWITCHER (view_switcher), ADW_VIEW_STACK (state->view_stack));
    adw_view_switcher_set_policy (ADW_VIEW_SWITCHER (view_switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), view_switcher);"""

new_switcher = """    GtkWidget *view_switcher_title = adw_view_switcher_title_new ();
    adw_view_switcher_title_set_stack (ADW_VIEW_SWITCHER_TITLE (view_switcher_title), ADW_VIEW_STACK (state->view_stack));
    adw_header_bar_set_title_widget (ADW_HEADER_BAR (header_bar), view_switcher_title);"""

if old_switcher in content:
    content = content.replace(old_switcher, new_switcher)

# 4. Update Sidebar Tags GtkListBox to GtkTreeView
old_tag_list = """    state->tag_list = gtk_list_box_new ();
    gtk_widget_add_css_class (state->tag_list, "sidebar-list");
    gtk_widget_add_css_class (state->tag_list, "boxed-list");
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (state->tag_list), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_tags), state->tag_list);"""

new_tag_tree = """    state->tag_tree_store = gtk_tree_store_new(4, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
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
    
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_tags), state->tag_tree_view);"""

if old_tag_list in content:
    content = content.replace(old_tag_list, new_tag_tree)

# 5. Fix show_tag_edit_dialog forward declaration if needed
if "static void show_tag_edit_dialog" not in content:
    content = content.replace("static void refresh_tags (CualiAppState *state);", "static void refresh_tags (CualiAppState *state);\nstatic void show_tag_edit_dialog (CualiAppState *state, int tag_id);")

with open('src/window.c', 'w') as f:
    f.write(content)
print("Done")
