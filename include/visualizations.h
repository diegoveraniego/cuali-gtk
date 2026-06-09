#ifndef VISUALIZATIONS_H
#define VISUALIZATIONS_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "window.h"

GtkWidget* create_visualizations_view(CualiAppState *state);
void refresh_visualizations(CualiAppState *state);

#endif
