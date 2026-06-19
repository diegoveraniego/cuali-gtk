
typedef struct {
    char *word;
    int freq;
} WordFreq;

typedef struct {
    double x, y, w, h;
} BoundingBox;

typedef struct {
    GList *words; // List of WordFreq
    int max_freq;
    double zoom;
    int width, height;
} WordCloudState;

static gint compare_word_freq(gconstpointer a, gconstpointer b) {
    return ((WordFreq*)b)->freq - ((WordFreq*)a)->freq;
}

static char *strip_html(const char *html) {
    if (!html) return NULL;
    GString *out = g_string_new("");
    int intag = 0;
    for (const char *p = html; *p; p++) {
        if (*p == '<') intag = 1;
        else if (*p == '>') {
            intag = 0;
            g_string_append_c(out, ' '); // Space to separate content
        } else if (!intag) {
            // Handle common entities
            if (strncmp(p, "&nbsp;", 6) == 0) { g_string_append_c(out, ' '); p += 5; }
            else if (strncmp(p, "&lt;", 4) == 0) { g_string_append_c(out, '<'); p += 3; }
            else if (strncmp(p, "&gt;", 4) == 0) { g_string_append_c(out, '>'); p += 3; }
            else if (strncmp(p, "&amp;", 5) == 0) { g_string_append_c(out, '&'); p += 4; }
            else g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

static void load_wordcloud_data(CualiAppState *app_state, WordCloudState *wc) {
    wc->words = NULL;
    wc->max_freq = 1;
    wc->zoom = 1.0;
    
    GHashTable *counts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    sqlite3_stmt *stmt = db_documents_get_all_contents(app_state->current_project_id);
    if(stmt) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *html = (const char*)sqlite3_column_text(stmt, 0);
            if(!html) continue;
            
            char *text = strip_html(html);
            char *lower = g_utf8_strdown(text, -1);
            char **words = g_strsplit_set(lower, " \n\t.,;:!?()\"'<>", -1);
            for(int i = 0; words[i] != NULL; i++) {
                if(strlen(words[i]) > 2) {
                    if(!g_hash_table_contains(stop_words_set, words[i])) {
                        gpointer val = g_hash_table_lookup(counts, words[i]);
                        int cnt = val ? GPOINTER_TO_INT(val) : 0;
                        g_hash_table_replace(counts, g_strdup(words[i]), GINT_TO_POINTER(cnt + 1));
                    }
                }
            }
            g_strfreev(words);
            g_free(lower);
            g_free(text);
        }
        sqlite3_finalize(stmt);
    }
    
    GList *keys = g_hash_table_get_keys(counts);
    for(GList *l = keys; l != NULL; l = l->next) {
        WordFreq *wf = g_new(WordFreq, 1);
        wf->word = g_strdup(l->data);
        wf->freq = GPOINTER_TO_INT(g_hash_table_lookup(counts, l->data));
        wc->words = g_list_append(wc->words, wf);
    }
    g_hash_table_destroy(counts);

    wc->words = g_list_sort(wc->words, compare_word_freq);
    if (wc->words) {
        wc->max_freq = ((WordFreq*)wc->words->data)->freq;
    }
}

static void free_wordcloud_data(WordCloudState *wc) {
    for (GList *l = wc->words; l != NULL; l = l->next) {
        WordFreq *wf = l->data;
        g_free(wf->word);
        g_free(wf);
    }
    g_list_free(wc->words);
    wc->words = NULL;
}

static gboolean check_collision(BoundingBox *boxes, int num_boxes, double x, double y, double w, double h) {
    for (int i = 0; i < num_boxes; i++) {
        if (x < boxes[i].x + boxes[i].w &&
            x + w > boxes[i].x &&
            y < boxes[i].y + boxes[i].h &&
            y + h > boxes[i].y) {
            return TRUE;
        }
    }
    return FALSE;
}

static void draw_wordcloud(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    WordCloudState *wc = (WordCloudState *)user_data;
    
    // Transparent background
    
    int max_words = 100;
    BoundingBox boxes[max_words];
    int num_boxes = 0;
    
    double center_x = width / 2.0;
    double center_y = height / 2.0;
    
    cairo_scale(cr, wc->zoom, wc->zoom);
    center_x /= wc->zoom;
    center_y /= wc->zoom;
    
    GdkRGBA fg_color;
    gtk_style_context_lookup_color(gtk_widget_get_style_context(GTK_WIDGET(area)), "theme_fg_color", &fg_color);
    
    int i = 0;
    for(GList *l = wc->words; l != NULL && i < max_words; l = l->next) {
        WordFreq *wf = l->data;
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, wf->word, -1);
        
        double scale = 1.0 + (5.0 * wf->freq / wc->max_freq);
        char font_str[64];
        snprintf(font_str, sizeof(font_str), "Inter, Cantarell %s %d", scale > 2.5 ? "Bold" : "Normal", (int)(10 * scale));
        PangoFontDescription *desc = pango_font_description_from_string(font_str);
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int tw, th;
        pango_layout_get_pixel_size(layout, &tw, &th);
        
        // Archimedean spiral placement
        double angle = 0;
        double radius = 0;
        double x = center_x - tw/2.0;
        double y = center_y - th/2.0;
        
        while (check_collision(boxes, num_boxes, x, y, tw, th)) {
            radius += 1.0;
            angle += 0.5;
            x = center_x + radius * cos(angle) - tw/2.0;
            y = center_y + radius * sin(angle) - th/2.0;
        }
        
        boxes[num_boxes].x = x;
        boxes[num_boxes].y = y;
        boxes[num_boxes].w = tw;
        boxes[num_boxes].h = th;
        num_boxes++;
        
        cairo_move_to(cr, x, y);
        
        double opacity = fmax(0.3, (double)wf->freq / wc->max_freq);
        if (i == 0) opacity = 1.0; // Max word is fully opaque
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, opacity);
        
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        i++;
    }
}

static gboolean on_wc_scroll(GtkEventControllerScroll *scroll, double dx, double dy, gpointer user_data) {
    WordCloudState *wc = (WordCloudState *)user_data;
    if (dy > 0) wc->zoom *= 0.9;
    else if (dy < 0) wc->zoom *= 1.1;
    wc->zoom = fmax(0.1, fmin(wc->zoom, 5.0));
    
    GtkWidget *area = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(scroll));
    gtk_widget_queue_draw(area);
    return TRUE;
}

static WordCloudState *g_wc_state = NULL;
static GtkWidget *g_wc_area = NULL;

GtkWidget* create_wordcloud_view(CualiAppState *state) {
    init_stopwords();
    
    WordCloudState *wc = g_new0(WordCloudState, 1);
    load_wordcloud_data(state, wc);
    g_wc_state = wc;
    
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_wordcloud, wc, NULL);
    gtk_widget_set_size_request(area, 800, 600);
    g_wc_area = area;
    
    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_wc_scroll), wc);
    gtk_widget_add_controller(area, scroll_ctrl);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), area);
    return scroll;
}

// --- Main Visualizations View ---
static void on_filter_changed(GtkRange *range, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    refresh_visualizations(state);
}

GtkWidget* create_visualizations_view(CualiAppState *state) {
    GtkWidget *toolbar_view = adw_toolbar_view_new();
    
