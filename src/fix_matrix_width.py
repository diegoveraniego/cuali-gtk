import re

with open("/home/diego/Proyectos/cuali-gtk/src/visualizations.c", "r") as f:
    content = f.read()

# 1. Add get_matrix_layout
layout_func = """static void get_matrix_layout(GtkWidget *widget, TagDocState *state, int given_width, double *label_w, double *col_w) {
    int w = given_width > 0 ? given_width : (widget ? gtk_widget_get_allocated_width(widget) : 0);
    double min_width = 220.0 + state->num_docs * 70.0;
    *label_w = 220.0;
    *col_w = 70.0;
    if (w > min_width && state->num_docs > 0) {
        *col_w = 70.0 + (w - min_width) / state->num_docs;
    }
}

static void draw_tagdoc(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {"""

content = content.replace("static void draw_tagdoc(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {", layout_func)

# 2. Inside draw_tagdoc
state_decl = """    TagDocState *state = (TagDocState *)user_data;
    
    double label_w, col_w;
    get_matrix_layout(GTK_WIDGET(area), state, width, &label_w, &col_w);"""
content = content.replace("    TagDocState *state = (TagDocState *)user_data;", state_decl)

content = content.replace("double col_start_x = 220.0 + col * 70.0;", "double col_start_x = label_w + col * col_w;")
content = content.replace("cairo_move_to(cr, col_start_x + (70.0 - aw)/2.0, 10.0);", "cairo_move_to(cr, col_start_x + (col_w - aw)/2.0, 10.0);")
content = content.replace("cairo_move_to(cr, col_start_x + (70.0 - fw)/2.0, 12.0 + ah);", "cairo_move_to(cr, col_start_x + (col_w - fw)/2.0, 12.0 + ah);")
content = content.replace("cairo_line_to(cr, 220.0 + state->num_docs * 70.0, 50.0);", "cairo_line_to(cr, label_w + state->num_docs * col_w, 50.0);")
content = content.replace("cairo_rectangle(cr, 0.0, vr->y, 220.0 + state->num_docs * 70.0, vr->height);", "cairo_rectangle(cr, 0.0, vr->y, label_w + state->num_docs * col_w, vr->height);")
content = content.replace("cairo_line_to(cr, 220.0 + state->num_docs * 70.0 - 10.0, vr->y);", "cairo_line_to(cr, label_w + state->num_docs * col_w - 10.0, vr->y);")
content = content.replace("double cell_x = 220.0 + col * 70.0 + (70.0 - 52.0) / 2.0;", "double cell_x = label_w + col * col_w + (col_w - 52.0) / 2.0;")

# 3. Inside on_mat_motion
motion_decl = """static void on_mat_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    TagDocState *state = (TagDocState *)user_data;
    
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    double label_w, col_w;
    get_matrix_layout(widget, state, 0, &label_w, &col_w);"""
content = content.replace("static void on_mat_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {\n    TagDocState *state = (TagDocState *)user_data;", motion_decl)
content = content.replace("if (state->mouse_x < 220.0) {", "if (state->mouse_x < label_w) {")

# 4. Inside on_mat_leave
# No changes needed in on_mat_leave (just clears hover)

# 5. Inside on_mat_click
click_decl = """static void on_mat_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    TagDocState *state = (TagDocState *)user_data;
    if (n_press != 1) return;
    
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    double label_w, col_w;
    get_matrix_layout(widget, state, 0, &label_w, &col_w);"""
content = content.replace("static void on_mat_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {\n    TagDocState *state = (TagDocState *)user_data;\n    if (n_press != 1) return;", click_decl)
content = content.replace("if (x >= 0.0 && x < 220.0) {", "if (x >= 0.0 && x < label_w) {")

# 6. Inside tooltip logic (which is inside on_mat_motion usually, wait, where is it?)
# The actual query in on_mat_motion:
content = content.replace("if (x >= 220.0) {", "if (x >= label_w) {")
content = content.replace("double col_x = x - 220.0;", "double col_x = x - label_w;")
content = content.replace("int c = (int)(col_x / 70.0);", "int c = (int)(col_x / col_w);")

# 7. Inside on_export_viz_save_response
# Wait, for export, we want the natural size, not stretched to screen.
# Let's check on_export_viz_save_response
export_decl = """        int width = 220 + td->num_docs * 70;"""
# Let's keep it fixed for export.

with open("/home/diego/Proyectos/cuali-gtk/src/visualizations.c", "w") as f:
    f.write(content)

print("Done")
