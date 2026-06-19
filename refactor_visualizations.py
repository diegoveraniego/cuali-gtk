import sys

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # Step 1: Add pango and libadwaita
    data = data.replace('#include <cairo.h>', '#include <cairo.h>\n#include <pango/pangocairo.h>\n#include <adwaita.h>')

    # Step 2: Update Whiteboard
    old_wb_draw = """static void draw_whiteboard(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    state->width = width;
    state->height = height;
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // background
    cairo_paint(cr);
    
    cairo_translate(cr, state->pan_x, state->pan_y);
    cairo_scale(cr, state->zoom, state->zoom);
    
    cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 0.6);
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *edge = l->data;
        GraphNode *n1 = find_node(state, edge->source_id);
        GraphNode *n2 = find_node(state, edge->target_id);
        if (n1 && n2) {
            cairo_set_line_width(cr, fmax(1.0, log(edge->weight + 1) * 2));
            cairo_move_to(cr, n1->x, n1->y);
            double dx = (n2->x - n1->x) / 2.0;
            cairo_curve_to(cr, n1->x + dx, n1->y, n2->x - dx, n2->y, n2->x, n2->y);
            cairo_stroke(cr);
        }
    }
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        
        GdkRGBA rgba;
        gdk_rgba_parse(&rgba, n->color ? n->color : "#77767b");
        cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
        
        cairo_arc(cr, n->x, n->y, n->radius, 0, 2 * G_PI);
        cairo_fill_preserve(cr);
        
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, n->name, &extents);
        cairo_move_to(cr, n->x - extents.width/2.0, n->y + n->radius + 18);
        cairo_show_text(cr, n->name);
    }
}"""

    new_wb_draw = """static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI/2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI/2);
    cairo_arc(cr, x + r, y + h - r, r, G_PI/2, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3*G_PI/2);
    cairo_close_path(cr);
}

static void draw_whiteboard(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    state->width = width;
    state->height = height;
    
    // Background is transparent to respect Adwaita theme
    cairo_translate(cr, state->pan_x, state->pan_y);
    cairo_scale(cr, state->zoom, state->zoom);
    
    // Draw edges
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.4);
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *edge = l->data;
        GraphNode *n1 = find_node(state, edge->source_id);
        GraphNode *n2 = find_node(state, edge->target_id);
        if (n1 && n2) {
            cairo_set_line_width(cr, fmax(1.5, log(edge->weight + 1) * 2));
            cairo_move_to(cr, n1->x, n1->y);
            double dx = (n2->x - n1->x) / 2.0;
            cairo_curve_to(cr, n1->x + dx, n1->y, n2->x - dx, n2->y, n2->x, n2->y);
            cairo_stroke(cr);
        }
    }
    
    // Draw Nodes
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, n->name, -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell Bold 11");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int text_w, text_h;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);
        
        double node_w = text_w + 32; // padding + icon space
        double node_h = 32;
        double nx = n->x - node_w/2.0;
        double ny = n->y - node_h/2.0;
        
        // Node shadow
        cairo_set_source_rgba(cr, 0, 0, 0, 0.15);
        rounded_rect(cr, nx + 2, ny + 2, node_w, node_h, 8.0);
        cairo_fill(cr);
        
        // Node background
        GdkRGBA rgba;
        gdk_rgba_parse(&rgba, n->color ? n->color : "#3584e4");
        cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
        rounded_rect(cr, nx, ny, node_w, node_h, 8.0);
        cairo_fill_preserve(cr);
        
        cairo_set_source_rgba(cr, 1, 1, 1, 0.2);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        
        // Icon (Diamond / Tag symbol)
        cairo_set_source_rgb(cr, 1, 1, 1); // White text inside colored pill
        cairo_move_to(cr, nx + 8, ny + 12);
        cairo_line_to(cr, nx + 14, ny + 18);
        cairo_line_to(cr, nx + 20, ny + 12);
        cairo_stroke(cr);
        
        // Text
        cairo_move_to(cr, nx + 24, ny + (node_h - text_h)/2.0);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        
        // Set radius for dragging calculations
        n->radius = fmax(node_w, node_h)/2.0;
    }
}"""
    data = data.replace(old_wb_draw, new_wb_draw)
    
    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
