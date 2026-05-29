#include "visualizations.h"
#include "database.h"
#include <cairo.h>
#include <cairo-svg.h>
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
    int degree;
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
    double drag_start_pan_x;
    double drag_start_pan_y;
    GraphNode *hover_node;
    GraphEdge *hover_edge;
    double mouse_x;
    double mouse_y;
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

static double distance_to_segment(double px, double py, double ax, double ay, double bx, double by) {
    double ab_x = bx - ax;
    double ab_y = by - ay;
    double ap_x = px - ax;
    double ap_y = py - ay;
    
    double ab_len_sq = ab_x * ab_x + ab_y * ab_y;
    if (ab_len_sq < 1e-9) {
        return sqrt(ap_x * ap_x + ap_y * ap_y);
    }
    
    double t = (ap_x * ab_x + ap_y * ab_y) / ab_len_sq;
    t = fmax(0.0, fmin(1.0, t));
    
    double proj_x = ax + t * ab_x;
    double proj_y = ay + t * ab_y;
    
    double dx = px - proj_x;
    double dy = py - proj_y;
    return sqrt(dx * dx + dy * dy);
}

static GraphEdge* find_edge_at(WhiteboardState *state, double px, double py) {
    GraphEdge *best_edge = NULL;
    double min_dist = 5.0; // 5 pixels tolerance in logical space
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *edge = l->data;
        GraphNode *n1 = find_node(state, edge->source_id);
        GraphNode *n2 = find_node(state, edge->target_id);
        if (n1 && n2) {
            double dist = distance_to_segment(px, py, n1->x, n1->y, n2->x, n2->y);
            if (dist < min_dist) {
                min_dist = dist;
                best_edge = edge;
            }
        }
    }
    return best_edge;
}

static void draw_whiteboard(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    state->width = width;
    state->height = height;
    
    // Background is #1a1a1a
    cairo_set_source_rgb(cr, 0.102, 0.102, 0.102);
    cairo_paint(cr);
    
    cairo_save(cr);
    cairo_translate(cr, state->pan_x, state->pan_y);
    cairo_scale(cr, state->zoom, state->zoom);
    
    // Find max edge weight for normalization
    int max_weight = 1;
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *edge = l->data;
        if (edge->weight > max_weight) {
            max_weight = edge->weight;
        }
    }
    
    // Draw edges as straight lines with weight-based thickness and opacity
    for (GList *l = state->edges; l != NULL; l = l->next) {
        GraphEdge *edge = l->data;
        GraphNode *n1 = find_node(state, edge->source_id);
        GraphNode *n2 = find_node(state, edge->target_id);
        if (n1 && n2) {
            double ratio = (double)edge->weight / max_weight;
            double line_w = 1.0 + 4.0 * ratio;
            double opacity = 0.15 + 0.35 * ratio;
            
            if (state->hover_edge == edge) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
                cairo_set_line_width(cr, line_w + 1.0);
            } else {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, opacity);
                cairo_set_line_width(cr, line_w);
            }
            cairo_move_to(cr, n1->x, n1->y);
            cairo_line_to(cr, n2->x, n2->y);
            cairo_stroke(cr);
        }
    }
    
    // Draw Nodes as filled circles with white borders
    for (GList *l = state->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        
        GdkRGBA rgba;
        gdk_rgba_parse(&rgba, n->color ? n->color : "#7F77DD");
        cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
        cairo_arc(cr, n->x, n->y, n->radius, 0, 2.0 * G_PI);
        cairo_fill_preserve(cr);
        
        if (state->hover_node == n) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_set_line_width(cr, 2.5);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
            cairo_set_line_width(cr, 1.5);
        }
        cairo_stroke(cr);
        
        // Draw tag path label next to node in white, 11px, truncated at 20 characters
        char *truncated_name = NULL;
        if (n->name) {
            if (g_utf8_strlen(n->name, -1) > 20) {
                gchar *sub = g_utf8_substring(n->name, 0, 17);
                truncated_name = g_strconcat(sub, "...", NULL);
                g_free(sub);
            } else {
                truncated_name = g_strdup(n->name);
            }
        } else {
            truncated_name = g_strdup("");
        }
        
        PangoLayout *layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, truncated_name, -1);
        PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 11");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        
        int text_w, text_h;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, n->x + n->radius + 6.0, n->y - (text_h / 2.0));
        pango_cairo_show_layout(cr, layout);
        
        g_object_unref(layout);
        g_free(truncated_name);
    }
    
    cairo_restore(cr);
    
    // Draw tooltips in physical coordinates if not exporting to SVG
    if (cairo_surface_get_type(cairo_get_target(cr)) != CAIRO_SURFACE_TYPE_SVG) {
        if (state->hover_node || state->hover_edge) {
            char *text = NULL;
            if (state->hover_node) {
                text = g_strdup_printf("Tag: %s\nHighlights: %d", state->hover_node->name, state->hover_node->degree);
            } else if (state->hover_edge) {
                GraphNode *n1 = find_node(state, state->hover_edge->source_id);
                GraphNode *n2 = find_node(state, state->hover_edge->target_id);
                if (n1 && n2) {
                    text = g_strdup_printf("Connection:\n%s ↔ %s\nCo-occurrences: %d", n1->name, n2->name, state->hover_edge->weight);
                }
            }
            
            if (text) {
                PangoLayout *layout = pango_cairo_create_layout(cr);
                pango_layout_set_text(layout, text, -1);
                PangoFontDescription *desc = pango_font_description_from_string("Inter, Cantarell 11");
                pango_layout_set_font_description(layout, desc);
                pango_font_description_free(desc);
                
                int text_w, text_h;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                
                double padding_x = 10.0;
                double padding_y = 8.0;
                double box_w = text_w + padding_x * 2.0;
                double box_h = text_h + padding_y * 2.0;
                
                double tx = state->mouse_x + 15.0;
                double ty = state->mouse_y + 15.0;
                
                if (tx + box_w > width) {
                    tx = state->mouse_x - box_w - 5.0;
                }
                if (ty + box_h > height) {
                    ty = state->mouse_y - box_h - 5.0;
                }
                if (tx < 0.0) tx = 5.0;
                if (ty < 0.0) ty = 5.0;
                
                // Draw tooltip shadow
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
                rounded_rect(cr, tx + 2, ty + 2, box_w, box_h, 6.0);
                cairo_fill(cr);
                
                // Draw tooltip box background
                cairo_set_source_rgba(cr, 0.15, 0.15, 0.15, 0.95);
                rounded_rect(cr, tx, ty, box_w, box_h, 6.0);
                cairo_fill_preserve(cr);
                
                // Draw border
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.2);
                cairo_set_line_width(cr, 1.0);
                cairo_stroke(cr);
                
                // Draw tooltip text
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, tx + padding_x, ty + padding_y);
                pango_cairo_show_layout(cr, layout);
                
                g_object_unref(layout);
                g_free(text);
            }
        }
    }
}

static void on_wb_drag_begin(GtkGestureDrag *gesture, double x, double y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    double logical_x = (x - state->pan_x) / state->zoom;
    double logical_y = (y - state->pan_y) / state->zoom;
    state->drag_node = find_node_at(state, logical_x, logical_y);
    if (!state->drag_node) {
        state->drag_start_pan_x = state->pan_x;
        state->drag_start_pan_y = state->pan_y;
    }
}

static void on_wb_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    double start_x, start_y;
    gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y);
    
    if (state->drag_node) {
        state->drag_node->x = (start_x + offset_x - state->pan_x) / state->zoom;
        state->drag_node->y = (start_y + offset_y - state->pan_y) / state->zoom;
        state->drag_node->vx = 0.0;
        state->drag_node->vy = 0.0;
        
        // Pinned node won't move; let other nodes respond to it
        for (int i = 0; i < 5; i++) {
            double dx = state->drag_node->x;
            double dy = state->drag_node->y;
            apply_force_directed(state);
            state->drag_node->x = dx;
            state->drag_node->y = dy;
            state->drag_node->vx = 0.0;
            state->drag_node->vy = 0.0;
        }
    } else {
        state->pan_x = state->drag_start_pan_x + offset_x;
        state->pan_y = state->drag_start_pan_y + offset_y;
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

static void on_wb_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    state->mouse_x = x;
    state->mouse_y = y;
    
    double logical_x = (x - state->pan_x) / state->zoom;
    double logical_y = (y - state->pan_y) / state->zoom;
    
    GraphNode *new_hover_node = find_node_at(state, logical_x, logical_y);
    GraphEdge *new_hover_edge = NULL;
    
    if (!new_hover_node) {
        new_hover_edge = find_edge_at(state, logical_x, logical_y);
    }
    
    if (state->hover_node != new_hover_node || state->hover_edge != new_hover_edge) {
        state->hover_node = new_hover_node;
        state->hover_edge = new_hover_edge;
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller)));
    } else if (state->hover_node || state->hover_edge) {
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller)));
    }
}

static void on_wb_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    WhiteboardState *state = (WhiteboardState *)user_data;
    if (state->hover_node != NULL || state->hover_edge != NULL) {
        state->hover_node = NULL;
        state->hover_edge = NULL;
        gtk_widget_queue_draw(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller)));
    }
}

static void load_whiteboard_data(CualiAppState *app_state, WhiteboardState *wb) {
    wb->nodes = NULL;
    wb->edges = NULL;
    wb->zoom = 1.0;
    wb->pan_x = 0.0;
    wb->pan_y = 0.0;
    wb->drag_node = NULL;
    wb->hover_node = NULL;
    wb->hover_edge = NULL;
    wb->mouse_x = 0.0;
    wb->mouse_y = 0.0;
    
    // Stable random seed based on project_id
    srand(app_state->current_project_id);
    
    sqlite3_stmt *stmt = db_tags_get_stats(app_state->current_project_id);
    if (stmt) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            GraphNode *n = g_new0(GraphNode, 1);
            n->id = sqlite3_column_int(stmt, 0);
            n->name = g_strdup((const char *)sqlite3_column_text(stmt, 1));
            n->color = g_strdup((const char *)sqlite3_column_text(stmt, 2));
            n->degree = sqlite3_column_int(stmt, 3);
            
            // Random start coordinates within canvas boundaries (800x600 canvas)
            n->x = 100.0 + (rand() % 600);
            n->y = 100.0 + (rand() % 400);
            n->vx = 0.0;
            n->vy = 0.0;
            wb->nodes = g_list_append(wb->nodes, n);
        }
        sqlite3_finalize(stmt);
    }
    
    // Assign radius proportional to degree (min 6px, max 20px)
    int max_degree = 0;
    for (GList *l = wb->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        if (n->degree > max_degree) {
            max_degree = n->degree;
        }
    }
    for (GList *l = wb->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        if (max_degree > 0) {
            n->radius = 6.0 + 14.0 * ((double)n->degree / max_degree);
        } else {
            n->radius = 6.0;
        }
    }
    
    // Assign custom palette colors based on node index
    const char *palette[] = {"#7F77DD", "#1D9E75", "#D85A30", "#378ADD", "#BA7517", "#D4537E"};
    int palette_size = G_N_ELEMENTS(palette);
    int node_idx = 0;
    for (GList *l = wb->nodes; l != NULL; l = l->next) {
        GraphNode *n = l->data;
        g_free(n->color);
        n->color = g_strdup(palette[node_idx % palette_size]);
        node_idx++;
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
    
    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_wb_motion), wb);
    g_signal_connect(motion, "leave", G_CALLBACK(on_wb_leave), wb);
    gtk_widget_add_controller(area, motion);
    
    g_wb_area = area;
    g_wb_state = wb;
    
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

static void draw_heatmap(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
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
    gtk_widget_set_size_request(area, 200 + td->num_docs*35, 200 + td->num_tags*25);
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
    char **participant_names; // NULL-terminated list of participant names to exclude
    int num_participants;
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

/* Load/save participant names from a per-project file */
static char* wc_participants_path(CualiAppState *app_state) {
    return g_strdup_printf("%s/.cuali_participants_%d",
                          g_get_home_dir(), app_state->current_project_id);
}

static void wc_load_participants(CualiAppState *app_state, WordCloudState *wc) {
    if (wc->participant_names) {
        g_strfreev(wc->participant_names);
        wc->participant_names = NULL;
        wc->num_participants = 0;
    }
    char *path = wc_participants_path(app_state);
    char *content = NULL;
    if (g_file_get_contents(path, &content, NULL, NULL)) {
        wc->participant_names = g_strsplit(content, "\n", -1);
        // Count non-empty
        wc->num_participants = 0;
        for (int i = 0; wc->participant_names[i] != NULL; i++) {
            char *trimmed = g_strstrip(wc->participant_names[i]);
            if (trimmed[0] != '\0') wc->num_participants++;
        }
        g_free(content);
    }
    g_free(path);
}

static void wc_save_participants(CualiAppState *app_state, const char *text) {
    char *path = wc_participants_path(app_state);
    g_file_set_contents(path, text ? text : "", -1, NULL);
    g_free(path);
}

static gboolean wc_is_participant_name(WordCloudState *wc, const char *word) {
    if (!wc->participant_names || !word) return FALSE;
    char *lower_word = g_utf8_strdown(word, -1);
    for (int i = 0; wc->participant_names[i] != NULL; i++) {
        char *name = g_strstrip(wc->participant_names[i]);
        if (name[0] == '\0') continue;
        char *lower_name = g_utf8_strdown(name, -1);
        gboolean match = (g_strcmp0(lower_word, lower_name) == 0);
        g_free(lower_name);
        if (match) { g_free(lower_word); return TRUE; }
    }
    g_free(lower_word);
    return FALSE;
}

static GtkTextBuffer *g_part_buf = NULL;

static const char *spanish_stopwords_str = 
    "que, de, en, la, el, un, es, y, a, más, qué, como, también, así, pero, "
    "por, con, para, si, no, se, lo, al, del, las, los, una, este, esta, ese, "
    "esa, yo, tú, él, ella, nos, ellos, me, te, le, ya, muy, bien, pues, eh, "
    "mm, bueno, entonces, o, e, ni, hay, era, fue, ser, estar, tiene, han, "
    "había, sobre, su, sus, mi, mis, tu, tus, aquí, cuando, donde, porque, "
    "aunque, sino, todo, todos, toda, todas, esto, eso, aquello, sí, igual, "
    "creo, claro, sea, uno, son, ese, nos, tener, cómo, hacer, cosas, mucho, "
    "quizás, sé, está, veces, también, así, porque, ejemplo, igual, entonces, "
    "mismo, nada, poco, poder, alguna, después, ahora, ahí, algo, siempre, "
    "vamos, tenemos, tenía, manera, hecho, cada, entre, solo, otro, otra, "
    "muchas, caso, vez, forma, sentido, tema, realidad, haber, esas, algún, "
    "debería, importante, puede, tipo, les, da, va, po, mí, ríe, voy, risa, "
    "risas, profe, ha, he, son, ver";

static const char *english_stopwords_str = 
    "the, a, an, is, are, was, were, be, been, being, have, has, had, do, does, "
    "did, will, would, could, should, may, might, shall, can, of, in, on, at, "
    "by, for, with, about, to, from, up, out, and, but, or, not, no, i, you, "
    "he, she, it, we, they, me, him, her, us, them, my, your, his, its, our, "
    "their, this, that, these, those, what, which, who, when, where, how, all, "
    "each, more, most, also, just, than, too, very, well, here, there, if, as, "
    "so, just, like, know, think, yeah, okay, right, thing, things, something, "
    "really, actually, basically, literally";

static char* wc_stopwords_path(CualiAppState *app_state) {
    return g_strdup_printf("%s/.cuali_stopwords_%d",
                           g_get_home_dir(), app_state->current_project_id);
}

static void wc_load_stopwords_settings(CualiAppState *app_state, gboolean *enabled, char **language, char **extra) {
    *enabled = FALSE;
    *language = g_strdup("es");
    *extra = g_strdup("");

    char *path = wc_stopwords_path(app_state);
    char *content = NULL;
    if (g_file_get_contents(path, &content, NULL, NULL)) {
        char **lines = g_strsplit(content, "\n", -1);
        for (int i = 0; lines[i] != NULL; i++) {
            char *line = g_strstrip(lines[i]);
            if (g_str_has_prefix(line, "enabled=")) {
                *enabled = (strcmp(line + 8, "1") == 0);
            } else if (g_str_has_prefix(line, "language=")) {
                g_free(*language);
                *language = g_strdup(line + 9);
            } else if (g_str_has_prefix(line, "extra=")) {
                g_free(*extra);
                *extra = g_strdup(line + 6);
            }
        }
        g_strfreev(lines);
        g_free(content);
    }
    g_free(path);
}

static void wc_save_stopwords_settings(CualiAppState *app_state, gboolean enabled, const char *language, const char *extra) {
    char *path = wc_stopwords_path(app_state);
    char *content = g_strdup_printf("enabled=%d\nlanguage=%s\nextra=%s\n",
                                    enabled ? 1 : 0, language ? language : "es", extra ? extra : "");
    g_file_set_contents(path, content, -1, NULL);
    g_free(content);
    g_free(path);
}

static void wc_add_stopwords_to_set(GHashTable *set, const char *str) {
    char **words = g_strsplit_set(str, ", \t\n\r", -1);
    for (int i = 0; words[i] != NULL; i++) {
        char *trimmed = g_strstrip(words[i]);
        if (trimmed[0] != '\0') {
            char *lower = g_utf8_strdown(trimmed, -1);
            g_hash_table_replace(set, lower, GINT_TO_POINTER(1));
        }
    }
    g_strfreev(words);
}

static gboolean wc_is_pure_digit(const char *s) {
    if (!s || *s == '\0') return FALSE;
    while (*s) {
        if (*s < '0' || *s > '9') return FALSE;
        s++;
    }
    return TRUE;
}

static gboolean is_only_nbsp(const char *s) {
    if (!s || *s == '\0') return TRUE;
    const char *p = s;
    while (*p) {
        if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xA0) {
            p += 2;
        } else {
            return FALSE;
        }
    }
    return TRUE;
}

static char* replace_multibyte_punctuation(const char *str) {
    if (!str) return NULL;
    GString *res = g_string_new("");
    const char *p = str;
    while (*p) {
        gunichar c = g_utf8_get_char(p);
        if (c == 0x00BF || c == 0x00A1 || c == 0x2014 || c == 0x2013 || 
            c == 0x00AB || c == 0x00BB || c == 0x201C || c == 0x201D) {
            g_string_append_c(res, ' ');
        } else {
            g_string_append_unichar(res, c);
        }
        p = g_utf8_next_char(p);
    }
    return g_string_free(res, FALSE);
}

static void load_wordcloud_data(CualiAppState *app_state, WordCloudState *wc) {
    // Keep existing participant_names if already set, else load from file
    if (!wc->participant_names) {
        wc_load_participants(app_state, wc);
    }
    wc->words = NULL;
    wc->max_freq = 1;
    wc->zoom = wc->zoom > 0 ? wc->zoom : 1.0;

    gboolean stopwords_enabled = FALSE;
    char *stopwords_language = NULL;
    char *stopwords_extra = NULL;
    wc_load_stopwords_settings(app_state, &stopwords_enabled, &stopwords_language, &stopwords_extra);

    GHashTable *stopword_set = NULL;
    if (stopwords_enabled) {
        stopword_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        if (g_strcmp0(stopwords_language, "es") == 0 || g_strcmp0(stopwords_language, "both") == 0) {
            wc_add_stopwords_to_set(stopword_set, spanish_stopwords_str);
        }
        if (g_strcmp0(stopwords_language, "en") == 0 || g_strcmp0(stopwords_language, "both") == 0) {
            wc_add_stopwords_to_set(stopword_set, english_stopwords_str);
        }
        if (stopwords_extra && stopwords_extra[0] != '\0') {
            wc_add_stopwords_to_set(stopword_set, stopwords_extra);
        }
    }

    GRegex *timestamp_rx = g_regex_new("^\\[[\\d:]+\\]$", G_REGEX_DEFAULT, 0, NULL);
    GRegex *speaker_prefix_rx = g_regex_new("^([A-ZÁÉÍÓÚÑÜ][a-záéíóúñü]+)(\\s+\\d+)?\\s*:\\s*", G_REGEX_DEFAULT, 0, NULL);
    GRegex *annotation_rx = g_regex_new("\\(.*?\\)", G_REGEX_DEFAULT, 0, NULL);

    GHashTable *counts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    sqlite3_stmt *stmt = db_documents_get_all_contents(app_state->current_project_id);
    if(stmt) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *html = (const char*)sqlite3_column_text(stmt, 0);
            if(!html) continue;

            char *text = strip_html(html);

            char **lines = g_strsplit(text, "\n", -1);
            for (int li = 0; lines[li] != NULL; li++) {
                const char *line = lines[li];
                char *trimmed = g_strdup(line);
                g_strstrip(trimmed);

                // Strip parenthesized annotations (e.g. "(Risas)") from the line
                char *no_annotations = NULL;
                if (annotation_rx) {
                    no_annotations = g_regex_replace(annotation_rx, trimmed, -1, 0, "", 0, NULL);
                } else {
                    no_annotations = g_strdup(trimmed);
                }
                g_free(trimmed);
                g_strstrip(no_annotations);
                trimmed = no_annotations;

                /*
                 * Three-pass Google Pinpoint transcript filtering pipeline:
                 *
                 * Pass 1: Skip standalone timestamp lines (e.g. "[MM:SS]", "[HH:MM:SS]")
                 *         and separator lines consisting entirely of non-breaking spaces (U+00A0).
                 */
                if (trimmed[0] == '\0' || is_only_nbsp(trimmed)) {
                    g_free(trimmed);
                    continue;
                }

                if (timestamp_rx && g_regex_match(timestamp_rx, trimmed, 0, NULL)) {
                    g_free(trimmed);
                    continue;
                }

                /*
                 * Pass 2: Strip same-line speaker label prefixes (e.g. "Estudiante 1: ", "Entrevistador: ")
                 *         using a regex prefix match at the start of dialogue lines.
                 */
                const char *dialog = trimmed;
                GMatchInfo *match_info = NULL;
                if (speaker_prefix_rx && g_regex_match(speaker_prefix_rx, trimmed, 0, &match_info)) {
                    int start_pos = 0, end_pos = 0;
                    if (g_match_info_fetch_pos(match_info, 0, &start_pos, &end_pos)) {
                        dialog = trimmed + end_pos;
                    }
                    g_match_info_free(match_info);
                }

                // If dialogue is empty, or only has whitespace/NBSP, skip it
                char *dialog_trimmed = g_strdup(dialog);
                g_strstrip(dialog_trimmed);
                if (dialog_trimmed[0] == '\0' || is_only_nbsp(dialog_trimmed)) {
                    g_free(dialog_trimmed);
                    g_free(trimmed);
                    continue;
                }

                /*
                 * Pass 3: Post-tokenization lookahead filter. Look for pseudonym+number
                 *         combinations (e.g., "estudiante 1", "estudiante 12") matching
                 *         a participant name and a number sequence, and skip both.
                 */
                char *lower = g_utf8_strdown(dialog_trimmed, -1);
                char *clean_lower = replace_multibyte_punctuation(lower);
                char **words_raw = g_strsplit_set(clean_lower, " \n\t.,;:!?()\"'<>[]{}", -1);

                // Build clean list of non-empty tokens
                GPtrArray *clean_tokens = g_ptr_array_new_with_free_func(g_free);
                for (int i = 0; words_raw[i] != NULL; i++) {
                    char *w = g_strstrip(words_raw[i]);
                    if (w[0] != '\0') {
                        g_ptr_array_add(clean_tokens, g_strdup(w));
                    }
                }
                g_strfreev(words_raw);
                g_free(clean_lower);
                g_free(lower);

                // Traverse and filter
                int i = 0;
                int min_len = stopwords_enabled ? 3 : 2;
                while (i < clean_tokens->len) {
                    char *w = g_ptr_array_index(clean_tokens, i);
                    gboolean skip = FALSE;

                    if (i + 1 < clean_tokens->len) {
                        char *next = g_ptr_array_index(clean_tokens, i + 1);
                        if (wc_is_pure_digit(next) && wc_is_participant_name(wc, w)) {
                            i += 2; // skip both
                            skip = TRUE;
                        }
                    }

                    if (!skip) {
                        if (g_utf8_strlen(w, -1) >= min_len) {
                            gboolean is_stopword = FALSE;
                            if (stopword_set) {
                                is_stopword = (g_hash_table_lookup(stopword_set, w) != NULL);
                            }
                            if (!is_stopword) {
                                gpointer val = g_hash_table_lookup(counts, w);
                                int cnt = val ? GPOINTER_TO_INT(val) : 0;
                                g_hash_table_replace(counts, g_strdup(w), GINT_TO_POINTER(cnt + 1));
                            }
                        }
                        i++;
                    }
                }
                g_ptr_array_free(clean_tokens, TRUE);
                g_free(dialog_trimmed);
                g_free(trimmed);
            }
            g_strfreev(lines);
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
    if (timestamp_rx) g_regex_unref(timestamp_rx);
    if (speaker_prefix_rx) g_regex_unref(speaker_prefix_rx);
    if (annotation_rx) g_regex_unref(annotation_rx);
    if (stopword_set) g_hash_table_destroy(stopword_set);
    g_free(stopwords_language);
    g_free(stopwords_extra);
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

// --- Export Logic ---
static void on_export_viz_save_response(GObject *source, GAsyncResult *res, gpointer user_data) {
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), res, NULL);
    char *viz_type = (char *)user_data;
    
    if (!file) {
        g_free(viz_type);
        return;
    }

    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (!path) {
        g_free(viz_type);
        return;
    }

    if (g_strcmp0(viz_type, "whiteboard") == 0 && g_wb_state && g_wb_area) {
        int width = 1200;
        int height = 800;
        cairo_surface_t *surface = cairo_svg_surface_create(path, width, height);
        cairo_t *cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        draw_whiteboard(GTK_DRAWING_AREA(g_wb_area), cr, width, height, g_wb_state);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    } else if (g_strcmp0(viz_type, "heatmap") == 0 && g_hm_state && g_hm_area) {
        double current_zoom = ((HeatmapState *)g_hm_state)->zoom;
        int width = 1200 * current_zoom;
        int height = (((HeatmapState *)g_hm_state)->num_tags * 35 + 100) * current_zoom;
        cairo_surface_t *surface = cairo_svg_surface_create(path, width, height);
        cairo_t *cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        draw_heatmap(GTK_DRAWING_AREA(g_hm_area), cr, width, height, g_hm_state);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    } else if (g_strcmp0(viz_type, "matrix") == 0 && g_mat_state && g_mat_area) {
        int width = 200 + ((TagDocState *)g_mat_state)->num_docs * 35;
        int height = 200 + ((TagDocState *)g_mat_state)->num_tags * 25;
        cairo_surface_t *surface = cairo_svg_surface_create(path, width, height);
        cairo_t *cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        draw_tagdoc(GTK_DRAWING_AREA(g_mat_area), cr, width, height, g_mat_state);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    } else if (g_strcmp0(viz_type, "wordcloud") == 0 && g_wc_state && g_wc_area) {
        int width = 800;
        int height = 600;
        cairo_surface_t *surface = cairo_svg_surface_create(path, width, height);
        cairo_t *cr = cairo_create(surface);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        draw_wordcloud(GTK_DRAWING_AREA(g_wc_area), cr, width, height, g_wc_state);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }

    g_free(path);
    g_free(viz_type);
}

static void on_export_viz_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *viz_stack = GTK_WIDGET(user_data);
    const char *visible_child_name = adw_view_stack_get_visible_child_name(ADW_VIEW_STACK(viz_stack));

    GtkFileDialog *dlg = gtk_file_dialog_new();
    
    if (g_strcmp0(visible_child_name, "whiteboard") == 0) {
        gtk_file_dialog_set_title(dlg, "Exportar Redes a SVG");
        gtk_file_dialog_set_initial_name(dlg, "redes.svg");
    } else if (g_strcmp0(visible_child_name, "heatmap") == 0) {
        gtk_file_dialog_set_title(dlg, "Exportar Co-ocurrencia a SVG");
        gtk_file_dialog_set_initial_name(dlg, "co-ocurrencia.svg");
    } else if (g_strcmp0(visible_child_name, "matrix") == 0) {
        gtk_file_dialog_set_title(dlg, "Exportar Matriz a SVG");
        gtk_file_dialog_set_initial_name(dlg, "matriz.svg");
    } else if (g_strcmp0(visible_child_name, "wordcloud") == 0) {
        gtk_file_dialog_set_title(dlg, "Exportar Frecuencias a SVG");
        gtk_file_dialog_set_initial_name(dlg, "frecuencias.svg");
    } else {
        g_object_unref(dlg);
        return;
    }

    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(btn));
    gtk_file_dialog_save(dlg, GTK_WINDOW(root), NULL, on_export_viz_save_response, g_strdup(visible_child_name));
    g_object_unref(dlg);
}

// --- Main Visualizations View ---
static void on_filter_changed(GtkRange *range, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    refresh_visualizations(state);
}

typedef struct {
    CualiAppState *app_state;
    GtkTextBuffer *text_buf;
    GtkPopover *popover;
} WcParticipantsData;

static void on_wc_participants_apply(GtkButton *btn, gpointer user_data) {
    WcParticipantsData *d = (WcParticipantsData *)user_data;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(d->text_buf, &start, &end);
    char *text = gtk_text_buffer_get_text(d->text_buf, &start, &end, FALSE);
    
    // Save to disk
    wc_save_participants(d->app_state, text);
    g_free(text);
    
    // Reload participant list and refresh word cloud
    if (g_wc_state) {
        wc_load_participants(d->app_state, (WordCloudState *)g_wc_state);
        free_wordcloud_data((WordCloudState *)g_wc_state);
        load_wordcloud_data(d->app_state, (WordCloudState *)g_wc_state);
        if (g_wc_area) gtk_widget_queue_draw(g_wc_area);
    }
    
    if (d->popover) gtk_popover_popdown(d->popover);
}

typedef struct {
    CualiAppState *app_state;
    GtkSwitch *enable_switch;
    GtkDropDown *lang_dropdown;
    GtkTextBuffer *text_buf;
    AdwDialog *dialog;
} WcStopwordsData;

static void on_wc_stopwords_apply(GtkButton *btn, gpointer user_data) {
    WcStopwordsData *d = (WcStopwordsData *)user_data;
    gboolean enabled = gtk_switch_get_active(d->enable_switch);
    guint selected = gtk_drop_down_get_selected(d->lang_dropdown);
    const char *language = "es";
    if (selected == 1) language = "en";
    else if (selected == 2) language = "both";
    
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(d->text_buf, &start, &end);
    char *extra = gtk_text_buffer_get_text(d->text_buf, &start, &end, FALSE);
    
    GString *clean_extra = g_string_new("");
    char **lines = g_strsplit(extra, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        char *trimmed = g_strstrip(lines[i]);
        if (trimmed[0] != '\0') {
            if (clean_extra->len > 0) g_string_append_c(clean_extra, ',');
            g_string_append(clean_extra, trimmed);
        }
    }
    g_strfreev(lines);
    g_free(extra);
    
    wc_save_stopwords_settings(d->app_state, enabled, language, clean_extra->str);
    g_string_free(clean_extra, TRUE);
    
    if (g_wc_state) {
        free_wordcloud_data((WordCloudState *)g_wc_state);
        load_wordcloud_data(d->app_state, (WordCloudState *)g_wc_state);
        if (g_wc_area) gtk_widget_queue_draw(g_wc_area);
    }
    
    if (d->dialog) adw_dialog_close(ADW_DIALOG(d->dialog));
}

static void on_stopwords_dialog_closed(AdwDialog *dialog, gpointer user_data) {
    WcStopwordsData *sd = (WcStopwordsData *)user_data;
    g_free(sd);
}

static void show_stopwords_dialog(CualiAppState *state) {
    GtkWidget *dialog = GTK_WIDGET(adw_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), "Stopwords");
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 400);
    adw_dialog_set_content_height(ADW_DIALOG(dialog), 450);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header_bar = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_bar);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(content_box, 16);
    gtk_widget_set_margin_end(content_box, 16);
    gtk_widget_set_margin_top(content_box, 16);
    gtk_widget_set_margin_bottom(content_box, 16);

    // 1. Switch row (using AdwActionRow with GtkSwitch)
    GtkWidget *enable_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(enable_row), "Activar stopwords");
    GtkWidget *enable_switch = gtk_switch_new();
    gtk_widget_set_valign(enable_switch, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(enable_row), enable_switch);
    gtk_box_append(GTK_BOX(content_box), enable_row);

    // 2. Dropdown (using AdwActionRow with GtkDropDown)
    GtkWidget *lang_row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(lang_row), "Idioma base");
    const char *langs[] = {"Español", "English", "Ambos", NULL};
    GtkWidget *lang_dropdown = gtk_drop_down_new_from_strings(langs);
    gtk_widget_set_valign(lang_dropdown, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(ADW_ACTION_ROW(lang_row), lang_dropdown);
    gtk_box_append(GTK_BOX(content_box), lang_row);

    // 3. Labels
    GtkWidget *extra_title_lbl = gtk_label_new("Palabras adicionales");
    gtk_widget_set_halign(extra_title_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(extra_title_lbl, "heading");

    GtkWidget *extra_subtitle_lbl = gtk_label_new("Una por línea. Se suman a la lista base.");
    gtk_widget_set_halign(extra_subtitle_lbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(extra_subtitle_lbl, "dim-label");

    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(label_box), extra_title_lbl);
    gtk_box_append(GTK_BOX(label_box), extra_subtitle_lbl);
    gtk_box_append(GTK_BOX(content_box), label_box);

    // 4. Scrolled TextView
    GtkWidget *extra_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(extra_scroll), 120);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(extra_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *extra_tv = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(extra_tv), GTK_WRAP_WORD);
    gtk_widget_add_css_class(extra_tv, "card");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(extra_scroll), extra_tv);
    gtk_box_append(GTK_BOX(content_box), extra_scroll);

    GtkTextBuffer *extra_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(extra_tv));

    // Load current values
    gboolean st_enabled = FALSE;
    char *st_lang = NULL;
    char *st_extra = NULL;
    wc_load_stopwords_settings(state, &st_enabled, &st_lang, &st_extra);

    gtk_switch_set_active(GTK_SWITCH(enable_switch), st_enabled);
    int st_idx = 0;
    if (g_strcmp0(st_lang, "en") == 0) st_idx = 1;
    else if (g_strcmp0(st_lang, "both") == 0) st_idx = 2;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(lang_dropdown), st_idx);

    GString *display_extra = g_string_new("");
    if (st_extra && st_extra[0] != '\0') {
        char **words = g_strsplit(st_extra, ",", -1);
        for (int i = 0; words[i] != NULL; i++) {
            if (display_extra->len > 0) g_string_append_c(display_extra, '\n');
            g_string_append(display_extra, words[i]);
        }
        g_strfreev(words);
    }
    gtk_text_buffer_set_text(extra_buf, display_extra->str, -1);
    g_string_free(display_extra, TRUE);

    g_free(st_lang);
    g_free(st_extra);

    // 5. Button suggested suggested-action
    GtkWidget *stop_apply_btn = gtk_button_new_with_label("Aplicar");
    gtk_widget_add_css_class(stop_apply_btn, "suggested-action");
    gtk_box_append(GTK_BOX(content_box), stop_apply_btn);

    WcStopwordsData *sd = g_new0(WcStopwordsData, 1);
    sd->app_state = state;
    sd->enable_switch = GTK_SWITCH(enable_switch);
    sd->lang_dropdown = GTK_DROP_DOWN(lang_dropdown);
    sd->text_buf = extra_buf;
    sd->dialog = ADW_DIALOG(dialog);

    g_signal_connect(stop_apply_btn, "clicked", G_CALLBACK(on_wc_stopwords_apply), sd);
    g_signal_connect(dialog, "closed", G_CALLBACK(on_stopwords_dialog_closed), sd);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), content_box);
    adw_dialog_set_child(ADW_DIALOG(dialog), toolbar_view);
    adw_dialog_present(ADW_DIALOG(dialog), state->window);
}

static void on_stopwords_btn_clicked(GtkButton *btn, gpointer user_data) {
    CualiAppState *state = (CualiAppState *)user_data;
    show_stopwords_dialog(state);
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
    
    // Participants config button (for Frequencies tab)
    GtkWidget *participants_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(participants_btn), "system-users-symbolic");
    gtk_widget_set_tooltip_text(participants_btn, "Participantes a excluir de Frequencies");
    gtk_widget_add_css_class(participants_btn, "flat");
    
    GtkWidget *part_popover = gtk_popover_new();
    GtkWidget *part_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(part_box, 12);
    gtk_widget_set_margin_end(part_box, 12);
    gtk_widget_set_margin_top(part_box, 12);
    gtk_widget_set_margin_bottom(part_box, 12);
    gtk_widget_set_size_request(part_box, 260, -1);
    
    GtkWidget *part_title = gtk_label_new("Nombres de participantes");
    gtk_widget_add_css_class(part_title, "title-4");
    gtk_widget_set_halign(part_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(part_box), part_title);
    
    GtkWidget *part_subtitle = gtk_label_new("Uno por línea. Se excluyen como label al inicio de párrafo y como 'Nombre N' en cualquier parte del texto.");
    gtk_label_set_wrap(GTK_LABEL(part_subtitle), TRUE);
    gtk_widget_add_css_class(part_subtitle, "caption");
    gtk_widget_set_halign(part_subtitle, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(part_box), part_subtitle);
    
    GtkWidget *part_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(part_scroll, 260, 140);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(part_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *part_tv = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(part_tv), GTK_WRAP_WORD);
    gtk_widget_add_css_class(part_tv, "card");
    gtk_widget_set_margin_top(part_tv, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(part_scroll), part_tv);
    gtk_box_append(GTK_BOX(part_box), part_scroll);
    
    GtkTextBuffer *part_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(part_tv));
    g_part_buf = part_buf;
    // Pre-load existing participants
    {
        char *pp = wc_participants_path(state);
        char *existing = NULL;
        if (g_file_get_contents(pp, &existing, NULL, NULL)) {
            gtk_text_buffer_set_text(part_buf, existing, -1);
            g_free(existing);
        }
        g_free(pp);
    }
    
    GtkWidget *apply_btn = gtk_button_new_with_label("Aplicar");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    gtk_box_append(GTK_BOX(part_box), apply_btn);
    
    WcParticipantsData *pd = g_new0(WcParticipantsData, 1);
    pd->app_state = state;
    pd->text_buf = part_buf;
    pd->popover = GTK_POPOVER(part_popover);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(on_wc_participants_apply), pd);
    
    gtk_popover_set_child(GTK_POPOVER(part_popover), part_box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(participants_btn), part_popover);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), participants_btn);
    
    // Stopwords Config Button (opens AdwDialog)
    GtkWidget *stopwords_btn = gtk_button_new_from_icon_name("preferences-system-symbolic");
    gtk_widget_set_tooltip_text(stopwords_btn, "Stopwords");
    gtk_widget_add_css_class(stopwords_btn, "flat");
    g_signal_connect(stopwords_btn, "clicked", G_CALLBACK(on_stopwords_btn_clicked), state);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), stopwords_btn);
    
    GtkWidget *export_btn = gtk_button_new_from_icon_name("document-save-symbolic");
    gtk_widget_set_tooltip_text(export_btn, "Exportar visualización a SVG");
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export_viz_clicked), viz_stack);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), export_btn);
    
    // 1. Networks
    GtkWidget *wb_view = create_whiteboard_view(state);
    AdwViewStackPage *p1 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wb_view, "whiteboard", "Networks");
    adw_view_stack_page_set_icon_name(p1, "com.github.maoschanz.drawing-symbolic");
    
    // 2. Sankey (replaces Heatmap)
    GtkWidget *hm_view = create_heatmap_view(state);
    AdwViewStackPage *p2 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), hm_view, "heatmap", "Co-occurrence");
    adw_view_stack_page_set_icon_name(p2, "view-grid-symbolic");
    
    // 3. Matrix
    GtkWidget *mat_view = create_matrix_view(state);
    AdwViewStackPage *p3 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), mat_view, "matrix", "Code-Document Matrix");
    adw_view_stack_page_set_icon_name(p3, "view-list-symbolic");
    
    // 4. Wordcloud
    GtkWidget *wc_view = create_wordcloud_view(state);
    AdwViewStackPage *p4 = adw_view_stack_add_titled(ADW_VIEW_STACK(viz_stack), wc_view, "wordcloud", "Frequencies");
    adw_view_stack_page_set_icon_name(p4, "format-text-rich-symbolic");
    
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), viz_stack);
    
    return toolbar_view;
}

void refresh_visualizations(CualiAppState *state) {
    if (g_part_buf) {
        char *pp = wc_participants_path(state);
        char *existing = NULL;
        if (g_file_get_contents(pp, &existing, NULL, NULL)) {
            gtk_text_buffer_set_text(g_part_buf, existing, -1);
            g_free(existing);
        } else {
            gtk_text_buffer_set_text(g_part_buf, "", -1);
        }
        g_free(pp);
    }



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
            gtk_widget_set_size_request(g_mat_area, 200 + ((TagDocState *)g_mat_state)->num_docs*35, 200 + ((TagDocState *)g_mat_state)->num_tags*25);
            gtk_widget_queue_draw(g_mat_area);
        }
    }
    
    if (g_wc_state) {
        free_wordcloud_data((WordCloudState *)g_wc_state);
        load_wordcloud_data(state, (WordCloudState *)g_wc_state);
        if (g_wc_area) gtk_widget_queue_draw(g_wc_area);
    }
}
