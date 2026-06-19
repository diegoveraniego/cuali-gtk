import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # Step 4: Replace tagdoc matrix
    old_td_draw = """static void draw_tagdoc(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    TagDocState *td = (TagDocState *)user_data;
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    
    int cell_width = 35;
    int cell_height = 25;
    int offset_x = 150;
    int offset_y = 150;
    
    int max_val = 1;
    for(int i = 0; i < td->num_tags; i++) {
        for(int j = 0; j < td->num_docs; j++) {
            if(td->matrix[i][j] > max_val) max_val = td->matrix[i][j];
        }
    }
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    
    for(int i = 0; i < td->num_tags; i++) {
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_move_to(cr, 10, offset_y + i * cell_height + 18);
        cairo_show_text(cr, td->tag_names[i]);
        
        for(int j = 0; j < td->num_docs; j++) {
            if (i == 0) {
                cairo_save(cr);
                cairo_translate(cr, offset_x + j * cell_width + 15, 140);
                cairo_rotate(cr, -G_PI / 4.0);
                cairo_move_to(cr, 0, 0);
                cairo_show_text(cr, td->doc_names[j]);
                cairo_restore(cr);
            }
            
            int val = td->matrix[i][j];
            if (val > 0) {
                double intensity = (double)val / (double)max_val;
                cairo_set_source_rgba(cr, 1.0, 1.0 - intensity, 1.0 - intensity, 1.0);
            } else {
                cairo_set_source_rgba(cr, 0.95, 0.95, 0.95, 1.0);
            }
            cairo_rectangle(cr, offset_x + j * cell_width, offset_y + i * cell_height, cell_width - 1, cell_height - 1);
            cairo_fill(cr);
            
            if (val > 0) {
                cairo_set_source_rgb(cr, 0, 0, 0);
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", val);
                cairo_move_to(cr, offset_x + j * cell_width + 8, offset_y + i * cell_height + 18);
                cairo_show_text(cr, buf);
            }
        }
    }
}"""

    new_td_draw = """static void draw_tagdoc(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    TagDocState *td = (TagDocState *)user_data;
    
    int cell_width = 45;
    int cell_height = 30;
    int offset_x = 180;
    int offset_y = 150;
    int gap = 2;
    
    int max_val = 1;
    for(int i = 0; i < td->num_tags; i++) {
        for(int j = 0; j < td->num_docs; j++) {
            if(td->matrix[i][j] > max_val) max_val = td->matrix[i][j];
        }
    }
    
    GdkRGBA fg_color;
    gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
    
    for(int i = 0; i < td->num_tags; i++) {
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, td->tag_names[i], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, offset_x - 15 - tw, offset_y + i * (cell_height + gap) + (cell_height - th)/2.0);
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        
        for(int j = 0; j < td->num_docs; j++) {
            if (i == 0) {
                cairo_save(cr);
                cairo_translate(cr, offset_x + j * (cell_width + gap) + cell_width/2.0, offset_y - 10);
                cairo_rotate(cr, -G_PI / 4.0);
                
                PangoLayout *hl = pango_cairo_create_layout(cr);
                pango_layout_set_text(hl, td->doc_names[j], -1);
                PangoFontDescription *hdesc = pango_font_description_from_string("Inter, Cantarell 10");
                pango_layout_set_font_description(hl, hdesc);
                pango_font_description_free(hdesc);
                
                cairo_move_to(cr, 0, -10);
                cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
                pango_cairo_show_layout(cr, hl);
                g_object_unref(hl);
                cairo_restore(cr);
            }
            
            int val = td->matrix[i][j];
            double rx = offset_x + j * (cell_width + gap);
            double ry = offset_y + i * (cell_height + gap);
            
            if (val > 0) {
                double intensity = 0.2 + 0.8 * ((double)val / max_val);
                cairo_set_source_rgba(cr, 0.2, 0.6, 0.4, intensity); // Adwaita-like green
            } else {
                cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.1); // Subtle empty cell
            }
            rounded_rect(cr, rx, ry, cell_width, cell_height, 4);
            cairo_fill(cr);
            
            if (val > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", val);
                PangoLayout *vl = pango_cairo_create_layout(cr);
                pango_layout_set_text(vl, buf, -1);
                PangoFontDescription *vdesc = pango_font_description_from_string("Inter, Cantarell Bold 10");
                pango_layout_set_font_description(vl, vdesc);
                pango_font_description_free(vdesc);
                
                int vw, vh;
                pango_layout_get_pixel_size(vl, &vw, &vh);
                
                // Contrast text color
                if (val > max_val * 0.5) cairo_set_source_rgb(cr, 1, 1, 1);
                else cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
                
                cairo_move_to(cr, rx + (cell_width - vw)/2.0, ry + (cell_height - vh)/2.0);
                pango_cairo_show_layout(cr, vl);
                g_object_unref(vl);
            }
        }
    }
}"""
    data = data.replace(old_td_draw, new_td_draw)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
