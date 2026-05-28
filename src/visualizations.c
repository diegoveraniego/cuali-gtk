#include "visualizations.h"
#include "database.h"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <adwaita.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>

// --- Stopwords ---
static const char *STOPWORDS_ES[] = {
    "el", "la", "los", "las", "un", "una", "unos", "unas",
    "de", "del", "a", "al", "en", "por", "para", "con", "sin", "sobre", "entre", "hacia", "hasta", "desde",
    "y", "e", "ni", "o", "u", "pero", "aunque", "mas", "sino", "porque", "como", "cuando", "donde", "quien",
    "que", "cual", "cuales", "quienes", "cuyo", "cuya", "cuyos", "cuyas",
    "yo", "tu", "el", "ella", "ello", "nosotros", "nosotras", "vosotros", "vosotras", "ellos", "ellas",
    "me", "te", "se", "nos", "os", "le", "les", "lo", "la", "los", "las",
    "mi", "tu", "su", "nuestro", "vuestro", "mis", "tus", "sus", "nuestros", "vuestros",
    "este", "esta", "estos", "estas", "ese", "esa", "esos", "esas", "aquel", "aquella", "aquellos", "aquellas",
    "ser", "es", "son", "era", "eran", "fui", "fue", "fueron", "siendo", "sido",
    "estar", "estoy", "esta", "estan", "estaba", "estaban", "estuve", "estuvo", "estuvieron",
    "tener", "tengo", "tiene", "tienen", "tenia", "tenian", "tuve", "tuvo", "tuvieron",
    "haber", "he", "has", "ha", "hemos", "han", "habia", "habian", "hube", "hubo", "hubieron",
    "hacer", "hace", "hacen", "hacia", "hacian", "hizo", "hicieron",
    "poder", "puedo", "puede", "pueden", "podia", "podian", "pudo", "pudieron",
    "decir", "digo", "dice", "dicen", "decia", "decian", "dijo", "dijeron",
    "ir", "voy", "va", "van", "iba", "iban", "fui", "fue", "fueron",
    "ver", "veo", "ve", "ven", "veia", "veian", "vi", "vio", "vieron",
    "dar", "doy", "da", "dan", "daba", "daban", "di", "dio", "dieron",
    "saber", "se", "sabe", "saben", "sabia", "sabian", "supo", "supieron",
    "querer", "quiero", "quiere", "quieren", "queria", "querian", "quiso", "quisieron",
    "llegar", "llegar", "llego", "llegan", "llegaba", "llegaban", "llegue", "llegaron",
    "pasar", "paso", "pasa", "pasan", "pasaba", "pasaban", "pasaron",
    "deber", "debo", "debe", "deben", "debia", "debian", "debio", "debieron",
    "poner", "pongo", "pone", "ponen", "ponia", "ponian", "puso", "pusieron",
    "parecer", "parezco", "parece", "parecen", "parecia", "parecian", "parecio", "parecieron",
    "quedar", "quedo", "queda", "quedan", "quedaba", "quedaban", "quedaron",
    "creer", "creo", "cree", "creen", "creia", "creian", "creyo", "creyeron",
    "hablar", "hablo", "habla", "hablan", "hablaba", "hablaban", "hablaron",
    "llevar", "llevo", "lleva", "llevan", "llevaba", "llevaban", "llevaron",
    "dejar", "dejo", "deja", "dejan", "dejaba", "dejaban", "dejaron",
    "seguir", "sigo", "sigue", "siguen", "seguia", "seguian", "siguio", "siguieron",
    "encontrar", "encuentro", "encuentra", "encuentran", "encontraba", "encontraban", "encontro", "encontraron",
    "llamar", "llamo", "llama", "llaman", "llamaba", "llamaban", "llamaron",
    "venir", "vengo", "viene", "vienen", "venia", "venian", "vino", "vinieron",
    "pensar", "pienso", "piensa", "piensan", "pensaba", "pensaban", "penso", "pensaron",
    "salir", "salgo", "sale", "salen", "salia", "salian", "salio", "salieron",
    "volver", "vuelvo", "vuelve", "vuelven", "volvia", "volvian", "volvio", "volvieron",
    "tomar", "tomo", "toma", "toman", "tomaba", "tomaban", "tomaron",
    "conocer", "conozco", "conoce", "conocen", "conocia", "conocian", "conocio", "conocieron",
    "vivir", "vivo", "vive", "viven", "vivia", "vivian", "vivio", "vivieron",
    "sentir", "siento", "siente", "sienten", "sentia", "sentian", "sintio", "sintieron",
    "tratar", "trato", "trata", "tratan", "trataba", "trataban", "trataron",
    "mirar", "miro", "mira", "miran", "miraba", "miraban", "miraron",
    "contar", "cuento", "cuenta", "cuentan", "contaba", "contaban", "conto", "contaron",
    "empezar", "empiezo", "empieza", "empiezan", "empezaba", "empezaban", "empezo", "empezaron",
    "esperar", "espero", "espera", "esperan", "esperaba", "esperaban", "espero", "esperaron",
    "buscar", "busco", "busca", "buscan", "buscaba", "buscaban", "busco", "buscaron",
    "existir", "existo", "existe", "existen", "existia", "existian", "existio", "existieron",
    "entrar", "entro", "entra", "entran", "entraba", "entraban", "entraron",
    "trabajar", "trabajo", "trabaja", "trabajan", "trabajaba", "trabajaban", "trabajaron",
    "escribir", "escribo", "escribe", "escriben", "escribia", "escribian", "escribio", "escribieron",
    "perder", "pierdo", "pierde", "pierden", "perdia", "perdian", "perdio", "perdieron",
    "producir", "produzco", "produce", "producen", "producia", "producian", "produjo", "produjeron",
    "ocurrir", "ocurro", "ocurre", "ocurren", "ocurria", "ocurrian", "ocurrio", "ocurrieron",
    "entender", "entiendo", "entiende", "entienden", "entendia", "entendian", "entendio", "entendieron",
    "pedir", "pido", "pide", "piden", "pedia", "pedian", "pidio", "pidieron",
    "recibir", "recibo", "recibe", "reciben", "recibia", "recibian", "recibio", "recibieron",
    "recordar", "recuerdo", "recuerda", "recuerdan", "recordaba", "recordaban", "recordo", "recordaron",
    "terminar", "termino", "termina", "terminan", "terminaba", "terminaban", "terminaron",
    "permitir", "permito", "permite", "permiten", "permitia", "permitian", "permitio", "permitieron",
    "aparecer", "aparezco", "aparece", "aparecen", "aparecia", "aparecian", "aparecio", "aparecieron",
    "conseguir", "consigo", "consigue", "consiguen", "conseguia", "conseguian", "consiguio", "consiguieron",
    "comenzar", "comienzo", "comienza", "comienzan", "comenzaba", "comenzaban", "comenzo", "comenzaron",
    "servir", "sirvo", "sirve", "sirven", "servia", "servian", "sirvio", "sirvieron",
    "sacar", "saco", "saca", "sacan", "sacaba", "sacaban", "saco", "sacaron",
    "necesitar", "necesito", "necesita", "necesitan", "necesitaba", "necesitaban", "necesitaron",
    "mantener", "mantengo", "mantiene", "mantienen", "mantenia", "mantenian", "mantuvo", "mantuvieron",
    "resultar", "resulto", "resulta", "resultan", "resultaba", "resultaban", "resultaron",
    "leer", "leo", "lee", "leen", "leia", "leian", "leyo", "leyeron",
    "caer", "caigo", "cae", "caen", "caia", "caian", "cayo", "cayeron",
    "cambiar", "cambio", "cambia", "cambian", "cambiaba", "cambiaban", "cambiaron",
    "presentar", "presento", "presenta", "presentan", "presentaba", "presentaban", "presentaron",
    "crear", "creo", "crea", "crean", "creaba", "creaban", "crearon",
    "abrir", "abro", "abre", "abren", "abria", "abrian", "abrio", "abrieron",
    "considerar", "considero", "considera", "consideran", "consideraba", "consideraban", "consideraron",
    "oir", "oigo", "oye", "oyen", "oia", "oian", "oyo", "oyeron",
    "acabar", "acabo", "acaba", "acaban", "acababa", "acababan", "acabaron",
    "convertir", "convierto", "convierte", "convierten", "convertia", "convertian", "convirtio", "convirtieron",
    "ganar", "gano", "gana", "ganan", "ganaba", "ganaban", "ganaron",
    "formar", "formo", "forma", "forman", "formaba", "formaban", "formaron",
    "traer", "traigo", "trae", "traen", "traia", "traian", "trajo", "trajeron",
    "partir", "parto", "parte", "parten", "partia", "partian", "partio", "partieron",
    "morir", "muero", "muere", "mueren", "moria", "morian", "murio", "murieron",
    "aceptar", "acepto", "acepta", "aceptan", "aceptaba", "aceptaban", "aceptaron",
    "realizar", "realizo", "realiza", "realizan", "realizaba", "realizaban", "realizaron",
    "suponer", "supongo", "supone", "suponen", "suponia", "suponian", "supuso", "supusieron",
    "comprender", "comprendo", "comprende", "comprenden", "comprendia", "comprendian", "comprendio", "comprendieron",
    "lograr", "logro", "logra", "logran", "lograba", "lograban", "lograron",
    "explicar", "explico", "explica", "explican", "explicaba", "explicaban", "explicaron",
    "preguntar", "pregunto", "pregunta", "preguntan", "preguntaba", "preguntaban", "preguntaron",
    "tocar", "toco", "toca", "tocan", "tocaba", "tocaban", "toco", "tocaron",
    "reconocer", "reconozco", "reconoce", "reconocen", "reconocia", "reconocian", "reconocio", "reconocieron",
    "estudiar", "estudio", "estudia", "estudian", "estudiaba", "estudiaban", "estudiaron",
    "alcanzar", "alcanzo", "alcanza", "alcanzan", "alcanzaba", "alcanzaban", "alcanzaron",
    "nacer", "nazco", "nace", "nacen", "nacia", "nacian", "nacio", "nacieron",
    "dirigir", "dirijo", "dirige", "dirigen", "dirigia", "dirigian", "dirigio", "dirigieron",
    "correr", "corro", "corre", "corren", "corria", "corrian", "corrio", "corrieron",
    "utilizar", "utilizo", "utiliza", "utilizan", "utilizaba", "utilizaban", "utilizaron",
    "pagar", "pago", "paga", "pagan", "pagaba", "pagaban", "pagaron",
    "ayudar", "ayudo", "ayuda", "ayudan", "ayudaba", "ayudaban", "ayudaron",
    "gustar", "gusto", "gusta", "gustan", "gustaba", "gustaban", "gustaron",
    "jugar", "juego", "juega", "juegan", "jugaba", "jugaban", "jugaron",
    "escuchar", "escucho", "escucha", "escuchan", "escuchaba", "escuchaban", "escucharon",
    "cumplir", "cumplo", "cumple", "cumplen", "cumplia", "cumplian", "cumplio", "cumplieron",
    "ofrecer", "ofrezco", "ofrece", "ofrecen", "ofrecia", "ofrecian", "ofrecio", "ofrecieron",
    "descubrir", "descubro", "descubre", "descubren", "descubria", "descubrian", "descubrio", "descubrieron",
    "levantar", "levanto", "levanta", "levantan", "levantaba", "levantaban", "levantaron",
    "intentar", "intento", "intenta", "intentan", "intentaba", "intentaban", "intentaron",
    "usar", "uso", "usa", "usan", "usaba", "usaban", "usaron",
    "si", "no", "mas", "muy", "mucho", "poco", "tanto", "tambien", "tampoco", "nada", "todo", "asi",
    "bien", "mal", "solo", "solamente", "siempre", "nunca", "jamas", "ya", "ahora", "todavia", "aun",
    "antes", "despues", "luego", "pronto", "tarde", "temprano", "ayer", "hoy", "manana", "aqui", "alli",
    "alla", "cerca", "lejos", "dentro", "fuera", "arriba", "abajo", "delante", "detras", "encima", "debajo",
    "algun", "alguna", "algunos", "algunas", "ningun", "ninguna", "ningunos", "ningunas", "cualquier",
    "cualquiera", "quienquiera", "tal", "tales", "demas", "mismo", "misma", "mismos", "mismas",
    "cada", "varios", "varias", "otro", "otra", "otros", "otras",
    NULL
};

static GHashTable *stop_words_set = NULL;
static void init_stopwords() {
    if (stop_words_set) return;
    stop_words_set = g_hash_table_new(g_str_hash, g_str_equal);
    for (int i = 0; STOPWORDS_ES[i] != NULL; i++) {
        g_hash_table_add(stop_words_set, (gpointer)STOPWORDS_ES[i]);
    }
}

// --- Data Structures ---

typedef struct {
    int id;
    char *name;
    char *color;
    double x, y;
    double vx, vy;
    double radius;
} GraphNode;

typedef struct {
    int source_id;
    int target_id;
    int weight;
} GraphEdge;

typedef struct {
    GList *nodes;
    GList *edges;
    double zoom;
    double pan_x, pan_y;
    GraphNode *drag_node;
    int width, height;
} WhiteboardState;

static GtkWidget *g_wb_area = NULL;
static WhiteboardState *g_wb_state = NULL;
static GtkWidget *g_hm_area = NULL;
static void *g_hm_state = NULL;
static GtkWidget *g_mat_area = NULL;
static void *g_mat_state = NULL;


// --- Whiteboard Logic ---

static GraphNode* find_node(WhiteboardState *state, int id) {
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        if (n->id == id) return n;
    }
    return NULL;
}

static GraphNode* find_node_at(WhiteboardState *state, double x, double y) {
    GraphNode *best = NULL;
    double best_dist = 1e9;
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        double dx = n->x - x;
        double dy = n->y - y;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist <= n->radius && dist < best_dist) {
            best = n;
            best_dist = dist;
        }
    }
    return best;
}

static void apply_force_directed(WhiteboardState *state) {
    double k_repel = 5000.0;
    double k_spring = 0.05;
    double damping = 0.85;
    double ideal_length = 150.0;
    
    // Repulsion
    for (GList *i = state->nodes; i != NULL; i = i->next) {
        GraphNode *n1 = i->data;
        for (GList *j = i->next; j != NULL; j = j->next) {
            GraphNode *n2 = j->data;
            double dx = n1->x - n2->x;
            double dy = n1->y - n2->y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist > 0.1 && dist < 1000.0) {
                double force = k_repel / (dist * dist);
                double fx = (dx / dist) * force;
                double fy = (dy / dist) * force;
                n1->vx += fx; n1->vy += fy;
                n2->vx -= fx; n2->vy -= fy;
            }
        }
    }
    
    // Spring (Edges)
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *e = l->data;
        GraphNode *n1 = find_node(state, e->source_id);
        GraphNode *n2 = find_node(state, e->target_id);
        if (n1 && n2) {
            double dx = n2->x - n1->x;
            double dy = n2->y - n1->y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist > 0.1) {
                double force = k_spring * (dist - ideal_length);
                double fx = (dx / dist) * force;
                double fy = (dy / dist) * force;
                n1->vx += fx; n1->vy += fy;
                n2->vx -= fx; n2->vy -= fy;
            }
        }
    }
    
    // Center gravity
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        double dx = (state->width/2.0) - n->x;
        double dy = (state->height/2.0) - n->y;
        n->vx += dx * 0.01;
        n->vy += dy * 0.01;
        
        n->vx *= damping;
        n->vy *= damping;
        n->x += n->vx;
        n->y += n->vy;
    }
}

static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
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
}

static void on_wb_drag_begin(GtkGestureDrag *gesture, double x, double y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    double logical_x = (x - state->pan_x) / state->zoom;
    double logical_y = (y - state->pan_y) / state->zoom;
    state->drag_node = find_node_at(state, logical_x, logical_y);
}

static void on_wb_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    double start_x, start_y;
    gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);
    
    if (state->drag_node) {
        state->drag_node->x = (start_x + offset_x - state->pan_x) / state->zoom;
        state->drag_node->y = (start_y + offset_y - state->pan_y) / state->zoom;
    } else {
        // Panning not fully implemented in this minimal drag for simplicity
    }
    gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));
}

static void on_wb_drag_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    state->drag_node = NULL;
}

static gboolean on_wb_scroll(GtkEventControllerScroll *scroll, double dx, double dy, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    if (dy > 0) state->zoom *= 0.9;
    else if (dy < 0) state->zoom *= 1.1;
    
    state->zoom = fmax(0.1, fmin(state->zoom, 5.0));
    gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(scroll)));
    return TRUE;
}

static void load_whiteboard_data(CualiAppState *app_state, WhiteboardState *wb) {
    wb->nodes = NULL;
    wb->edges = NULL;
    wb->zoom = 1.0;
    wb->pan_x = 0.0;
    wb->pan_y = 0.0;
    wb->drag_node = NULL;
    
    sqlite3_stmt *stmt = db_tags_get_all(app_state->current_project_id);
    if (stmt) {
        int idx = 0;
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            GraphNode *n = g_new0(GraphNode, 1);
            n->id = sqlite3_column_int(stmt, 0);
            n->name = g_strdup((const char *)sqlite3_column_text(stmt, 1));
            n->color = g_strdup((const char *)sqlite3_column_text(stmt, 2));
            n->radius = 20.0;
            // Random start around center
            n->x = 400 + (rand() % 400 - 200);
            n->y = 300 + (rand() % 400 - 200);
            n->vx = 0; n->vy = 0;
            wb->nodes = g_list_append(wb->nodes, n);
            idx++;
        }
        sqlite3_finalize(stmt);
    }
    
    sqlite3_stmt *estmt = db_tags_get_cooccurrence(app_state->current_project_id);
    if (estmt) {
        while(sqlite3_step(estmt) == SQLITE_ROW) {
            GraphEdge *e = g_new0(GraphEdge, 1);
            e->source_id = sqlite3_column_int(estmt, 0);
            e->target_id = sqlite3_column_int(estmt, 1);
            e->weight = sqlite3_column_int(estmt, 2);
            wb->edges = g_list_append(wb->edges, e);
        }
        sqlite3_finalize(estmt);
    }
    
    // Run physics initially to settle
    wb->width = 800; wb->height = 600;
    for(int i = 0; i < 200; i++) {
        apply_force_directed(wb);
    }
}

static void free_whiteboard_data(WhiteboardState *wb) {
    for (GList *l = wb->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        g_free(n->name);
        g_free(n->color);
        g_free(n);
    }
    g_list_free(wb->nodes);
    wb->nodes = NULL;
    
    for (GList *l = wb->edges; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(wb->edges);
    wb->edges = NULL;
}

GtkWidget* create_whiteboard_view(CualiAppState *state) {
    WhiteboardState *wb = g_new0(WhiteboardState, 1);
    load_whiteboard_data(state, wb);
    
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_whiteboard, wb, NULL); // Note: Should free wb on destroy
    
    GtkGesture *drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_wb_drag_begin), wb);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_wb_drag_update), wb);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_wb_drag_end), wb);
    gtk_widget_add_controller(area, GTK_EVENT_CONTROLLER(drag));
    
    GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_wb_scroll), wb);
    gtk_widget_add_controller(area, scroll);
    
    return area;
}

// --- Matrix Heatmap Logic ---

typedef struct {
    int val;
} Cell;

typedef struct {
    char **tag_names;
    int *tag_ids;
    int num_tags;
    int **matrix; // NxN
    double zoom;
} HeatmapState;

static void load_heatmap_data(CualiAppState *app_state, HeatmapState *hm) {
    // Count tags
    sqlite3_stmt *tstmt = db_tags_get_all(app_state->current_project_id);
    hm->zoom = 1.0;
    hm->num_tags = 0;
    while(tstmt && sqlite3_step(tstmt) == SQLITE_ROW) hm->num_tags++;
    if(tstmt) sqlite3_finalize(tstmt);
    
    hm->tag_names = g_new0(char*, hm->num_tags);
    hm->tag_ids = g_new0(int, hm->num_tags);
    hm->matrix = g_new0(int*, hm->num_tags);
    for(int i = 0; i < hm->num_tags; i++) hm->matrix[i] = g_new0(int, hm->num_tags);
    
    tstmt = db_tags_get_all(app_state->current_project_id);
    int idx = 0;
    if (tstmt) {
        while(sqlite3_step(tstmt) == SQLITE_ROW) {
            hm->tag_ids[idx] = sqlite3_column_int(tstmt, 0);
            hm->tag_names[idx] = g_strdup((const char*)sqlite3_column_text(tstmt, 1));
            idx++;
        }
        sqlite3_finalize(tstmt);
    }
    
    // Fill matrix
    sqlite3_stmt *estmt = db_tags_get_cooccurrence(app_state->current_project_id);
    if(estmt) {
        while(sqlite3_step(estmt) == SQLITE_ROW) {
            int t1 = sqlite3_column_int(estmt, 0);
            int t2 = sqlite3_column_int(estmt, 1);
            int weight = sqlite3_column_int(estmt, 2);
            
            int i1 = -1, i2 = -1;
            for(int i = 0; i < hm->num_tags; i++) {
                if(hm->tag_ids[i] == t1) i1 = i;
                if(hm->tag_ids[i] == t2) i2 = i;
            }
            if(i1 >= 0 && i2 >= 0) {
                hm->matrix[i1][i2] = weight;
                hm->matrix[i2][i1] = weight;
            }
        }
        sqlite3_finalize(estmt);
    }
}

static void free_heatmap_data(HeatmapState *hm) {
    for(int i = 0; i < hm->num_tags; i++) {
        g_free(hm->tag_names[i]);
        g_free(hm->matrix[i]);
    }
    g_free(hm->tag_names);
    g_free(hm->tag_ids);
    g_free(hm->matrix);
    hm->tag_names = NULL; hm->tag_ids = NULL; hm->matrix = NULL;
    hm->num_tags = 0;
}

static void get_category_color(const char *tag_name, double *r, double *g, double *b) {
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

typedef struct {
    int original_index;
    double value;
    double y;
    double dy; // height
} SankeyNode;

typedef struct {
    int source; // original_index in left array
    int target; // original_index in right array
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

// Helper to find node by original index in an array of nodes
static int find_node_by_orig(SankeyNode *nodes, int n, int orig) {
    for (int i = 0; i < n; i++) {
        if (nodes[i].original_index == orig) return i;
    }
    return -1;
}

static void compute_sankey_layout(HeatmapState *hm, SankeyNode *left, SankeyNode *right, SankeyLink *links, int *num_links, double max_height) {
    int n = hm->num_tags;
    *num_links = 0;
    
    // Calculate total flows
    for (int i = 0; i < n; i++) {
        left[i].original_index = i;
        right[i].original_index = i;
        left[i].value = 0;
        right[i].value = 0;
        for (int j = 0; j < n; j++) {
            if (hm->matrix[i][j] > 0) {
                left[i].value += hm->matrix[i][j];
                right[j].value += hm->matrix[i][j];
            }
        }
    }
    
    // Create links (Full symmetric co-occurrence)
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && hm->matrix[i][j] > 0) {
                links[*num_links].source = i;
                links[*num_links].target = j;
                links[*num_links].value = hm->matrix[i][j];
                (*num_links)++;
            }
        }
    }
    
    // Calculate scale factor ky
    double total_val = 0;
    for (int i = 0; i < n; i++) total_val += left[i].value;
    
    double node_padding = 15.0;
    double ky = (max_height - (n - 1) * node_padding) / (total_val + 0.1);
    if (ky > 15.0) ky = 15.0;
    if (ky < 1.0) ky = 1.0;
    
    for (int i = 0; i < n; i++) {
        left[i].dy = fmax(2.0, left[i].value * ky);
        right[i].dy = fmax(2.0, right[i].value * ky);
        left[i].y = i * (max_height / (double)n);
        right[i].y = i * (max_height / (double)n);
    }
    
    // Relaxation iterations (Barycenter heuristic)
    for (int iter = 0; iter < 32; iter++) {
        // Right nodes
        for (int i = 0; i < n; i++) {
            double sum_y = 0;
            double sum_v = 0;
            for (int l = 0; l < *num_links; l++) {
                if (links[l].target == right[i].original_index) {
                    int s_idx = find_node_by_orig(left, n, links[l].source);
                    if (s_idx >= 0) {
                        sum_y += (left[s_idx].y + left[s_idx].dy / 2.0) * links[l].value;
                        sum_v += links[l].value;
                    }
                }
            }
            if (sum_v > 0) right[i].y = (sum_y / sum_v) - right[i].dy / 2.0;
        }
        qsort(right, n, sizeof(SankeyNode), compare_nodes_y);
        
        // Push apart right nodes
        double cy = 0;
        for (int i = 0; i < n; i++) {
            if (right[i].y < cy) right[i].y = cy;
            cy = right[i].y + right[i].dy + node_padding;
        }
        if (cy > max_height + node_padding && n > 1) {
            double ratio = max_height / (cy - node_padding);
            for(int i=0; i<n; i++) { right[i].y *= ratio; right[i].dy *= ratio; }
        }
        
        // Left nodes
        for (int i = 0; i < n; i++) {
            double sum_y = 0;
            double sum_v = 0;
            for (int l = 0; l < *num_links; l++) {
                if (links[l].source == left[i].original_index) {
                    int t_idx = find_node_by_orig(right, n, links[l].target);
                    if (t_idx >= 0) {
                        sum_y += (right[t_idx].y + right[t_idx].dy / 2.0) * links[l].value;
                        sum_v += links[l].value;
                    }
                }
            }
            if (sum_v > 0) left[i].y = (sum_y / sum_v) - left[i].dy / 2.0;
        }
        qsort(left, n, sizeof(SankeyNode), compare_nodes_y);
        
        // Push apart left nodes
        cy = 0;
        for (int i = 0; i < n; i++) {
            if (left[i].y < cy) left[i].y = cy;
            cy = left[i].y + left[i].dy + node_padding;
        }
        if (cy > max_height + node_padding && n > 1) {
            double ratio = max_height / (cy - node_padding);
            for(int i=0; i<n; i++) { left[i].y *= ratio; left[i].dy *= ratio; }
        }
    }
    
    // Calculate source and target Y offsets (Stacking with sorting)
    for (int i = 0; i < n; i++) {
        int node_links_count = 0;
        int node_links[n*2]; // increased size just in case
        for (int l = 0; l < *num_links; l++) {
            if (links[l].source == left[i].original_index) {
                node_links[node_links_count++] = l;
            }
        }
        // Sort by target position
        for(int m=0; m<node_links_count; m++) {
            for(int k=m+1; k<node_links_count; k++) {
                int l1 = node_links[m];
                int l2 = node_links[k];
                int t1 = find_node_by_orig(right, n, links[l1].target);
                int t2 = find_node_by_orig(right, n, links[l2].target);
                if (right[t1].y > right[t2].y) {
                    int tmp = node_links[m];
                    node_links[m] = node_links[k];
                    node_links[k] = tmp;
                }
            }
        }
        double sy = 0;
        for (int m = 0; m < node_links_count; m++) {
            int l = node_links[m];
            links[l].sy = sy;
            links[l].value = fmax(0.1, links[l].value);
            sy += (links[l].value / left[i].value) * left[i].dy;
        }
    }
    
    for (int i = 0; i < n; i++) {
        int node_links_count = 0;
        int node_links[n*2];
        for (int l = 0; l < *num_links; l++) {
            if (links[l].target == right[i].original_index) {
                node_links[node_links_count++] = l;
            }
        }
        // Sort by source position
        for(int m=0; m<node_links_count; m++) {
            for(int k=m+1; k<node_links_count; k++) {
                int l1 = node_links[m];
                int l2 = node_links[k];
                int s1 = find_node_by_orig(left, n, links[l1].source);
                int s2 = find_node_by_orig(left, n, links[l2].source);
                if (left[s1].y > left[s2].y) {
                    int tmp = node_links[m];
                    node_links[m] = node_links[k];
                    node_links[k] = tmp;
                }
            }
        }
        double ty = 0;
        for (int m = 0; m < node_links_count; m++) {
            int l = node_links[m];
            links[l].ty = ty;
            ty += (links[l].value / right[i].value) * right[i].dy;
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
    
    int margin_x = max_text_w + 40;
    int offset_y = 50;
    
    cairo_scale(cr, hm->zoom, hm->zoom);
    width = width / hm->zoom;
    height = height / hm->zoom;
    
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
        
        double x1 = margin_x;
        double y1 = offset_y + left[s].y + links[l].sy;
        double x2 = width - margin_x;
        double y2 = offset_y + right[t].y + links[l].ty;
        double link_width = (links[l].value / left[s].value) * left[s].dy;
        
        double r, g, b;
        get_category_color(hm->tag_names[links[l].source], &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.3);
        
        cairo_new_path(cr);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1, x2 - (x2-x1)/2.0, y2, x2, y2);
        cairo_line_to(cr, x2, y2 + link_width);
        cairo_curve_to(cr, x2 - (x2-x1)/2.0, y2 + link_width, x1 + (x2-x1)/2.0, y1 + link_width, x1, y1 + link_width);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Border for the ribbon
        cairo_set_source_rgba(cr, r, g, b, 0.1);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1, x2 - (x2-x1)/2.0, y2, x2, y2);
        cairo_stroke(cr);
        cairo_move_to(cr, x1, y1 + link_width);
        cairo_curve_to(cr, x1 + (x2-x1)/2.0, y1 + link_width, x2 - (x2-x1)/2.0, y2 + link_width, x2, y2 + link_width);
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
        
        // Shadow/glow
        cairo_set_source_rgba(cr, r, g, b, 0.2);
        rounded_rect(cr, margin_x - 12, y - 2, 14, left[i].dy + 4, 3);
        cairo_fill(cr);

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
        cairo_move_to(cr, margin_x - 20 - tw, y + (left[i].dy - th)/2.0);
        
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
        
        // Shadow/glow
        cairo_set_source_rgba(cr, r, g, b, 0.2);
        rounded_rect(cr, width - margin_x - 2, y - 2, 14, right[i].dy + 4, 3);
        cairo_fill(cr);

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
        cairo_move_to(cr, width - margin_x + 20, y + (right[i].dy - th)/2.0);
        
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }
    
    g_free(left);
    g_free(right);
    g_free(links);
}

static gboolean on_hm_scroll(GtkEventControllerScroll *scroll, double dx, double dy, gpointer user_data) {
    HeatmapState *hm = (HeatmapState *)user_data;
    if (dy > 0) hm->zoom *= 0.9;
    else if (dy < 0) hm->zoom *= 1.1;
    hm->zoom = fmax(0.1, fmin(hm->zoom, 5.0));
    
    GtkWidget *area = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(scroll));
    gtk_widget_set_size_request(area, -1, (hm->num_tags * 35 + 100) * hm->zoom);
    gtk_widget_queue_draw(area);
    return TRUE;
}

GtkWidget* create_heatmap_view(CualiAppState *state) {
    HeatmapState *hm = g_new0(HeatmapState, 1);
    load_heatmap_data(state, hm);
    g_hm_state = hm;
    
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_heatmap, hm, NULL);
    gtk_widget_set_size_request(area, -1, hm->num_tags * 35 + 100);
    g_hm_area = area;
    
    GtkEventController *scroll_ctrl = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_ctrl, "scroll", G_CALLBACK(on_hm_scroll), hm);
    gtk_widget_add_controller(area, scroll_ctrl);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), area);
    return scroll;
}

// --- Tag-Doc Matrix Logic ---
// Very similar to Heatmap, just different query

typedef struct {
    char **tag_names;
    char **doc_names;
    int *tag_ids;
    int *doc_ids;
    int num_tags;
    int num_docs;
    int **matrix;
} TagDocState;

static void load_tagdoc_data(CualiAppState *app_state, TagDocState *td) {
    td->num_tags = 0;
    td->num_docs = 0;
    
    // Count tags
    sqlite3_stmt *tstmt = db_tags_get_all(app_state->current_project_id);
    while(tstmt && sqlite3_step(tstmt) == SQLITE_ROW) td->num_tags++;
    if(tstmt) sqlite3_finalize(tstmt);
    
    // Count docs
    sqlite3_stmt *dstmt = db_documents_get_all(app_state->current_project_id);
    while(dstmt && sqlite3_step(dstmt) == SQLITE_ROW) td->num_docs++;
    if(dstmt) sqlite3_finalize(dstmt);
    
    td->tag_names = g_new0(char*, td->num_tags);
    td->tag_ids = g_new0(int, td->num_tags);
    td->doc_names = g_new0(char*, td->num_docs);
    td->doc_ids = g_new0(int, td->num_docs);
    
    td->matrix = g_new0(int*, td->num_tags);
    for(int i = 0; i < td->num_tags; i++) td->matrix[i] = g_new0(int, td->num_docs);
    
    tstmt = db_tags_get_all(app_state->current_project_id);
    int idx = 0;
    if (tstmt) {
        while(sqlite3_step(tstmt) == SQLITE_ROW) {
            td->tag_ids[idx] = sqlite3_column_int(tstmt, 0);
            td->tag_names[idx] = g_strdup((const char*)sqlite3_column_text(tstmt, 1));
            idx++;
        }
        sqlite3_finalize(tstmt);
    }
    
    dstmt = db_documents_get_all(app_state->current_project_id);
    idx = 0;
    if (dstmt) {
        while(sqlite3_step(dstmt) == SQLITE_ROW) {
            td->doc_ids[idx] = sqlite3_column_int(dstmt, 0);
            td->doc_names[idx] = g_strdup((const char*)sqlite3_column_text(dstmt, 1));
            idx++;
        }
        sqlite3_finalize(dstmt);
    }
    
    // Fill matrix
    sqlite3_stmt *mstmt = db_tags_get_matrix(app_state->current_project_id);
    if(mstmt) {
        while(sqlite3_step(mstmt) == SQLITE_ROW) {
            int doc_id = sqlite3_column_int(mstmt, 0);
            int tag_id = sqlite3_column_int(mstmt, 1);
            int freq = sqlite3_column_int(mstmt, 2);
            
            int t_idx = -1, d_idx = -1;
            for(int i = 0; i < td->num_tags; i++) if(td->tag_ids[i] == tag_id) t_idx = i;
            for(int i = 0; i < td->num_docs; i++) if(td->doc_ids[i] == doc_id) d_idx = i;
            
            if(t_idx >= 0 && d_idx >= 0) {
                td->matrix[t_idx][d_idx] = freq;
            }
        }
        sqlite3_finalize(mstmt);
    }
}

static void free_tagdoc_data(TagDocState *td) {
    for(int i = 0; i < td->num_tags; i++) {
        g_free(td->tag_names[i]);
        g_free(td->matrix[i]);
    }
    for(int i = 0; i < td->num_docs; i++) {
        g_free(td->doc_names[i]);
    }
    g_free(td->tag_names); g_free(td->tag_ids);
    g_free(td->doc_names); g_free(td->doc_ids);
    g_free(td->matrix);
    td->tag_names = NULL; td->tag_ids = NULL;
    td->doc_names = NULL; td->doc_ids = NULL;
    td->matrix = NULL;
    td->num_tags = 0; td->num_docs = 0;
}

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
                
                if (val > max_val * 0.5) cairo_set_source_rgb(cr, 1, 1, 1);
                else cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, fg_color.alpha);
                
                cairo_move_to(cr, rx + (cell_width - vw)/2.0, ry + (cell_height - vh)/2.0);
                pango_cairo_show_layout(cr, vl);
                g_object_unref(vl);
            }
        }
    }
}

GtkWidget* create_matrix_view(CualiAppState *state) {
    TagDocState *td = g_new0(TagDocState, 1);
    load_tagdoc_data(state, td);
    g_mat_state = td;
    
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_tagdoc, td, NULL);
    gtk_widget_set_size_request(area, 300 + td->num_docs*47, 300 + td->num_tags*32);
    g_mat_area = area;
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), area);
    return scroll;
}

// --- Word Cloud Logic ---

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
}

void refresh_visualizations(CualiAppState *state) {
    if (g_wb_state) {
        free_whiteboard_data((WhiteboardState *)g_wb_state);
        load_whiteboard_data(state, (WhiteboardState *)g_wb_state);
        if (g_wb_area) gtk_widget_queue_draw(g_wb_area);
    }
    
    if (g_hm_state) {
        free_heatmap_data((HeatmapState *)g_hm_state);
        load_heatmap_data(state, (HeatmapState *)g_hm_state);
        if (g_hm_area) {
            gtk_widget_set_size_request(g_hm_area, -1, ((HeatmapState *)g_hm_state)->num_tags*35 + 100);
            gtk_widget_queue_draw(g_hm_area);
        }
    }
    
    if (g_mat_state) {
        free_tagdoc_data((TagDocState *)g_mat_state);
        load_tagdoc_data(state, (TagDocState *)g_mat_state);
        if (g_mat_area) {
            gtk_widget_set_size_request(g_mat_area, 300 + ((TagDocState *)g_mat_state)->num_docs*47, 300 + ((TagDocState *)g_mat_state)->num_tags*32);
            gtk_widget_queue_draw(g_mat_area);
        }
    }
    
    if (g_wc_state) {
        free_wordcloud_data((WordCloudState *)g_wc_state);
        load_wordcloud_data(state, (WordCloudState *)g_wc_state);
        if (g_wc_area) gtk_widget_queue_draw(g_wc_area);
    }
}
