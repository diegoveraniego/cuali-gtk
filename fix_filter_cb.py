import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    old_text = "// --- Main Visualizations View ---\nGtkWidget* create_visualizations_view"
    new_text = """// --- Main Visualizations View ---
static void on_filter_changed(GtkRange *range, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    refresh_visualizations(state);
}

GtkWidget* create_visualizations_view"""

    data = data.replace(old_text, new_text)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
