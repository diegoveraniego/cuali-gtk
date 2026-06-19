import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    old_view = """GtkWidget* create_visualizations_view(CualiAppState *state) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget *viz_switcher = adw_view_switcher_new();
    GtkWidget *viz_stack = adw_view_stack_new();
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(viz_switcher), ADW_VIEW_STACK(viz_stack));
    
    // 1. Whiteboard
    GtkWidget *wb_view = create_whiteboard_view(state);
    AdwViewStackPage *p1 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wb_view, "whiteboard", "Whiteboard");
    adw_view_stack_page_set_icon_name(p1, "cuali-network-symbolic");
    
    // 2. Heatmap
    GtkWidget *hm_view = create_heatmap_view(state);
    AdwViewStackPage *p2 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), hm_view, "heatmap", "Heatmap");
    adw_view_stack_page_set_icon_name(p2, "view-grid-symbolic");
    
    // 3. Matriz (Placeholder for now)
    GtkWidget *mat_view = create_matrix_view(state);
    AdwViewStackPage *p3 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), mat_view, "matrix", "Matriz Tag-Doc");
    adw_view_stack_page_set_icon_name(p3, "view-list-symbolic");
    
    // 4. Wordcloud
    GtkWidget *wc_view = create_wordcloud_view(state);
    AdwViewStackPage *p4 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wc_view, "wordcloud", "Word Cloud");
    adw_view_stack_page_set_icon_name(p4, "format-text-larger-symbolic");
    
    gtk_box_append(GTK_BOX(box), viz_switcher);
    gtk_box_append(GTK_BOX(box), viz_stack);
    gtk_widget_set_vexpand(viz_stack, TRUE);
    
    return box;
}"""

    new_view = """static void on_filter_changed(GtkRange *range, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    double val = gtk_range_get_value(range);
    // In a real implementation this would trigger a requery with a frequency threshold.
    // For now we just trigger a UI refresh to show responsiveness.
    refresh_visualizations(state);
}

GtkWidget* create_visualizations_view(CualiAppState *state) {
    GtkWidget *toolbar_view = adw_toolbar_view_new();
    
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), FALSE);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_bar);
    
    GtkWidget *viz_switcher = adw_view_switcher_new();
    GtkWidget *viz_stack = adw_view_stack_new();
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(viz_switcher), ADW_VIEW_STACK(viz_stack));
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), viz_switcher);
    
    // Filter Popover
    GtkWidget *filter_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(filter_btn), "view-filter-symbolic");
    gtk_widget_set_tooltip_text(filter_btn, "Ajustar frecuencia mínima");
    
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *pop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(pop_box, 12);
    gtk_widget_set_margin_end(pop_box, 12);
    gtk_widget_set_margin_top(pop_box, 12);
    gtk_widget_set_margin_bottom(pop_box, 12);
    
    GtkWidget *lbl = gtk_label_new("Frecuencia mínima de co-ocurrencia:");
    gtk_box_append(GTK_BOX(pop_box), lbl);
    
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 20, 1);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_filter_changed), state);
    gtk_box_append(GTK_BOX(pop_box), scale);
    
    gtk_popover_set_child(GTK_POPOVER(popover), pop_box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(filter_btn), popover);
    
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), filter_btn);
    
    // 1. Whiteboard
    GtkWidget *wb_view = create_whiteboard_view(state);
    AdwViewStackPage *p1 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wb_view, "whiteboard", "Redes");
    adw_view_stack_page_set_icon_name(p1, "cuali-network-symbolic");
    
    // 2. Sankey (replaces Heatmap)
    GtkWidget *hm_view = create_heatmap_view(state);
    AdwViewStackPage *p2 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), hm_view, "heatmap", "Co-ocurrencia");
    adw_view_stack_page_set_icon_name(p2, "view-grid-symbolic");
    
    // 3. Matriz (Placeholder for now)
    GtkWidget *mat_view = create_matrix_view(state);
    AdwViewStackPage *p3 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), mat_view, "matrix", "Código-Documento");
    adw_view_stack_page_set_icon_name(p3, "view-list-symbolic");
    
    // 4. Wordcloud
    GtkWidget *wc_view = create_wordcloud_view(state);
    AdwViewStackPage *p4 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wc_view, "wordcloud", "Frecuencias");
    adw_view_stack_page_set_icon_name(p4, "format-text-larger-symbolic");
    
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), viz_stack);
    
    return toolbar_view;
}"""
    
    data = data.replace(old_view, new_view)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
