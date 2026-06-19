import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # Step 3: Replace heatmap with sankey
    old_hm_draw = """static void draw_heatmap(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    
    int cell_size = 25;
    int offset_x = 150;
    int offset_y = 150;
    
    int max_val = 1;
    for(int i = 0; i < hm->num_tags; i++) {
        for(int j = 0; j < hm->num_tags; j++) {
            if(hm->matrix[i][j] > max_val) max_val = hm->matrix[i][j];
        }
    }
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    
    for(int i = 0; i < hm->num_tags; i++) {
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_move_to(cr, 10, offset_y + i * cell_size + 18);
        cairo_show_text(cr, hm->tag_names[i]);
        
        cairo_save(cr);
        cairo_translate(cr, offset_x + i * cell_size + 15, 140);
        cairo_rotate(cr, -G_PI / 4.0);
        cairo_move_to(cr, 0, 0);
        cairo_show_text(cr, hm->tag_names[i]);
        cairo_restore(cr);
        
        for(int j = 0; j < hm->num_tags; j++) {
            int val = hm->matrix[i][j];
            if (val > 0) {
                double intensity = (double)val / (double)max_val;
                cairo_set_source_rgba(cr, 1.0, 1.0 - intensity, 1.0 - intensity, 1.0);
            } else {
                cairo_set_source_rgba(cr, 0.95, 0.95, 0.95, 1.0);
            }
            cairo_rectangle(cr, offset_x + j * cell_size, offset_y + i * cell_size, cell_size - 1, cell_size - 1);
            cairo_fill(cr);
            
            if (val > 0) {
                cairo_set_source_rgb(cr, 0, 0, 0);
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", val);
                cairo_move_to(cr, offset_x + j * cell_size + 5, offset_y + i * cell_size + 18);
                cairo_show_text(cr, buf);
            }
        }
    }
}"""

    new_hm_draw = """static void draw_heatmap(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    // We are drawing a Sankey diagram instead of a heatmap now
    
    int max_val = 1;
    for(int i = 0; i < hm->num_tags; i++) {
        for(int j = 0; j < hm->num_tags; j++) {
            if(hm->matrix[i][j] > max_val) max_val = hm->matrix[i][j];
        }
    }
    
    int node_height = 20;
    int gap = 15;
    int margin_x = 200;
    int offset_y = 50;
    int total_height = hm->num_tags * (node_height + gap);
    
    // Draw Sankey Ribbons
    for(int i = 0; i < hm->num_tags; i++) {
        for(int j = i + 1; j < hm->num_tags; j++) {
            int val = hm->matrix[i][j];
            if (val > 0) {
                double y1 = offset_y + i * (node_height + gap) + node_height/2.0;
                double y2 = offset_y + j * (node_height + gap) + node_height/2.0;
                double x1 = margin_x;
                double x2 = width - margin_x;
                
                double ribbon_width = fmax(2.0, ((double)val / max_val) * 15.0);
                
                cairo_set_source_rgba(cr, 0.5, 0.3, 0.8, 0.3); // Purple ribbon
                cairo_set_line_width(cr, ribbon_width);
                cairo_move_to(cr, x1, y1);
                cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1, x2 - (x2-x1)/2.0, y2, x2, y2);
                cairo_stroke(cr);
            }
        }
    }
    
    // Draw Nodes (Left side)
    for(int i = 0; i < hm->num_tags; i++) {
        double y = offset_y + i * (node_height + gap);
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8);
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
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5); // Fallback gray
        
        // For dark mode compatibility, we use Pango markup or rely on transparent bg
        // Wait, since we can't reliably guess theme text color without CSS querying, we just use a neutral color or extract it from style context.
        GdkRGBA fg_color;
        gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    // Draw Nodes (Right side)
    for(int i = 0; i < hm->num_tags; i++) {
        double y = offset_y + i * (node_height + gap);
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8);
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
        
        GdkRGBA fg_color;
        gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
}"""
    data = data.replace(old_hm_draw, new_hm_draw)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
