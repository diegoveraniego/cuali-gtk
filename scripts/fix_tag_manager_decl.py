with open("src/window.c", "r") as f:
    content = f.read()

decl = """
// Tag Manager Logic
static void refresh_tag_manager(CualiAppState *state);
static GtkWidget* create_tag_manager_view(CualiAppState *state);
"""

if decl not in content:
    content = content.replace("static GtkWidget *create_visualizations_view(CualiAppState *state);", 
                              decl + "\nstatic GtkWidget *create_visualizations_view(CualiAppState *state);")

with open("src/window.c", "w") as f:
    f.write(content)
