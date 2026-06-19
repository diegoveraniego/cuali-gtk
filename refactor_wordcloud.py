import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # Step 5: Update wordcloud creation
    old_wc_create = """GtkWidget* create_wordcloud_view(CualiAppState *state) {
    init_stopwords();
    
    GtkWidget *box = gtk_flow_box_new();
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    g_wc_box = box;
    
    populate_wordcloud(state, box);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    return scroll;
}"""

    new_wc_create = """GtkWidget* create_wordcloud_view(CualiAppState *state) {
    init_stopwords();
    
    GtkWidget *box = gtk_flow_box_new();
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(box, 32);
    gtk_widget_set_margin_bottom(box, 32);
    g_wc_box = box;
    
    populate_wordcloud(state, box);
    
    // Wrap in AdwClamp for nice GNOME margins
    GtkWidget *clamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 800);
    
    // Wrap in a card
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_box_append(GTK_BOX(card), box);
    
    adw_clamp_set_child(ADW_CLAMP(clamp), card);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), clamp);
    
    return scroll;
}"""
    data = data.replace(old_wc_create, new_wc_create)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
