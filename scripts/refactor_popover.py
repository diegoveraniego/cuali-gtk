import sys

with open('src/window.c', 'r') as f:
    content = f.read()

# 1. Change highlight_popover creation in window_init_with_file
old_dialog_new = """    /* Create dialogs once */
    state->highlight_popover = GTK_WIDGET(adw_dialog_new());
    build_highlight_dialog(state);"""

new_popover_new = """    /* Create dialogs once */
    state->highlight_popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(state->highlight_popover), TRUE);
    gtk_popover_set_position(GTK_POPOVER(state->highlight_popover), GTK_POS_BOTTOM);
    gtk_widget_set_parent(state->highlight_popover, text_view);
    build_highlight_dialog(state);"""

if old_dialog_new in content:
    content = content.replace(old_dialog_new, new_popover_new)


# 2. Modify build_highlight_dialog
old_build_dialog = """static void build_highlight_dialog(CualiAppState *state)
{
    adw_dialog_set_title(ADW_DIALOG(state->highlight_popover), "Highlight");
    adw_dialog_set_content_width(ADW_DIALOG(state->highlight_popover), 360);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header_bar = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header_bar);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);"""

new_build_popover = """static void build_highlight_dialog(CualiAppState *state)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(box, 360, -1);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);"""

if old_build_dialog in content:
    content = content.replace(old_build_dialog, new_build_popover)


old_dialog_end = """    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), box);
    adw_dialog_set_child(ADW_DIALOG(state->highlight_popover), toolbar_view);
    g_signal_connect(state->highlight_popover, "closed",
                     G_CALLBACK(on_highlight_dialog_closed), state);
}"""

new_popover_end = """    gtk_popover_set_child(GTK_POPOVER(state->highlight_popover), box);
    g_signal_connect(state->highlight_popover, "closed",
                     G_CALLBACK(on_highlight_dialog_closed), state);
}"""

if old_dialog_end in content:
    content = content.replace(old_dialog_end, new_popover_end)

# 3. Modify show_highlight_dialog_at or open_highlight_dialog_at_cursor to popup
# We just replace adw_dialog_present with gtk_popover_popup
old_present = """    adw_dialog_present(ADW_DIALOG(state->highlight_popover), state->window);"""
new_popup = """    GdkRectangle rect;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(state->text_view), &start_iter, &rect);
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(state->text_view), GTK_TEXT_WINDOW_WIDGET, rect.x, rect.y, &rect.x, &rect.y);
    gtk_popover_set_pointing_to(GTK_POPOVER(state->highlight_popover), &rect);
    gtk_popover_popup(GTK_POPOVER(state->highlight_popover));"""

if old_present in content:
    content = content.replace(old_present, new_popup)
    
old_force_close = "adw_dialog_force_close(ADW_DIALOG(state->highlight_popover));"
new_popdown = "gtk_popover_popdown(GTK_POPOVER(state->highlight_popover));"
content = content.replace(old_force_close, new_popdown)

# Add AdwBanner
old_banner_spot = """    /* Content view with toolbar */
    GtkWidget *doc_content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_view), doc_content_vbox);

    GtkWidget *doc_toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);"""

new_banner_spot = """    /* Content view with toolbar */
    GtkWidget *doc_content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_view), doc_content_vbox);

    GtkWidget *banner = adw_banner_new("Selecciona texto para etiquetar un segmento");
    adw_banner_set_revealed(ADW_BANNER(banner), TRUE);
    gtk_box_append(GTK_BOX(doc_content_vbox), banner);

    GtkWidget *doc_toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);"""

if old_banner_spot in content:
    content = content.replace(old_banner_spot, new_banner_spot)

with open('src/window.c', 'w') as f:
    f.write(content)

print("Popover and banner done")
