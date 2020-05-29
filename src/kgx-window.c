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
 * SECTION:kgx-window
 * @title: KgxWindow
 * @short_description: Window
 * 
 * The main #GtkApplicationWindow that acts as the terminal
 * 
 * Since: 0.1.0
 */

#define G_LOG_DOMAIN "Kgx"

#include <glib/gi18n.h>
#include <vte/vte.h>
#include <math.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "rgba.h"
#include "fp-vte-util.h"

#include "kgx-config.h"
#include "kgx-window.h"
#include "kgx-application.h"
#include "kgx-process.h"
#include "kgx-close-dialog.h"
#include "kgx-pages.h"
#include "kgx-local-page.h"

G_DEFINE_TYPE (KgxWindow, kgx_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_APPLICATION,
  PROP_INITIAL_WORK_DIR,
  PROP_COMMAND,
  PROP_CLOSE_ON_ZERO,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

/*
ON PID CLOSE:
  #if HAS_GTOP
  app = gtk_window_get_application (GTK_WINDOW (self));

  // If the application is in closing the app may already be null
  if (app) {
    kgx_application_remove_watch (KGX_APPLICATION (app), pid);
  }
  #endif
*/


static void
started (GObject      *src,
         GAsyncResult *res,
         gpointer      win)
{
  g_autoptr (GError) error = NULL;
  KgxPage *page = KGX_PAGE (src);
  GtkApplication *app = NULL;
  GPid pid;

  pid = kgx_page_start_finish (page, res, &error);

  if (error) {
    g_critical ("Failed to start %s: %s",
                G_OBJECT_TYPE_NAME (src),
                error->message);
  }

  app = gtk_window_get_application (GTK_WINDOW (win));

  kgx_application_add_watch (KGX_APPLICATION (app), pid, page);
}


static void
update_zoom (KgxWindow      *self,
             KgxApplication *app)
{
  g_autofree char *label = NULL;
  gdouble zoom;

  g_object_get (app,
                "font-scale", &zoom,
                NULL);

  label = g_strdup_printf ("%i%%",
                           (int) round (zoom * 100));
  gtk_label_set_label (GTK_LABEL (self->zoom_level), label);
}


static void
zoomed (GObject *object, GParamSpec *pspec, gpointer data)
{
  KgxWindow *self = KGX_WINDOW (data);

  update_zoom (self, KGX_APPLICATION (object));
}


static void
kgx_window_constructed (GObject *object)
{
  KgxWindow          *self = KGX_WINDOW (object);
  const char         *initial = NULL;
  g_autoptr (GError)  error = NULL;
  g_auto (GStrv)      shell = NULL;
  g_autofree char    *command = NULL;
  GtkWidget          *page;
  GtkApplication     *application = NULL;

  G_OBJECT_CLASS (kgx_window_parent_class)->constructed (object);

  if (G_UNLIKELY (self->command != NULL)) {
    // dup the string so we can free command later to handle the
    // (more likely) fp_vte_guess_shell case
    command = g_strdup (self->command);
  } else {
    command = fp_vte_guess_shell (NULL, &error);
    if (error) {
      g_warning ("flatterm: %s", error->message);
    }
  }

  if (command == NULL) {
    command = g_strdup ("/bin/sh");
    g_warning ("Defaulting to %s", shell[0]);
  }

  g_shell_parse_argv (command, NULL, &shell, &error);
  if (error) {
    g_warning ("Can't handle %s: %s", command, error->message);
  }

  if (self->working_dir) {
    initial = self->working_dir;
  } else {
    initial = g_get_home_dir ();
  }

  g_debug ("Working in %s", initial);

  application = gtk_window_get_application (GTK_WINDOW (self));

  page = g_object_new (KGX_TYPE_LOCAL_PAGE,
                       "application", application,
                       "visible", TRUE,
                       "initial-work-dir", initial,
                       "command", shell,
                       "close-on-quit", self->close_on_zero,
                       NULL);
  kgx_page_start (KGX_PAGE (page), started, self);

  kgx_pages_add_page (KGX_PAGES (self->pages), KGX_PAGE (page));

  g_object_bind_property (application,
                          "theme",
                          self->pages,
                          "theme",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (application,
                          "font",
                          self->pages,
                          "font",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (application,
                          "font-scale",
                          self->pages,
                          "zoom",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (application,
                    "notify::font-scale",
                    G_CALLBACK (zoomed),
                    self);

  update_zoom (self, KGX_APPLICATION (application));
}


static void
kgx_window_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  KgxWindow *self = KGX_WINDOW (object);

  switch (property_id) {
    case PROP_APPLICATION:
      gtk_window_set_application (GTK_WINDOW (self),
                                  g_value_get_object (value));
      break;
    case PROP_INITIAL_WORK_DIR:
      self->working_dir = g_value_dup_string (value);
      break;
    case PROP_COMMAND:
      self->command = g_value_dup_string (value);
      break;
    case PROP_CLOSE_ON_ZERO:
      self->close_on_zero = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kgx_window_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  KgxWindow *self = KGX_WINDOW (object);

  switch (property_id) {
    case PROP_APPLICATION:
      g_value_set_object (value,
                          gtk_window_get_application (GTK_WINDOW (self)));
      break;
    case PROP_INITIAL_WORK_DIR:
      g_value_set_string (value, self->working_dir);
      break;
    case PROP_COMMAND:
      g_value_set_string (value, self->command);
      break;
    case PROP_CLOSE_ON_ZERO:
      g_value_set_boolean (value, self->close_on_zero);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
kgx_window_finalize (GObject *object)
{
  KgxWindow *self = KGX_WINDOW (object);

  g_clear_pointer (&self->working_dir, g_free);
  g_clear_pointer (&self->command, g_free);

  G_OBJECT_CLASS (kgx_window_parent_class)->finalize (object);
}


static void
delete_response (GtkWidget *dlg,
                 int        response,
                 KgxWindow *self)
{
  gtk_widget_destroy (dlg);

  if (response == GTK_RESPONSE_OK) {
    self->close_anyway = TRUE;

    gtk_widget_destroy (GTK_WIDGET (self));
  }
}

static gboolean
kgx_window_delete_event (GtkWidget   *widget,
                         GdkEventAny *event)
{
  KgxWindow *self = KGX_WINDOW (widget);
  GtkWidget *dlg;
  g_autoptr (GPtrArray) children = NULL;

  children = kgx_pages_get_children (KGX_PAGES (self->pages));

  if (children->len < 1 || self->close_anyway) {
    return FALSE; // Aka no, I don't want to block closing
  }

  dlg = g_object_new (KGX_TYPE_CLOSE_DIALOG,
                      "transient-for", self,
                      "use-header-bar", TRUE,
                      NULL);

  g_signal_connect (dlg, "response", G_CALLBACK (delete_response), self);

  for (int i = 0; i < children->len; i++) {
    KgxProcess *process = g_ptr_array_index (children, i);
    
    kgx_close_dialog_add_command (KGX_CLOSE_DIALOG (dlg),
                                  kgx_process_get_exec (process));
  }

  gtk_widget_show (dlg);
  
  return TRUE; // Block the close
}


static void
active_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkApplication *app;

  app = gtk_window_get_application (GTK_WINDOW (object));

  if (gtk_window_is_active (GTK_WINDOW (object))) {
    kgx_application_push_active (KGX_APPLICATION (app));
  } else {
    kgx_application_pop_active (KGX_APPLICATION (app));
  }
}


static void
search_enabled (GObject    *object,
                GParamSpec *pspec,
                KgxWindow  *self)
{
  if (!hdy_search_bar_get_search_mode (HDY_SEARCH_BAR (self->search_bar))) {
    kgx_pages_focus_terminal (KGX_PAGES (self->pages));
  }
}


static void
search_changed (HdySearchBar *bar,
                KgxWindow    *self)
{
  const char *search = NULL;

  search = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  kgx_pages_search (KGX_PAGES (self->pages), search);
}


static void
search_next (HdySearchBar *bar,
             KgxWindow    *self)
{
  kgx_pages_search_forward (KGX_PAGES (self->pages));
}


static void
search_prev (HdySearchBar *bar,
             KgxWindow    *self)
{
  kgx_pages_search_back (KGX_PAGES (self->pages));
}


static void
zoom (KgxPages  *pages,
      KgxZoom    dir,
      KgxWindow *self)
{
  GAction *action = NULL;

  switch (dir) {
    case KGX_ZOOM_IN:
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-in");
      break;
    case KGX_ZOOM_OUT:
      action = g_action_map_lookup_action (G_ACTION_MAP (self), "zoom-out");
      break;
    default:
      g_return_if_reached ();
  }
  g_action_activate (action, NULL);
}


static void
status_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  KgxWindow *self = KGX_WINDOW (object);
  GtkStyleContext *context;
  KgxStatus status;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  status = kgx_pages_current_status (KGX_PAGES (self->pages));

  if (status & KGX_REMOTE) {
    gtk_style_context_add_class (context, KGX_WINDOW_STYLE_REMOTE);
  } else {
    gtk_style_context_remove_class (context, KGX_WINDOW_STYLE_REMOTE);
  }

  if (status & KGX_PRIVILEGED) {
    gtk_style_context_add_class (context, KGX_WINDOW_STYLE_ROOT);
  } else {
    gtk_style_context_remove_class (context, KGX_WINDOW_STYLE_ROOT);
  }
}


static void
kgx_window_class_init (KgxWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = kgx_window_constructed;
  object_class->set_property = kgx_window_set_property;
  object_class->get_property = kgx_window_get_property;
  object_class->finalize = kgx_window_finalize;

  widget_class->delete_event = kgx_window_delete_event;

  /**
   * KgxWindow:application:
   * 
   * Proxy for #GtkWindow #GtkWindow:application but with %G_PARAM_CONSTRUCT,
   * simple as that
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_APPLICATION] =
    g_param_spec_object ("application", "Application",
                         "The application the window is part of",
                         KGX_TYPE_APPLICATION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * KgxWindow:initial-work-dir:
   * 
   * Used to handle --working-dir
   * 
   * Since: 0.1.0
   */
  pspecs[PROP_INITIAL_WORK_DIR] =
    g_param_spec_string ("initial-work-dir", "Initial directory",
                         "Initial working directory",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * KgxWindow:command:
   * 
   * Used to handle -e
   * 
   * Since: 0.1.0
   */
  pspecs[PROP_COMMAND] =
    g_param_spec_string ("command", "Command",
                         "Command to run",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * KgxWindow:close-on-zero:
   * 
   * Should the window autoclose when the terminal complete
   * 
   * Since: 0.1.0
   */
  pspecs[PROP_CLOSE_ON_ZERO] =
    g_param_spec_boolean ("close-on-zero", "Close on zero",
                          "Should close when child exits with 0",
                          TRUE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               RES_PATH "kgx-window.ui");

  gtk_widget_class_bind_template_child (widget_class, KgxWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, exit_info);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, exit_message);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, zoom_level);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, about_item);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, pages);

  gtk_widget_class_bind_template_callback (widget_class, active_changed);

  gtk_widget_class_bind_template_callback (widget_class, search_enabled);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_next);
  gtk_widget_class_bind_template_callback (widget_class, search_prev);

  gtk_widget_class_bind_template_callback (widget_class, zoom);
  gtk_widget_class_bind_template_callback (widget_class, status_changed);
}


static void
new_activated (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  GtkWindow       *window = NULL;
  GtkApplication  *app = NULL;
  guint32          timestamp;
  g_autofree char *dir = NULL;

  /* Slightly "wrong" but hopefully by taking the time before
   * we spend non-zero time initing the window it's far enough in the
   * past for shell to do-the-right-thing
   */
  timestamp = GDK_CURRENT_TIME;

  app = gtk_window_get_application (GTK_WINDOW (data));

  dir = kgx_window_get_working_dir (KGX_WINDOW (data));

  window = g_object_new (KGX_TYPE_WINDOW,
                        "application", app,
                        "initial-work-dir", dir,
                        "close-on-zero", TRUE,
                        NULL);

  gtk_window_present_with_time (window, timestamp);
}


static void
new_tab_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  const char         *initial = NULL;
  g_autoptr (GError)  error = NULL;
  g_auto (GStrv)      shell = NULL;
  g_autofree char    *command = NULL;
  GtkWidget          *page;
  KgxWindow          *self = data;
  GtkApplication     *application = NULL;

  command = fp_vte_guess_shell (NULL, &error);
  if (error) {
    g_warning ("flatterm: %s", error->message);
  }

  if (command == NULL) {
    command = g_strdup ("/bin/sh");
    g_warning ("Defaulting to %s", shell[0]);
  }

  g_shell_parse_argv (command, NULL, &shell, &error);
  if (error) {
    g_warning ("Can't handle %s: %s", command, error->message);
  }

  initial = g_get_home_dir ();

  g_debug ("Working in %s", initial);

  application = gtk_window_get_application (GTK_WINDOW (self));

  page = g_object_new (KGX_TYPE_LOCAL_PAGE,
                       "application", application,
                       "visible", TRUE,
                       "initial-work-dir", initial,
                       "command", shell,
                       "close-on-quit", TRUE,
                       NULL);
  kgx_page_start (KGX_PAGE (page), started, self);

  kgx_pages_add_page (KGX_PAGES (self->pages), KGX_PAGE (page));
  kgx_pages_focus_page (KGX_PAGES (self->pages), KGX_PAGE (page));
}


static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  const char *authors[] = { "Zander Brown <zbrown@gnome.org>", NULL };
  const char *artists[] = { "Tobias Bernard", NULL };
  g_autofree char *copyright = NULL;
  
  copyright = g_strdup_printf (_("Copyright © %s Zander Brown"),
                               "2019-2020");

  gtk_show_about_dialog (GTK_WINDOW (data),
                         "authors", authors,
                         "artists", artists,
                         // Translators: Credit yourself here
                         "translator-credits", _("translator-credits"),
                         "comments", _("Terminal Emulator"),
                         "copyright", copyright,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "kgx-original",
                         #if IS_GENERIC
                         // Translators: "by King’s Cross" here is meaning
                         // author or creator of 'Terminal'
                         "program-name", _("Terminal (King’s Cross)"),
                         #else
                         "program-name", _("King’s Cross"),
                         #endif
                         "version", PACKAGE_VERSION,
                         NULL);
}


static GActionEntry win_entries[] =
{
  { "new-window", new_activated, NULL, NULL, NULL },
  { "new-tab", new_tab_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL },
};


static gboolean
update_subtitle (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      data)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *path = NULL;
  const char *home = NULL;

  file = g_value_dup_object (from_value);
  if (file == NULL) {
    g_value_set_string (to_value, NULL);
    return TRUE;
  }

  path = g_file_get_path (file);
  if (path == NULL) {
    g_value_set_string (to_value, NULL);

    return TRUE;
  }

  home = g_get_home_dir ();
  if (g_str_has_prefix (path, home)) {
    g_autofree char *short_home = g_strdup_printf ("~%s",
                                                   path + strlen (home));

    g_value_set_string (to_value, short_home);

    return TRUE;
  }

  g_value_set_string (to_value, path);

  return TRUE;
}


static void
kgx_window_init (KgxWindow *self)
{
  GPropertyAction *pact;

  gtk_widget_init_template (GTK_WIDGET (self));

  #if IS_GENERIC
  g_object_set (self->about_item,
                "text", _("_About Terminal"),
                NULL);
  #endif

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   win_entries,
                                   G_N_ELEMENTS (win_entries),
                                   self);

  self->theme = KGX_THEME_NIGHT;
  self->close_on_zero = TRUE;

  pact = g_property_action_new ("find",
                                G_OBJECT (self->search_bar),
                                "search-mode-enabled");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (pact));

  g_object_bind_property (self->pages, "title",
                          self, "title",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property_full (self->pages, "path",
                               self->header_bar, "subtitle",
                               G_BINDING_SYNC_CREATE,
                               update_subtitle,
                               NULL, NULL, NULL);

  g_object_bind_property (self, "is-maximized",
                          self->pages, "opaque",
                          G_BINDING_SYNC_CREATE);

  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (self->search_bar),
                                GTK_ENTRY (self->search_entry));
}


/**
 * kgx_window_get_working_dir:
 * @self: the #KgxWindow
 * 
 * Get the working directory path of this window, used to open new windows
 * in the same directory
 * 
 * Since: 0.1.0
 */
char *
kgx_window_get_working_dir (KgxWindow *self)
{
  // TODO: page.similar() ?
  g_autoptr (GFile) file = NULL;

  g_return_val_if_fail (KGX_IS_WINDOW (self), NULL);

  g_object_get (self->pages,
                "path", &file,
                NULL);

  if (!file) {
    return NULL;
  }

  return g_file_get_path (file);
}
