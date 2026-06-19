static void draw_heatmap(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    
    int n = hm->num_tags;
    if (n == 0) return;
    
    // Dynamic margin based on longest text
    int max_text_w = 100;
    for(int i = 0; i < n; i++) {
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
    
    int margin_x = max_text_w + 40;
    int sankey_lane_w = 400; // fixed width for ribbons
    
    cairo_scale(cr, hm->zoom, hm->zoom);
    double v_width = (double)width / hm->zoom;
    
    // Center the whole thing
    double total_needed_w = margin_x * 2 + sankey_lane_w;
    double start_x = 0;
    if (v_width > total_needed_w) {
        start_x = (v_width - total_needed_w) / 2.0;
    } else {
        // If too narrow, shrink lane but keep margins
        if (v_width > margin_x * 2 + 50) {
            sankey_lane_w = v_width - margin_x * 2;
        } else {
            // Extreme narrow case
            margin_x = v_width * 0.3;
            sankey_lane_w = v_width * 0.4;
        }
    }
    
    int offset_y = 50;
    
    // Allocate Sankey data
    SankeyNode *left = g_new0(SankeyNode, n);
    SankeyNode *right = g_new0(SankeyNode, n);
    SankeyLink *links = g_new0(SankeyLink, n * n);
    int num_links = 0;
    
    double max_h = fmax(600.0, n * 30.0);
    compute_sankey_layout(hm, left, right, links, &num_links, max_h);
    
    // Draw Links (Ribbons)
    for (int l = 0; l < num_links; l++) {
        int s = find_node_by_orig(left, n, links[l].source);
        int t = find_node_by_orig(right, n, links[l].target);
        if (s == -1 || t == -1) continue;
        if (left[s].value <= 0) continue; // Safety
        
        double x1 = start_x + margin_x;
        double y1 = offset_y + left[s].y + links[l].sy;
        double x2 = x1 + sankey_lane_w;
        double y2 = offset_y + right[t].y + links[l].ty;
        double link_width = (links[l].value / left[s].value) * left[s].dy;
        
        if (isnan(y1) || isnan(y2) || isnan(link_width)) continue;

        double r, g, b;
        get_category_color(hm->tag_names[links[l].source], &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.3);
        
        cairo_new_path(cr);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr, x1 + sankey_lane_w / 2.0, y1, x2 - sankey_lane_w / 2.0, y2, x2, y2);
        cairo_line_to(cr, x2, y2 + link_width);
        cairo_curve_to(cr, x2 - sankey_lane_w / 2.0, y2 + link_width, x1 + sankey_lane_w / 2.0, y1 + link_width, x1, y1 + link_width);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Border for the ribbon
        cairo_set_source_rgba(cr, r, g, b, 0.1);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr, x1 + sankey_lane_w / 2.0, y1, x2 - sankey_lane_w / 2.0, y2, x2, y2);
        cairo_stroke(cr);
        cairo_move_to(cr, x1, y1 + link_width);
        cairo_curve_to(cr, x1 + sankey_lane_w / 2.0, y1 + link_width, x2 - sankey_lane_w / 2.0, y2 + link_width, x2, y2 + link_width);
        cairo_stroke(cr);
    }
    
    GdkRGBA fg_color;
    gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
    
    // Draw Left Nodes
    for(int i = 0; i < n; i++) {
        int orig = left[i].original_index;
        if (left[i].value == 0) continue;
        
        double r, g, b;
        get_category_color(hm->tag_names[orig], &r, &g, &b);
        double y = offset_y + left[i].y;
        double node_x = start_x + margin_x;
        
        // Shadow/glow
        cairo_set_source_rgba(cr, r, g, b, 0.2);
        rounded_rect(cr, node_x - 12, y - 2, 14, left[i].dy + 4, 3);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, node_x - 10, y, 10, left[i].dy, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[orig], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_width(layout, (margin_x - 25) * PANGO_SCALE);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, node_x - 20 - tw, y + (left[i].dy - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    // Draw Right Nodes
    for(int i = 0; i < n; i++) {
        int orig = right[i].original_index;
        if (right[i].value == 0) continue;
        
        double r, g, b;
        get_category_color(hm->tag_names[orig], &r, &g, &b);
        double y = offset_y + right[i].y;
        double node_x = start_x + margin_x + sankey_lane_w;
        
        // Shadow/glow
        cairo_set_source_rgba(cr, r, g, b, 0.2);
        rounded_rect(cr, node_x - 2, y - 2, 14, right[i].dy + 4, 3);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, node_x, y, 10, right[i].dy, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[orig], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_width(layout, (margin_x - 25) * PANGO_SCALE);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, node_x + 20, y + (right[i].dy - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    g_free(left);
    g_free(right);
    g_free(links);
}
