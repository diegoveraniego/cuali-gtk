#include <adwaita.h>
#include "window.h"
#include "database.h"

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  window_init(app);
}

static void
shutdown (GtkApplication *app,
          gpointer        user_data)
{
  db_close();
}

int
main (int    argc,
      char **argv)
{
  g_autoptr (AdwApplication) app = NULL;

  app = adw_application_new ("org.cuali.CualiGTK", G_APPLICATION_DEFAULT_FLAGS);

  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "shutdown", G_CALLBACK (shutdown), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
