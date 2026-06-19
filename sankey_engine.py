import sys
import re

def main():
    with open('src/visualizations.c', 'r') as f:
        data = f.read()

    # The block we are replacing starts with "static void draw_heatmap" and ends before "static gboolean on_hm_scroll"
    old_draw = re.search(r"static void draw_heatmap.*?static gboolean on_hm_scroll", data, re.DOTALL).group(0)

    new_draw = """typedef struct {
    int original_index;
    double value;
    double y;
    double dy; // height
} SankeyNode;

typedef struct {
    int source; // index in left array
    int target; // index in right array
    double value;
    double sy; // vertical offset at source
    double ty; // vertical offset at target
} SankeyLink;

static int compare_nodes_y(const void *a, const void *b) {
    SankeyNode *na = (SankeyNode *)a;
    SankeyNode *nb = (SankeyNode *)b;
    if (na->y < nb->y) return -1;
    if (na->y > nb->y) return 1;
    return 0;
}

static void compute_sankey_layout(HeatmapState *hm, SankeyNode *left, SankeyNode *right, SankeyLink *links, int *num_links, double max_height) {
    int n = hm->num_tags;
    *num_links = 0;
    
    // Calculate total flows
    double max_node_val = 1;
    for (int i = 0; i < n; i++) {
        left[i].original_index = i;
        right[i].original_index = i;
        left[i].value = 0;
        right[i].value = 0;
        for (int j = 0; j < n; j++) {
            if (i < j && hm->matrix[i][j] > 0) {
                left[i].value += hm->matrix[i][j];
                right[j].value += hm->matrix[i][j];
            } else if (i > j && hm->matrix[j][i] > 0) {
                // Symmetric
                left[i].value += hm->matrix[j][i];
                right[j].value += hm->matrix[j][i];
            }
        }
        if (left[i].value > max_node_val) max_node_val = left[i].value;
        if (right[i].value > max_node_val) max_node_val = right[i].value;
    }
    
    // Create links (we draw edges for i < j to avoid duplicates, but sankey goes left to right)
    // To make it bipartite, we just map everything to "left" and "right".
    // For every non-zero matrix[i][j] (i<j), we add a link from left[i] to right[j].
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (hm->matrix[i][j] > 0) {
                links[*num_links].source = i;
                links[*num_links].target = j;
                links[*num_links].value = hm->matrix[i][j];
                (*num_links)++;
            }
        }
    }
    
    // Base Heights
    double ky = max_height / (n * max_node_val + 0.1); // scale factor
    if (ky > 5.0) ky = 5.0; // max scale
    
    for (int i = 0; i < n; i++) {
        left[i].dy = fmax(10.0, left[i].value * ky);
        right[i].dy = fmax(10.0, right[i].value * ky);
        left[i].y = i * (left[i].dy + 15);
        right[i].y = i * (right[i].dy + 15);
    }
    
    // Relaxation iterations (Barycenter heuristic)
    for (int iter = 0; iter < 3; iter++) {
        // Right nodes
        for (int i = 0; i < n; i++) {
            double sum_y = 0;
            double sum_v = 0;
            for (int l = 0; l < *num_links; l++) {
                if (links[l].target == right[i].original_index) {
                    sum_y += left[links[l].source].y * links[l].value;
                    sum_v += links[l].value;
                }
            }
            if (sum_v > 0) right[i].y = sum_y / sum_v;
        }
        qsort(right, n, sizeof(SankeyNode), compare_nodes_y);
        
        // Push apart right nodes
        double cy = 0;
        for (int i = 0; i < n; i++) {
            if (right[i].y < cy) right[i].y = cy;
            cy = right[i].y + right[i].dy + 15;
        }
        
        // Left nodes
        for (int i = 0; i < n; i++) {
            double sum_y = 0;
            double sum_v = 0;
            for (int l = 0; l < *num_links; l++) {
                if (links[l].source == left[i].original_index) {
                    sum_y += right[links[l].target].y * links[l].value;
                    sum_v += links[l].value;
                }
            }
            if (sum_v > 0) left[i].y = sum_y / sum_v;
        }
        qsort(left, n, sizeof(SankeyNode), compare_nodes_y);
        
        // Push apart left nodes
        cy = 0;
        for (int i = 0; i < n; i++) {
            if (left[i].y < cy) left[i].y = cy;
            cy = left[i].y + left[i].dy + 15;
        }
    }
    
    // Calculate source and target Y offsets (Stacking)
    for (int i = 0; i < n; i++) {
        double sy = 0;
        for (int l = 0; l < *num_links; l++) {
            if (links[l].source == left[i].original_index) {
                links[l].sy = sy;
                sy += fmax(2.0, links[l].value * ky);
            }
        }
        double ty = 0;
        for (int l = 0; l < *num_links; l++) {
            if (links[l].target == right[i].original_index) {
                links[l].ty = ty;
                ty += fmax(2.0, links[l].value * ky);
            }
        }
    }
}

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
    
    int margin_x = max_text_w + 30;
    int offset_y = 50;
    
    cairo_scale(cr, hm->zoom, hm->zoom);
    width = width / hm->zoom;
    height = height / hm->zoom;
    
    // Allocate Sankey data
    SankeyNode *left = g_new0(SankeyNode, n);
    SankeyNode *right = g_new0(SankeyNode, n);
    SankeyLink *links = g_new0(SankeyLink, n * n);
    int num_links = 0;
    
    compute_sankey_layout(hm, left, right, links, &num_links, 600.0);
    
    double ky = 600.0 / (n * 10 + 0.1);
    if (ky > 5.0) ky = 5.0;
    
    // Draw Links
    for (int l = 0; l < num_links; l++) {
        int s = -1, t = -1;
        for (int i=0; i<n; i++) {
            if (left[i].original_index == links[l].source) s = i;
            if (right[i].original_index == links[l].target) t = i;
        }
        if (s == -1 || t == -1) continue;
        
        double x1 = margin_x;
        double y1 = offset_y + left[s].y + links[l].sy;
        double x2 = width - margin_x;
        double y2 = offset_y + right[t].y + links[l].ty;
        double link_width = fmax(2.0, links[l].value * ky);
        
        double r, g, b;
        get_category_color(hm->tag_names[links[l].source], &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.4);
        
        cairo_new_path(cr);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1, x2 - (x2-x1)/2.0, y2, x2, y2);
        cairo_line_to(cr, x2, y2 + link_width);
        cairo_curve_to(cr, x2 - (x2-x1)/2.0, y2 + link_width, x1 + (x2-x1)/2.0, y1 + link_width, x1, y1 + link_width);
        cairo_close_path(cr);
        cairo_fill(cr);
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
        
        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, margin_x - 10, y, 10, left[i].dy, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[orig], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, margin_x - 15 - tw, y + (left[i].dy - th)/2.0);
        
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
        
        cairo_set_source_rgb(cr, r, g, b);
        rounded_rect(cr, width - margin_x, y, 10, right[i].dy, 2);
        cairo_fill(cr);
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, hm->tag_names[orig], -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 10");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, width - margin_x + 15, y + (right[i].dy - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    g_free(left);
    g_free(right);
    g_free(links);
}

static gboolean on_hm_scroll"""

    data = data.replace(old_draw, new_draw)

    with open('src/visualizations.c', 'w') as f:
        f.write(data)

if __name__ == '__main__':
    main()
