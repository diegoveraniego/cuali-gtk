static void draw_tagdoc(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    TagDocState *td = (TagDocState *)user_data;
    
    int cell_width = 45;
    int cell_height = 30;
    int gap = 2;
    
    int max_val = 1;
    for(int i = 0; i < td->num_tags; i++) {
        for(int j = 0; j < td->num_docs; j++) {
            if(td->matrix[i][j] > max_val) max_val = td->matrix[i][j];
        }
    }
    
    // Calculate max text width for tags
    int max_tag_w = 0;
    for(int i = 0; i < td->num_tags; i++) {
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, td->tag_names[i], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        if (tw > max_tag_w) max_tag_w = tw;
        pango_font_description_free(desc);
        g_object_unref(layout);
    }
    
    // Calculate max text width for docs (angled)
    int max_doc_w = 0;
    for(int j = 0; j < td->num_docs; j++) {
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, td->doc_names[j], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        if (tw > max_doc_w) max_doc_w = tw;
        pango_font_description_free(desc);
        g_object_unref(layout);
    }
    
    // Angled at 45 degrees, vertical space needed is sin(45) * width
    int doc_header_height = (int)(max_doc_w * 0.707) + 40;
    
    int base_offset_x = max_tag_w + 40;
    int total_matrix_w = td->num_docs * (cell_width + gap);
    int total_viz_w = base_offset_x + total_matrix_w;
    
    int offset_x = base_offset_x;
    int offset_y = doc_header_height;
    
    // Centering calculation for the whole block
    if (total_viz_w < width) {
        offset_x += (width - total_viz_w) / 2;
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
        cairo_move_to(cr, offset_x - 20 - tw, offset_y + i * (cell_height + gap) + (cell_height - th)/2.0);
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        
        for(int j = 0; j < td->num_docs; j++) {
            if (i == 0) {
                cairo_save(cr);
                // Position for angled doc labels: center of cell, slightly above matrix
                cairo_translate(cr, offset_x + j * (cell_width + gap) + cell_width/2.0, offset_y - 10);
                cairo_rotate(cr, -G_PI / 4.0);
                
                PangoLayout *hl = pango_cairo_create_layout(cr);
                pango_layout_set_text(hl, td->doc_names[j], -1);
                PangoFontDescription *hdesc = pango_font_description_from_string("Inter, Cantarell 10");
                pango_layout_set_font_description(hl, hdesc);
                pango_font_description_free(hdesc);
                
                int dtw, dth;
                pango_layout_get_pixel_size(hl, &dtw, &dth);
                // Move text so its end (right side) is near the anchor point if rotating, 
                // but here we just want it to extend upwards.
                cairo_move_to(cr, 0, -5); 
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
