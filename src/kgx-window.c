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

#define KGX_WINDOW_STYLE_ROOT "root"
#define KGX_WINDOW_STYLE_REMOTE "remote"

G_DEFINE_TYPE (KgxWindow, kgx_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_INITIAL_WORK_DIR,
  PROP_COMMAND,
  PROP_CLOSE_ON_ZERO,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };


static void
wait_cb (GPid     pid,
         gint     status,
         gpointer user_data)

{
  KgxWindow *self = KGX_WINDOW (user_data);
  #if HAS_GTOP
  GtkApplication *app = NULL;
  #endif
  g_autoptr (GError) error = NULL;
  GtkStyleContext *context = NULL;

  g_return_if_fail (KGX_IS_WINDOW (self));

  #if HAS_GTOP
  app = gtk_window_get_application (GTK_WINDOW (self));

  // If the application is in closing the app may already be null
  if (app) {
    kgx_application_remove_watch (KGX_APPLICATION (app), pid);
  }
  #endif

  // If the window has already closed we can't do much
  if (self->exit_info == NULL) {
    return;
  }

  /* wait_check will set @error if it got a signal/non-zero exit */
  if (!g_spawn_check_exit_status (status, &error)) {
    g_autofree char *message = NULL;

    context = gtk_widget_get_style_context (GTK_WIDGET (self->exit_info));

    // translators: <b> </b> marks the text as bold, ensure they are
    // matched please!
    message = g_strdup_printf (_("<b>Read Only</b> — Command exited with code %i"),
                               status);

    gtk_label_set_markup (GTK_LABEL (self->exit_message), message);
    gtk_style_context_add_class (context, "error");
  } else if (self->close_on_zero) {
    gtk_widget_destroy (GTK_WIDGET (self));

    return;
  } else {
    context = gtk_widget_get_style_context (GTK_WIDGET (self->exit_info));

    gtk_label_set_markup (GTK_LABEL (self->exit_message),
    // translators: <b> </b> marks the text as bold, ensure they are
    // matched please!
                         _("<b>Read Only</b> — Command exited"));
    gtk_style_context_remove_class (context, "error");
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->exit_info), TRUE);
}


static void
spawned (VtePty       *pty,
         GAsyncResult *res,
         gpointer      data)

{
  g_autoptr(GError) error = NULL;
  KgxWindow *self = KGX_WINDOW (data);
  #if HAS_GTOP
  GtkApplication *app = NULL;
  #endif
  GPid pid;

  g_return_if_fail (VTE_IS_PTY (pty));
  g_return_if_fail (G_IS_ASYNC_RESULT (res));

  fp_vte_pty_spawn_finish (pty, res, &pid, &error);

  if (error) {
    GtkStyleContext *context;
    g_autofree char *message = NULL;

    g_critical ("Failed to spawn: %s", error->message);

    context = gtk_widget_get_style_context (GTK_WIDGET (self->exit_info));

    gtk_style_context_add_class (context, "error");

    // translators: <b> </b> marks the text as bold, ensure they are
    // matched please!
    message = g_strdup_printf (_("<b>Failed to start</b> — %s"),
                               error->message);
   
    gtk_label_set_markup (GTK_LABEL (self->exit_message), message);

    gtk_revealer_set_reveal_child (GTK_REVEALER (self->exit_info), TRUE);

    return;
  }

  #if HAS_GTOP
  app = gtk_window_get_application (GTK_WINDOW (self));

  kgx_application_add_watch (KGX_APPLICATION (app),
                             pid,
                             KGX_WINDOW (self));
  #endif

  g_child_watch_add (pid, wait_cb, self);
}


static void
kgx_window_constructed (GObject *object)
{
  KgxWindow          *self = KGX_WINDOW (object);
  const char         *initial = NULL;
  g_autoptr (VtePty)  pty = NULL;
  g_autoptr (GError)  error = NULL;
  g_auto (GStrv)      shell = NULL;
  g_auto (GStrv)      env = NULL;
  g_autofree char    *command = NULL;
  guint               id = 0;
  GtkWidget *page;
  
  id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (self));

  self->notification_id = g_strdup_printf ("command-completed-%u", id);

  pty = vte_pty_new_sync (fp_vte_pty_default_flags (), NULL, &error);

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

  env = g_environ_setenv (env, "TERM", "xterm-256color", TRUE);

  page = g_object_new (KGX_TYPE_LOCAL_PAGE,
                                  "visible", TRUE,
                                  NULL);

  vte_terminal_set_pty (VTE_TERMINAL (KGX_LOCAL_PAGE (page)->terminal), pty);
  fp_vte_pty_spawn_async (pty,
                          initial,
                          (const gchar * const *) shell,
                          (const gchar * const *) env,
                          -1,
                          NULL,
                          (GAsyncReadyCallback) spawned,
                          self);

  kgx_pages_add_page (KGX_PAGES (self->pages), KGX_PAGE (page));

  G_OBJECT_CLASS (kgx_window_parent_class)->constructed (object);
}

static void
kgx_window_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  KgxWindow *self = KGX_WINDOW (object);

  switch (property_id) {
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
  g_clear_pointer (&self->notification_id, g_free);

  g_clear_pointer (&self->root, g_hash_table_unref);
  g_clear_pointer (&self->remote, g_hash_table_unref);

  G_OBJECT_CLASS (kgx_window_parent_class)->finalize (object);
}

#if HAS_GTOP
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
  GHashTableIter iter;
  GtkWidget *dlg;
  gpointer pid, process;

  if (g_hash_table_size (self->children) < 1 || self->close_anyway) {
    GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));

    g_application_withdraw_notification (G_APPLICATION (app),
                                         self->notification_id);

    return FALSE; // Aka no, I don't want to block closing
  }

  dlg = g_object_new (KGX_TYPE_CLOSE_DIALOG,
                      "transient-for", self,
                      "use-header-bar", TRUE,
                      NULL);

  g_signal_connect (dlg, "response", G_CALLBACK (delete_response), self);

  g_hash_table_iter_init (&iter, self->children);
  while (g_hash_table_iter_next (&iter, &pid, &process)) {
    kgx_close_dialog_add_command (KGX_CLOSE_DIALOG (dlg),
                                  kgx_process_get_exec (process));
  }

  gtk_widget_show (dlg);
  
  return TRUE; // Block the close
}
#else
static gboolean
kgx_window_delete_event (GtkWidget   *widget,
                         GdkEventAny *event)
{
  return FALSE; // Aka no, I don't want to block closing
}
#endif


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
application_set (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkApplication *app;
  KgxWindow *self = KGX_WINDOW (object);

  app = gtk_window_get_application (GTK_WINDOW (object));

  g_object_bind_property (app,
                          "theme",
                          self->pages,
                          "theme",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (app,
                          "font",
                          self->pages,
                          "font",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (app,
                          "font-scale",
                          self->pages,
                          "zoom",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (app,
                    "notify::font-scale",
                    G_CALLBACK (zoomed),
                    self);

  update_zoom (self, KGX_APPLICATION (app));
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

  gtk_widget_class_bind_template_callback (widget_class, application_set);
  gtk_widget_class_bind_template_callback (widget_class, active_changed);

  gtk_widget_class_bind_template_callback (widget_class, search_enabled);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_next);
  gtk_widget_class_bind_template_callback (widget_class, search_prev);

  gtk_widget_class_bind_template_callback (widget_class, zoom);
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
                               "2019");

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

  self->root = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->remote = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->children = g_hash_table_new_full (g_direct_hash,
                                          g_direct_equal,
                                          NULL,
                                          (GDestroyNotify) kgx_process_unref);

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

  return g_file_get_path (file);
}


#if HAS_GTOP
static inline void
push_type (GHashTable      *table,
           GPid             pid,
           KgxProcess      *process,
           GtkStyleContext *context,
           const char      *class_name)
{
  g_hash_table_insert (table,
                       GINT_TO_POINTER (pid),
                       process != NULL ? g_rc_box_acquire (process) : NULL);

  g_debug ("Now %i %s", g_hash_table_size (table), class_name);

  if (G_LIKELY (class_name != NULL)) {
    gtk_style_context_add_class (context, class_name);
  }
}
#endif

/**
 * kgx_window_push_child:
 * @self: the #KgxWindow
 * @process: the #KgxProcess of the remote process
 * 
 * Registers @pid as a child of @self
 * 
 * Since: 0.2.0
 */
void
kgx_window_push_child (KgxWindow    *self,
                       KgxProcess   *process)
{
  #if HAS_GTOP
  GtkStyleContext *context;
  GPid pid = 0;
  const char *exec = NULL;

  g_return_if_fail (KGX_IS_WINDOW (self));

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  pid = kgx_process_get_pid (process);
  exec = kgx_process_get_exec (process);

  if (G_UNLIKELY (g_str_has_prefix (exec, "ssh "))) {
    push_type (self->remote, pid, NULL, context, KGX_WINDOW_STYLE_REMOTE);
  }

  if (G_UNLIKELY (kgx_process_get_is_root (process))) {
    push_type (self->root, pid, NULL, context, KGX_WINDOW_STYLE_ROOT);
  }

  push_type (self->children, pid, process, context, NULL);
  #endif
}

inline static void
pop_type (GHashTable      *table,
          GPid             pid,
          GtkStyleContext *context,
          const char      *class_name)
{
  guint size = 0;

  g_hash_table_remove (table, GINT_TO_POINTER (pid));

  size = g_hash_table_size (table);

  if (G_LIKELY (size <= 0)) {
    if (G_LIKELY (class_name)) {
      gtk_style_context_remove_class (context, class_name);
    }
    g_debug ("No longer %s", class_name);
  } else {
    g_debug ("%i %s remaining", size, class_name);
  }
}

/**
 * kgx_window_pop_child:
 * @self: the #KgxWindow
 * @process: the #KgxProcess of the child process
 * 
 * Remove a child added with kgx_window_push_child()
 * 
 * Since: 0.2.0
 */
void
kgx_window_pop_child (KgxWindow    *self,
                      KgxProcess   *process)
{
  GtkStyleContext *context;
  GPid pid = 0;
  guint id = 0;

  g_return_if_fail (KGX_IS_WINDOW (self));

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  #if HAS_GTOP
  pid = kgx_process_get_pid (process);
  #endif
  
  pop_type (self->remote, pid, context, KGX_WINDOW_STYLE_REMOTE);
  pop_type (self->root, pid, context, KGX_WINDOW_STYLE_ROOT);
  pop_type (self->children, pid, context, NULL);
  
  if (!gtk_window_is_active (GTK_WINDOW (self))) {
    g_autoptr (GNotification) noti = NULL;
    GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));

    noti = g_notification_new (_("Command completed"));
    #if HAS_GTOP
    g_notification_set_body (noti, kgx_process_get_exec (process));
    #endif
    id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (self));
    g_notification_set_default_action_and_target (noti,
                                                  "app.focus-window",
                                                  "u",
                                                  id);

    g_application_send_notification (G_APPLICATION (app),
                                     self->notification_id,
                                     noti);
  }
}
