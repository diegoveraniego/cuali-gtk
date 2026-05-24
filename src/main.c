#include <adwaita.h>
#include "window.h"
#include "database.h"

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  gtk_icon_theme_add_search_path (theme, "./icons");
  gtk_icon_theme_add_resource_path (theme, "/org/cuali/icons");
  window_init(app);
}

static void
shutdown (GtkApplication *app,
          gpointer        user_data)
{
  db_close();
}

static void
open (GApplication  *app,
      GFile        **files,
      int            n_files,
      const char    *hint,
      gpointer       user_data)
{
  if (n_files > 0) {
    char *path = g_file_get_path (files[0]);
    window_init_with_file (GTK_APPLICATION (app), path);
    g_free (path);
  } else {
    window_init (GTK_APPLICATION (app));
  }
}

int
main (int    argc,
      char **argv)
{
  g_autoptr (AdwApplication) app = NULL;

  app = adw_application_new ("org.cuali.CualiGTK", G_APPLICATION_HANDLES_OPEN);

  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);
  g_signal_connect (app, "shutdown", G_CALLBACK (shutdown), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
