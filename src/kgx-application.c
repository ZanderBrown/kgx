/* kgx-window.c
 *
 * Copyright 2019 Zander Brown
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:kgx-application
 * @title: KgxApplication
 * @short_description: Application
 *
 * The application, on the face of it nothing particularly interesting but
 * under the hood it contains a #GSource used to monitor the shells (and
 * there children) running in the open #KgxWindow s
 */

#include "kgx-config.h"

#include <glib/gi18n.h>
#include <vte/vte.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "rgba.h"

#include "kgx-application.h"
#include "kgx-window.h"
#include "kgx-pages.h"
#include "kgx-simple-tab.h"
#include "kgx-resources.h"
#include "kgx-watcher.h"

#define LOGO_COL_SIZE 28
#define LOGO_ROW_SIZE 14

G_DEFINE_TYPE (KgxApplication, kgx_application, ADW_TYPE_APPLICATION)

enum {
  PROP_0,
  PROP_THEME,
  PROP_FONT,
  PROP_FONT_SCALE,
  PROP_SCROLLBACK_LINES,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };


static void
kgx_application_set_theme (KgxApplication *self,
                           KgxTheme        theme)
{
  AdwStyleManager *style_manager;

  g_return_if_fail (KGX_IS_APPLICATION (self));

  self->theme = theme;

  style_manager = adw_style_manager_get_default ();

  switch (theme) {
    case KGX_THEME_AUTO:
      adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_PREFER_LIGHT);
      break;
    case KGX_THEME_DAY:
      adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
      break;
    case KGX_THEME_NIGHT:
    case KGX_THEME_HACKER:
    default:
      adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_DARK);
      break;
  }

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_THEME]);
}


static void
kgx_application_set_scale (KgxApplication *self,
                           gdouble         scale)
{
  GAction *action;

  g_return_if_fail (KGX_IS_APPLICATION (self));

  self->scale = CLAMP (scale, KGX_FONT_SCALE_MIN, KGX_FONT_SCALE_MAX);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-out");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               self->scale > KGX_FONT_SCALE_MIN);
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-normal");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               self->scale != KGX_FONT_SCALE_DEFAULT);
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-in");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               self->scale < KGX_FONT_SCALE_MAX);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FONT_SCALE]);
}


static void
kgx_application_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  KgxApplication *self = KGX_APPLICATION (object);

  switch (property_id) {
    case PROP_THEME:
      kgx_application_set_theme (self, g_value_get_enum (value));
      break;
    case PROP_FONT_SCALE:
      kgx_application_set_scale (self, g_value_get_double (value));
      break;
    case PROP_SCROLLBACK_LINES:
      self->scrollback_lines = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kgx_application_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  KgxApplication *self = KGX_APPLICATION (object);

  switch (property_id) {
    case PROP_THEME:
      g_value_set_enum (value, self->theme);
      break;
    case PROP_FONT:
      g_value_take_boxed (value, kgx_application_get_font (self));
      break;
    case PROP_FONT_SCALE:
      g_value_set_double (value, self->scale);
      break;
    case PROP_SCROLLBACK_LINES:
      g_value_set_int64 (value, self->scrollback_lines);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
kgx_application_finalize (GObject *object)
{
  KgxApplication *self = KGX_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->desktop_interface);

  g_clear_pointer (&self->pages, g_tree_unref);

  G_OBJECT_CLASS (kgx_application_parent_class)->finalize (object);
}


static void
kgx_application_activate (GApplication *app)
{
  GtkWindow *window;
  guint32 timestamp;

  timestamp = GDK_CURRENT_TIME;

  /* Get the current window or create one if necessary. */
  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL) {
    kgx_application_add_terminal (KGX_APPLICATION (app),
                                  NULL,
                                  timestamp,
                                  NULL,
                                  NULL,
                                  NULL);

    return;
  }

  gtk_window_present_with_time (window, timestamp);
}


static void
kgx_application_startup (GApplication *app)
{
  KgxApplication    *self = KGX_APPLICATION (app);
  g_autoptr (GAction) settings_action = NULL;

  const char *const new_window_accels[] = { "<shift><primary>n", NULL };
  const char *const new_tab_accels[] = { "<shift><primary>t", NULL };
  const char *const close_tab_accels[] = { "<shift><primary>w", NULL };
  const char *const copy_accels[] = { "<shift><primary>c", NULL };
  const char *const paste_accels[] = { "<shift><primary>v", NULL };
  const char *const find_accels[] = { "<shift><primary>f", NULL };
  const char *const zoom_in_accels[] = { "<primary>plus", NULL };
  const char *const zoom_out_accels[] = { "<primary>minus", NULL };
  const char *const zoom_normal_accels[] = { "<primary>0", NULL };

  g_set_prgname (g_application_get_application_id (app));

  g_resources_register (kgx_get_resource ());

  g_type_ensure (KGX_TYPE_TERMINAL);
  g_type_ensure (KGX_TYPE_PAGES);

  G_APPLICATION_CLASS (kgx_application_parent_class)->startup (app);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "win.new-window", new_window_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "win.new-tab", new_tab_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "win.close-tab", close_tab_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "term.copy", copy_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "term.paste", paste_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "win.find", find_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.zoom-in", zoom_in_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.zoom-out", zoom_out_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.zoom-normal", zoom_normal_accels);

  self->settings = g_settings_new (KGX_APPLICATION_ID);
  g_settings_bind (self->settings, "theme", app, "theme", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "font-scale", app, "font-scale", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "scrollback-lines", app, "scrollback-lines", G_SETTINGS_BIND_DEFAULT);

  settings_action = g_settings_create_action (self->settings, "theme");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (settings_action));
}


static void
kgx_application_open (GApplication  *app,
                      GFile        **files,
                      int            n_files,
                      const char    *hint)
{
  guint32 timestamp = GDK_CURRENT_TIME;

  for (int i = 0; i < n_files; i++) {
    kgx_application_add_terminal (KGX_APPLICATION (app),
                                  NULL,
                                  timestamp,
                                  files[i],
                                  NULL,
                                  NULL);
  }
}


static int
kgx_application_local_command_line (GApplication   *app,
                                    char         ***arguments,
                                    int            *exit_status)
{
  for (size_t i = 0; (*arguments)[i] != NULL; i++) {
    /* Don't edit argv[0], but also don't start the loop with i = 1,
     * because it's technically possible to run a program with argc == 0 */
    if (i == 0) {
      continue;
    }

    if (strcmp ((*arguments)[i], "-e") == 0) {
      /* For xterm-compatible handling of -e, we want to stop parsing
       * other command-line options at this point, so that
       *
       *     kgx -T "Directory listing" -e ls -al /
       *
       * passes "-al" to ls instead of trying to parse them as kgx
       * options. To do this, turn -e into the "--" pseudo-argument -
       * unless there is exactly one argument following it, in which
       * case leave it as -e, which the GOptionContext will treat
       * like --command. */
      if (!((*arguments)[i + 1] != NULL && (*arguments)[i + 2] == NULL)) {
        (*arguments)[i][1] = '-';
      }

      break;
    } else if (strcmp ((*arguments)[i], "--") == 0) {
      /* Don't continue to edit arguments after the -- separator,
       * so you can do: kgx -- some-command ... -e ... */
      break;
    }
  }

  return G_APPLICATION_CLASS (kgx_application_parent_class)->local_command_line (app, arguments, exit_status);
}


static int
kgx_application_command_line (GApplication            *app,
                              GApplicationCommandLine *cli)
{
  KgxApplication *self = KGX_APPLICATION (app);
  guint32 timestamp = GDK_CURRENT_TIME;
  GVariantDict *options = NULL;
  g_auto (GStrv) argv = NULL;
  const char *command = NULL;
  const char *working_dir = NULL;
  const char *title = NULL;
  const char *const *shell = NULL;
  const char *cwd = NULL;
  gint64 scrollback;
  gboolean tab;
  g_autoptr (GFile) path = NULL;

  options = g_application_command_line_get_options_dict (cli);
  cwd = g_application_command_line_get_cwd (cli);

  g_variant_dict_lookup (options, "working-directory", "^&ay", &working_dir);
  g_variant_dict_lookup (options, "title", "&s", &title);
  g_variant_dict_lookup (options, "command", "^&ay", &command);
  g_variant_dict_lookup (options, G_OPTION_REMAINING, "^aay", &argv);

  if (g_variant_dict_lookup (options, "set-shell", "^as", &shell) && shell) {
    g_settings_set_strv (self->settings, "shell", shell);

    return EXIT_SUCCESS;
  }

  if (g_variant_dict_lookup (options, "set-scrollback", "x", &scrollback)) {
    g_settings_set_int64 (self->settings, "scrollback-lines", scrollback);

    return EXIT_SUCCESS;
  }

  if (working_dir != NULL) {
    path = g_file_new_for_commandline_arg_and_cwd (working_dir, cwd);
  }

  if (path == NULL) {
    path = g_file_new_for_path (cwd);
  }

  if (command != NULL) {
    gboolean can_exec_directly;

    if (argv != NULL && argv[0] != NULL) {
      g_warning (_("Cannot use both --command and positional parameters"));
      return EXIT_FAILURE;
    }

    g_clear_pointer (&argv, g_strfreev);

    if (strchr (command, '/') != NULL) {
      can_exec_directly = g_file_test (command, G_FILE_TEST_IS_EXECUTABLE);
    } else {
      g_autofree char *program = g_find_program_in_path (command);

      can_exec_directly = (program != NULL);
    }

    if (can_exec_directly) {
      argv = g_new0 (char *, 2);
      argv[0] = g_strdup (command);
      argv[1] = NULL;
    } else {
      argv = g_new0 (char *, 4);
      argv[0] = g_strdup ("/bin/sh");
      argv[1] = g_strdup ("-c");
      argv[2] = g_strdup (command);
      argv[3] = NULL;
    }
  }

  if (g_variant_dict_lookup (options, "tab", "b", &tab) && tab) {
    kgx_application_add_terminal (self,
                                  KGX_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (self))),
                                  timestamp,
                                  path,
                                  argv,
                                  title);
  } else {
    kgx_application_add_terminal (self, NULL, timestamp, path, argv, title);
  }

  return EXIT_SUCCESS;
}


static void
print_center (char *msg, int ign, short width)
{
  int half_msg = 0;
  int half_screen = 0;

  half_msg = strlen (msg) / 2;
  half_screen = width / 2;

  g_print ("%*s\n",
           half_screen + half_msg,
           msg);
}

static void
print_logo (short width)
{
  g_autoptr (GFile) logo = NULL;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) logo_lines = NULL;
  g_autofree char *logo_text = NULL;
  int i = 0;
  int half_screen = width / 2;

  logo = g_file_new_for_uri ("resource:/" KGX_APPLICATION_PATH "logo.txt");

  g_file_load_contents (logo, NULL, &logo_text, NULL, NULL, &error);

  if (error) {
    g_error ("Wat? %s", error->message);
  }

  logo_lines = g_strsplit (logo_text, "\n", -1);

  while (logo_lines[i]) {
    g_print ("%*s%s\n",
             half_screen - (LOGO_COL_SIZE / 2),
             "",
             logo_lines[i]);

    i++;
  }
}

static int
kgx_application_handle_local_options (GApplication *app,
                                      GVariantDict *options)
{
  gboolean version = FALSE;
  gboolean about = FALSE;

  if (g_variant_dict_lookup (options, "version", "b", &version)) {
    if (version) {
      // Translators: The leading # is intentional, the initial %s is the
      // version of KGX itself, the latter format is the VTE version
      g_print (_("# KGX %s using VTE %u.%u.%u %s\n"),
               PACKAGE_VERSION,
               vte_get_major_version (),
               vte_get_minor_version (),
               vte_get_micro_version (),
               vte_get_features ());
      return EXIT_SUCCESS;
    }
  }

  if (g_variant_dict_lookup (options, "about", "b", &about)) {
    if (about) {
      g_autofree char *copyright = g_strdup_printf (_("© %s Zander Brown"),
                                                    "2019-2021");
      struct winsize w;
      int padding = 0;

      ioctl (STDOUT_FILENO, TIOCGWINSZ, &w);

      padding = ((w.ws_row -1) - (LOGO_ROW_SIZE + 5)) / 2;

      for (int i = 0; i < padding; i++) {
        g_print ("\n");
      }

      print_logo (w.ws_col);
      print_center (_("King’s Cross"), -1, w.ws_col);
      print_center (PACKAGE_VERSION, -1, w.ws_col);
      print_center (_("Terminal Emulator"), -1, w.ws_col);
      print_center (copyright, -1, w.ws_col);
      print_center (_("GPL 3.0 or later"), -1, w.ws_col);

      for (int i = 0; i < padding; i++) {
        g_print ("\n");
      }

      return EXIT_SUCCESS;
    }
  }

  return G_APPLICATION_CLASS (kgx_application_parent_class)->handle_local_options (app, options);
}


static void
kgx_application_class_init (KgxApplicationClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class    = G_APPLICATION_CLASS (klass);

  object_class->set_property = kgx_application_set_property;
  object_class->get_property = kgx_application_get_property;
  object_class->finalize = kgx_application_finalize;

  app_class->activate = kgx_application_activate;
  app_class->startup = kgx_application_startup;
  app_class->open = kgx_application_open;
  app_class->local_command_line = kgx_application_local_command_line;
  app_class->command_line = kgx_application_command_line;
  app_class->handle_local_options = kgx_application_handle_local_options;

  /**
   * KgxApplication:theme:
   *
   * The palette to use, one of the values of #KgxTheme
   *
   * Officially only "night" exists, "hacker" is just a little fun
   *
   * Bound to ‘theme’ GSetting so changes persist
   *
   * Stability: Private
   */
  pspecs[PROP_THEME] =
    g_param_spec_enum ("theme", "Theme", "Terminal theme",
                       KGX_TYPE_THEME, KGX_THEME_NIGHT,
                       G_PARAM_READWRITE);

  pspecs[PROP_FONT] =
    g_param_spec_boxed ("font", "Font", "Monospace font",
                         PANGO_TYPE_FONT_DESCRIPTION,
                         G_PARAM_READABLE);

  pspecs[PROP_FONT_SCALE] =
    g_param_spec_double ("font-scale", "Font scale", "Font scaling",
                         KGX_FONT_SCALE_MIN,
                         KGX_FONT_SCALE_MAX,
                         KGX_FONT_SCALE_DEFAULT,
                         G_PARAM_READWRITE);

  /**
   * KgxApplication:scrollback-lines:
   *
   * How many lines of scrollback #KgxTerminal should keep
   *
   * Bound to ‘scrollback-lines’ GSetting so changes persist
   *
   * Stability: Private
   */
  pspecs[PROP_SCROLLBACK_LINES] =
    g_param_spec_int64 ("scrollback-lines", "Scrollback Lines", "Size of the scrollback",
                        G_MININT64, G_MAXINT64, 512,
                        G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);
}


static void
font_changed (GSettings      *settings,
              const char     *key,
              KgxApplication *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FONT]);
}


static GOptionEntry entries[] = {
  {
    "version",
    0,
    0,
    G_OPTION_ARG_NONE,
    NULL,
    NULL,
    NULL
  },
  {
    "about",
    0,
    0,
    G_OPTION_ARG_NONE,
    NULL,
    NULL,
    NULL
  },
  {
    "tab",
    0,
    0,
    G_OPTION_ARG_NONE,
    NULL,
    NULL,
    NULL
  },
  {
    "command",
    'e',
    0,
    G_OPTION_ARG_FILENAME,
    NULL,
    N_("Execute the argument to this option inside the terminal"),
    N_("COMMAND")
  },
  {
    "working-directory",
    0,
    0,
    G_OPTION_ARG_FILENAME,
    NULL,
    N_("Set the working directory"),
    // Translators: Placeholder of for a given directory
    N_("DIRNAME")
  },
  {
    "wait",
    0,
    0,
    G_OPTION_ARG_NONE,
    NULL,
    N_("Wait until the child exits (TODO)"),
    NULL
  },
  {
    "title",
    'T',
    0,
    G_OPTION_ARG_STRING,
    NULL,
    N_("Set the initial window title"),
    N_("TITLE")
  },
  {
    "set-shell",
    0,
    0,
    G_OPTION_ARG_STRING_ARRAY,
    NULL,
    N_("ADVANCED: Set the shell to launch"),
    N_("SHELL")
  },
  {
    "set-scrollback",
    0,
    0,
    G_OPTION_ARG_INT64,
    NULL,
    N_("ADVANCED: Set the scrollback length"),
    N_("LINES")
  },
  {
    G_OPTION_REMAINING,
    0,
    0,
    G_OPTION_ARG_FILENAME_ARRAY,
    NULL,
    NULL,
    N_("[-e|-- COMMAND [ARGUMENT...]]")
  },
  { NULL }
};


static void
new_window_activated (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);
  guint32 timestamp = GDK_CURRENT_TIME;

  kgx_application_add_terminal (self, NULL, timestamp, NULL, NULL, NULL);
}


static void
new_tab_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);
  guint32 timestamp = GDK_CURRENT_TIME;
  g_autoptr (GFile) dir = NULL;
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (window) {
    dir = kgx_window_get_working_dir (KGX_WINDOW (window));

    kgx_application_add_terminal (self, KGX_WINDOW (window), timestamp, dir, NULL, NULL);
  } else {
    kgx_application_add_terminal (self, NULL, timestamp, NULL, NULL, NULL);
  }
}


static void
focus_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);
  GtkRoot *root;
  KgxPages *pages;
  KgxTab *page;

  page = kgx_application_lookup_page (self, g_variant_get_uint32 (parameter));
  pages = kgx_tab_get_pages (page);
  kgx_pages_focus_page (pages, page);
  root = gtk_widget_get_root (GTK_WIDGET (pages));

  gtk_window_present_with_time (GTK_WINDOW (root), GDK_CURRENT_TIME);
}


static void
zoom_out_activated (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);

  if (self->scale < 0.1) {
    return;
  }

  kgx_application_set_scale (self, self->scale - 0.1);
}


static void
zoom_normal_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);

  kgx_application_set_scale (self, KGX_FONT_SCALE_DEFAULT);
}


static void
zoom_in_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  KgxApplication *self = KGX_APPLICATION (data);

  kgx_application_set_scale (self, self->scale + 0.1);
}


static GActionEntry app_entries[] = {
  { "new-window", new_window_activated, NULL, NULL, NULL },
  { "new-tab", new_tab_activated, NULL, NULL, NULL },
  { "focus-page", focus_activated, "u", NULL, NULL },
  { "zoom-out", zoom_out_activated, NULL, NULL, NULL },
  { "zoom-normal", zoom_normal_activated, NULL, NULL, NULL },
  { "zoom-in", zoom_in_activated, NULL, NULL, NULL },
};


static void
kgx_application_init (KgxApplication *self)
{
  g_application_add_main_option_entries (G_APPLICATION (self), entries);

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   app_entries,
                                   G_N_ELEMENTS (app_entries),
                                   self);

  self->desktop_interface = g_settings_new (DESKTOP_INTERFACE_SETTINGS_SCHEMA);

  g_signal_connect (self->desktop_interface,
                    "changed::" MONOSPACE_FONT_KEY_NAME,
                    G_CALLBACK (font_changed),
                    self);

  self->pages = g_tree_new_full (kgx_pid_cmp, NULL, NULL, NULL);
}


/**
 * kgx_application_get_font:
 * @self: the #KgxApplication
 *
 * Creates a #PangoFontDescription for the system monospace font.
 *
 * Returns: (transfer full): a new #PangoFontDescription
 */
PangoFontDescription *
kgx_application_get_font (KgxApplication *self)
{
  // Taken from gnome-terminal
  g_autofree char *font = NULL;

  g_return_val_if_fail (KGX_IS_APPLICATION (self), NULL);

  font = g_settings_get_string (self->desktop_interface,
                                MONOSPACE_FONT_KEY_NAME);

  return pango_font_description_from_string (font);
}


static void
page_died (gpointer data, GObject *dead_object)
{
  KgxApplication *self = KGX_APPLICATION (g_application_get_default ());

  g_tree_remove (self->pages, data);
}


/**
 * kgx_application_add_page:
 * @self: the instance to look for @id in
 * @page: the page to add
 *
 * Register a new #KgxTab with @self
 */
void
kgx_application_add_page (KgxApplication *self,
                          KgxTab         *page)
{
  guint id = 0;

  g_return_if_fail (KGX_IS_APPLICATION (self));
  g_return_if_fail (KGX_IS_TAB (page));

  id = kgx_tab_get_id (page);

  g_tree_insert (self->pages, GINT_TO_POINTER (id), page);
  g_object_weak_ref (G_OBJECT (page), page_died, GINT_TO_POINTER (id));
}


/**
 * kgx_application_lookup_page:
 * @self: the instance to look for @id in
 * @id: the page id to look for
 *
 * Try and find a #KgxTab with @id in @self
 *
 * Returns: (transfer none) (nullable): the found #KgxTab or %NULL
 */
KgxTab *
kgx_application_lookup_page (KgxApplication *self,
                             guint           id)
{
  g_return_val_if_fail (KGX_IS_APPLICATION (self), NULL);

  return g_tree_lookup (self->pages, GUINT_TO_POINTER (id));
}


static void
started (GObject      *src,
         GAsyncResult *res,
         gpointer      app)
{
  g_autoptr (GError) error = NULL;
  KgxTab *page = KGX_TAB (src);
  GPid pid;

  pid = kgx_tab_start_finish (page, res, &error);

  if (error) {
    g_warning ("Failed to start %s: %s",
               G_OBJECT_TYPE_NAME (src),
               error->message);

    return;
  }

  kgx_watcher_add (kgx_watcher_get_default (), pid, page);
}


KgxTab *
kgx_application_add_terminal (KgxApplication *self,
                              KgxWindow      *existing_window,
                              guint32         timestamp,
                              GFile          *working_directory,
                              GStrv           argv,
                              const char     *title)
{
  g_autofree char *user_shell = vte_get_user_shell ();
  g_autofree char *directory = NULL;
  g_auto (GStrv) shell = NULL;
  g_auto (GStrv) custom_shell = NULL;
  GtkWindow *window;
  GtkWidget *tab;
  KgxPages *pages;

  if (argv == NULL) {
    custom_shell = g_settings_get_strv (self->settings, "shell");

    if (g_strv_length (custom_shell) > 0) {
      shell = g_steal_pointer (&custom_shell);
    } else if (user_shell != NULL) {
      shell = g_new0 (char *, 2);
      shell[0] = g_steal_pointer (&user_shell);
      shell[1] = NULL;
    }
  }

  /* We should probably do something other than /bin/sh  */
  if (argv == NULL && shell == NULL) {
    shell = g_new0 (char *, 2);
    shell[0] = g_strdup ("/bin/sh");
    shell[1] = NULL;
    g_warning ("Defaulting to “%s”", shell[0]);
  }

  if (working_directory) {
    directory = g_file_get_path (working_directory);
  } else {
    directory = g_strdup (g_get_home_dir ());
  }

  tab = g_object_new (KGX_TYPE_SIMPLE_TAB,
                      "application", self,
                      "initial-work-dir", directory,
                      "command", shell != NULL ? shell : argv,
                      "tab-title", title,
                      "close-on-quit", argv == NULL,
                      NULL);
  kgx_tab_start (KGX_TAB (tab), started, self);

  if (existing_window) {
    window = GTK_WINDOW (existing_window);
  } else {
    window = g_object_new (KGX_TYPE_WINDOW,
                           "application", self,
                           NULL);
  }

  pages = kgx_window_get_pages (KGX_WINDOW (window));

  kgx_pages_add_page (pages, KGX_TAB (tab));
  kgx_pages_focus_page (pages, KGX_TAB (tab));

  gtk_window_present_with_time (window, timestamp);

  return KGX_TAB (tab);
}
