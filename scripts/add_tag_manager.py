import re
import sys

with open("src/window.c", "r") as f:
    content = f.read()

# We need to insert the declarations and implementation of the tag manager
decl = """
// Tag Manager Logic
static void refresh_tag_manager(CualiAppState *state);
static GtkWidget* create_tag_manager_view(CualiAppState *state);
"""

if "create_tag_manager_view" not in content:
    content = content.replace("static GtkWidget *create_visualizations_view(CualiAppState *state);", 
                              "static GtkWidget *create_visualizations_view(CualiAppState *state);\n" + decl)

impl = """
// ------------------ TAG MANAGER ------------------

static void populate_tag_manager_store(TagNode *n, GtkTreeStore *store, GtkTreeIter *parent_iter) {
    GtkTreeIter iter;
    if (n->name) {
        gtk_tree_store_append(store, &iter, parent_iter);
        // Column 0: ID, 1: Display Name, 2: Color, 3: Count, 4: Full Path (Draft), 5: Modified (bool)
        gtk_tree_store_set(store, &iter,
            0, n->tag_id,
            1, n->name,
            2, n->color,
            3, n->count,
            4, "", // We will set full path later or compute it
            5, FALSE,
            -1);
    }
    
    GtkTreeIter *new_parent = n->name ? &iter : parent_iter;
    for (GList *l = n->children; l; l = l->next) {
        populate_tag_manager_store((TagNode *)l->data, store, new_parent);
    }
}

static void refresh_tag_manager(CualiAppState *state) {
    // Left as a placeholder for now, we will refine this
}

static GtkWidget* create_tag_manager_view(CualiAppState *state) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *label = gtk_label_new("Gestor de Etiquetas (Borrador)");
    gtk_widget_add_css_class(label, "title-1");
    gtk_widget_set_margin_top(label, 20);
    gtk_widget_set_margin_bottom(label, 20);
    gtk_box_append(GTK_BOX(box), label);
    return box;
}
// -------------------------------------------------
"""

if "create_tag_manager_view(" not in content.split("static GtkWidget* create_tag_manager_view")[1:]:
    # Insert before create_visualizations_view implementation or just at the end before main
    content = content.replace("int\nmain (int argc, char **argv)", impl + "\nint\nmain (int argc, char **argv)")

# Now insert it into the view stack
tab_code = """
    /* --- Pestaña 4: Gestor de Etiquetas --- */
    GtkWidget *tag_manager_view = create_tag_manager_view(state);
    page = adw_view_stack_add_titled (ADW_VIEW_STACK (state->view_stack), tag_manager_view, "tag_manager", "Tag Manager");
    adw_view_stack_page_set_icon_name (page, "view-list-symbolic");
"""

if "tag_manager_view" not in content:
    content = content.replace("/* 5. Visualizations View */", tab_code + "\n    /* 5. Visualizations View */")

with open("src/window.c", "w") as f:
    f.write(content)
print("Tag manager view injected")
