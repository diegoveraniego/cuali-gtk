import re

with open("src/window.c", "r") as f:
    src = f.read()

impl = """static gboolean apply_draft_changes_func(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
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
}"""

# Find the start and end of apply_draft_changes_func
start_idx = src.find("static gboolean apply_draft_changes_func")
end_idx = src.find("static void on_tag_manager_apply_clicked")

if start_idx != -1 and end_idx != -1:
    src = src[:start_idx] + impl + "\n\n" + src[end_idx:]
    with open("src/window.c", "w") as f:
        f.write(src)
print("Updated apply function")
