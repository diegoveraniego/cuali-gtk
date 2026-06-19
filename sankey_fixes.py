import sys
import re

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # We need to add zoom to HeatmapState
    data = data.replace(
        "    int **matrix; // NxN\n} HeatmapState;",
        "    int **matrix; // NxN\n    double zoom;\n} HeatmapState;"
    )

    # Initialize zoom
    data = data.replace(
        "    hm->num_tags = 0;\n    while(tstmt",
        "    hm->zoom = 1.0;\n    hm->num_tags = 0;\n    while(tstmt"
    )

    # Color helper function to add
    color_helper = """static void get_category_color(const char *tag_name, double *r, double *g, double *b) {
    const char *slash = strchr(tag_name, '/');
    int len = slash ? (slash - tag_name) : strlen(tag_name);
    unsigned int hash = 0;
    for (int i = 0; i < len; i++) {
        hash = hash * 31 + tag_name[i];
    }
    double h = (hash % 360) / 360.0;
    
    // hsv to rgb (v=0.8, s=0.6)
    int i = (int)(h * 6);
    double f = h * 6 - i;
    double p = 0.8 * (1 - 0.6);
    double q = 0.8 * (1 - f * 0.6);
    double t = 0.8 * (1 - (1 - f) * 0.6);
    switch(i % 6) {
        case 0: *r=0.8, *g=t, *b=p; break;
        case 1: *r=q, *g=0.8, *b=p; break;
        case 2: *r=p, *g=0.8, *b=t; break;
        case 3: *r=p, *g=q, *b=0.8; break;
        case 4: *r=t, *g=p, *b=0.8; break;
        case 5: *r=0.8, *g=p, *b=q; break;
    }
}

static void draw_heatmap"""

    data = data.replace("static void draw_heatmap", color_helper)

    # Replace draw_heatmap implementation
    old_draw = re.search(r"static void draw_heatmap.*?(?=GtkWidget\* create_heatmap_view)", data, re.DOTALL).group(0)

    new_draw = """static void draw_heatmap(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    
    int max_val = 1;
    for(int i = 0; i < hm->num_tags; i++) {
        for(int j = 0; j < hm->num_tags; j++) {
            if(hm->matrix[i][j] > max_val) max_val = hm->matrix[i][j];
        }
    }
    
    // Dynamic margin based on longest text
    int max_text_w = 100;
    for(int i = 0; i < hm->num_tags; i++) {
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[i], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        if (tw > max_text_w) max_text_w = tw;
        pango_font_description_free(desc);
        g_object_unref(layout);
    }
    
    int margin_x = max_text_w + 30; // 30px padding for the node box
    int node_height = 20;
    int gap = 15;
    int offset_y = 50;
    
    cairo_scale(cr, hm->zoom, hm->zoom);
    width = width / hm->zoom;
    height = height / hm->zoom;
    
    // Draw Sankey Ribbons
    for(int i = 0; i < hm->num_tags; i++) {
        double r, g, b;
        get_category_color(hm->tag_names[i], &r, &g, &b);
        
        for(int j = i + 1; j < hm->num_tags; j++) {
            int val = hm->matrix[i][j];
            if (val > 0) {
                double y1 = offset_y + i * (node_height + gap) + node_height/2.0;
                double y2 = offset_y + j * (node_height + gap) + node_height/2.0;
                double x1 = margin_x;
                double x2 = width - margin_x;
                
                double ribbon_width = fmax(2.0, ((double)val / max_val) * 15.0);
                
                cairo_set_source_rgba(cr, r, g, b, 0.4); // Colored ribbon based on source
                cairo_set_line_width(cr, ribbon_width);
                cairo_move_to(cr, x1, y1);
                cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1, x2 - (x2-x1)/2.0, y2, x2, y2);
                cairo_stroke(cr);
            }
        }
    }
    
    GdkRGBA fg_color;
    gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
    
    // Draw Nodes (Left side)
    for(int i = 0; i < hm->num_tags; i++) {
        double r, g, b;
        get_category_color(hm->tag_names[i], &r, &g, &b);
        double y = offset_y + i * (node_height + gap);
        
        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, margin_x - 10, y, 10, node_height, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[i], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, margin_x - 15 - tw, y + (node_height - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    // Draw Nodes (Right side)
    for(int i = 0; i < hm->num_tags; i++) {
        double r, g, b;
        get_category_color(hm->tag_names[i], &r, &g, &b);
        double y = offset_y + i * (node_height + gap);
        
        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, width - margin_x, y, 10, node_height, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[i], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, width - margin_x + 15, y + (node_height - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
}
"""
    data = data.replace(old_draw, new_draw)

    # Scroll event for zooming
    scroll_cb = """static gboolean on_hm_scroll(GtkEventControllerScroll *scroll, double dx, double dy, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    if (dy > 0) hm->zoom *= 0.9;
    else if (dy < 0) hm->zoom *= 1.1;
    hm->zoom = fmax(0.1, fmin(hm->zoom, 5.0));
    
    GtkWidget *area = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(scroll));
    gtk_widget_set_size_request(area, -1, (hm->num_tags * 35 + 100) * hm->zoom);
    gtk_widget_queue_draw(area);
    return TRUE;
}

GtkWidget* create_heatmap_view"""

    data = data.replace("GtkWidget* create_heatmap_view", scroll_cb)

    # Attach scroll controller in create_heatmap_view
    old_create = """GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_heatmap, hm, NULL);
    gtk_widget_set_size_request(area, 200 + hm->num_tags*25, 200 + hm->num_tags*25);
    g_hm_area = area;"""

    new_create = """GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_heatmap, hm, NULL);
    gtk_widget_set_size_request(area, -1, hm->num_tags * 35 + 100);
    g_hm_area = area;
    
    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_hm_scroll), hm);
    gtk_widget_add_controller(area, scroll_ctrl);"""

    data = data.replace(old_create, new_create)

    # Update refresh size request
    data = data.replace(
        "gtk_widget_set_size_request(g_hm_area, 200 + ((HeatmapState *)g_hm_state)->num_tags*25, 200 + ((HeatmapState *)g_hm_state)->num_tags*25);",
        "gtk_widget_set_size_request(g_hm_area, -1, ((HeatmapState *)g_hm_state)->num_tags*35 + 100);"
    )

    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
