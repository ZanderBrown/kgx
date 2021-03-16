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
 * The main #HdyApplicationWindow that acts as the terminal
 * 
 * Since: 0.1.0
 */

#include "kgx-config.h"

#include <glib/gi18n.h>
#include <vte/vte.h>
#include <math.h>
#include <handy.h>

#include "rgba.h"

#include "kgx-window.h"
#include "kgx-application.h"
#include "kgx-process.h"
#include "kgx-close-dialog.h"
#include "kgx-pages.h"

G_DEFINE_TYPE (KgxWindow, kgx_window, HDY_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_APPLICATION,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };


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
  g_autoptr (GError)  error = NULL;
  GtkApplication     *application = NULL;

  G_OBJECT_CLASS (kgx_window_parent_class)->constructed (object);

  application = gtk_window_get_application (GTK_WINDOW (self));

  g_object_bind_property (application, "theme",
                          self->pages, "theme",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (application, "font",
                          self->pages, "font",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (application, "font-scale",
                          self->pages, "zoom",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (application, "scrollback-lines",
                          self->pages, "scrollback-lines",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (application,
                    "notify::font-scale", G_CALLBACK (zoomed),
                    self);

  update_zoom (self, KGX_APPLICATION (application));
}


static void
kgx_window_dispose (GObject *object)
{
  KgxWindow *self = KGX_WINDOW (object);

  g_clear_object (&self->tab_actions);

  G_OBJECT_CLASS (kgx_window_parent_class)->dispose (object);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
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
    return FALSE; // Aka no, I don’t want to block closing
  }

  dlg = kgx_close_dialog_new (KGX_CONTEXT_WINDOW, children);

  gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (self));

  g_signal_connect (dlg, "response", G_CALLBACK (delete_response), self);

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


static gboolean
key_press_event (GtkWidget   *widget,
                 GdkEventKey *event,
                 KgxWindow   *self)
{
  GdkModifierType default_modifiers = gtk_accelerator_get_default_mod_mask ();
  guint keyval;
  GdkModifierType state;
  GdkModifierType consumed;
  GdkKeymap *keymap;

  gdk_event_get_state ((GdkEvent *) event, &state);

  keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));

  gdk_keymap_translate_keyboard_state (keymap,
                                       event->hardware_keycode,
                                       state,
                                       event->group,
                                       &keyval, NULL, NULL, &consumed);

  state &= ~consumed & default_modifiers;

  /* Override some shortcuts from HdyTabView:shortcut-widget back, as they are
   * needed for terminal apps. This should be fixed on libhandy side, but would
   * likely require API changes, so carry this for now.
   *
   * See https://gitlab.gnome.org/GNOME/libhandy/-/issues/422
   */
  if ((keyval == GDK_KEY_Page_Up ||
       keyval == GDK_KEY_KP_Page_Up ||
       keyval == GDK_KEY_Page_Down ||
       keyval == GDK_KEY_KP_Page_Down ||
       keyval == GDK_KEY_Home ||
       keyval == GDK_KEY_KP_Home ||
       keyval == GDK_KEY_End ||
       keyval == GDK_KEY_KP_End) &&
      (state == GDK_CONTROL_MASK ||
       state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))) {
    return kgx_pages_key_press_event (KGX_PAGES (self->pages),
                                      (GdkEvent *) event);
  }

  return GDK_EVENT_PROPAGATE;
}


static void
zoom (KgxPages  *pages,
      KgxZoom    dir,
      KgxWindow *self)
{
  GAction *action = NULL;
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));

  switch (dir) {
    case KGX_ZOOM_IN:
      action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-in");
      break;
    case KGX_ZOOM_OUT:
      action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-out");
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
extra_drag_data_received (HdyTabBar        *bar,
                          HdyTabPage       *page,
                          GdkDragContext   *context,
                          GtkSelectionData *selection_data,
                          guint             info,
                          guint             time,
                          KgxWindow        *self)
{
  KgxTab *tab = KGX_TAB (hdy_tab_page_get_child (page));

  kgx_tab_accept_drop (tab, selection_data);
}


static void new_tab_activated (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       data);


static void
new_tab_cb (HdyTabSwitcher *switcher,
            KgxWindow      *self)
{
  new_tab_activated (NULL, NULL, self);
}


static gboolean
kgx_window_window_state_event (GtkWidget           *widget,
                               GdkEventWindowState *event)
{
  KgxWindow *self = KGX_WINDOW (widget);

  self->is_maximized_or_tiled =
    (event->new_window_state & (GDK_WINDOW_STATE_FULLSCREEN |
                                GDK_WINDOW_STATE_MAXIMIZED |
                                GDK_WINDOW_STATE_TILED |
                                GDK_WINDOW_STATE_TOP_TILED |
                                GDK_WINDOW_STATE_RIGHT_TILED |
                                GDK_WINDOW_STATE_BOTTOM_TILED |
                                GDK_WINDOW_STATE_LEFT_TILED)) > 0;

  g_object_set (self->pages, "opaque", self->is_maximized_or_tiled, NULL);

  if (self->is_maximized_or_tiled)
    gtk_style_context_add_class (gtk_widget_get_style_context (widget), "opaque");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "opaque");

  return GTK_WIDGET_CLASS (kgx_window_parent_class)->window_state_event (widget, event);
}


static void
kgx_window_size_allocate (GtkWidget     *widget,
                          GtkAllocation *alloc)
{
  KgxWindow *self = KGX_WINDOW (widget);

  GTK_WIDGET_CLASS (kgx_window_parent_class)->size_allocate (widget, alloc);

  if (!self->is_maximized_or_tiled)
    gtk_window_get_size (GTK_WINDOW (self),
                         &self->current_width,
                         &self->current_height);
}


static void
kgx_window_class_init (KgxWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = kgx_window_constructed;
  object_class->dispose = kgx_window_dispose;
  object_class->set_property = kgx_window_set_property;
  object_class->get_property = kgx_window_get_property;

  widget_class->delete_event = kgx_window_delete_event;
  widget_class->window_state_event = kgx_window_window_state_event;
  widget_class->size_allocate = kgx_window_size_allocate;

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

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               RES_PATH "kgx-window.ui");

  gtk_widget_class_bind_template_child (widget_class, KgxWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, exit_info);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, exit_message);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, zoom_level);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, about_item);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, tab_bar);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, tab_button);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, tab_switcher);
  gtk_widget_class_bind_template_child (widget_class, KgxWindow, pages);

  gtk_widget_class_bind_template_callback (widget_class, active_changed);
  gtk_widget_class_bind_template_callback (widget_class, key_press_event);

  gtk_widget_class_bind_template_callback (widget_class, zoom);
  gtk_widget_class_bind_template_callback (widget_class, status_changed);
  gtk_widget_class_bind_template_callback (widget_class, extra_drag_data_received);
  gtk_widget_class_bind_template_callback (widget_class, new_tab_cb);
}


static void
new_activated (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  KgxWindow *self = data;
  guint32 timestamp = GDK_CURRENT_TIME;
  GtkApplication *application = NULL;
  g_autoptr (GFile) dir = NULL;

  application = gtk_window_get_application (GTK_WINDOW (self));
  dir = kgx_window_get_working_dir (KGX_WINDOW (data));

  kgx_application_add_terminal (KGX_APPLICATION (application),
                                NULL,
                                timestamp,
                                dir,
                                NULL,
                                NULL);
}


static void
new_tab_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  KgxWindow *self = data;
  guint32 timestamp = GDK_CURRENT_TIME;
  GtkApplication *application = NULL;
  g_autoptr (GFile) dir = NULL;

  application = gtk_window_get_application (GTK_WINDOW (self));
  dir = kgx_window_get_working_dir (KGX_WINDOW (data));

  kgx_application_add_terminal (KGX_APPLICATION (application),
                                self,
                                timestamp,
                                dir,
                                NULL,
                                NULL);
}


static void
close_tab_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  KgxWindow *self = data;

  kgx_pages_close_page (KGX_PAGES (self->pages));
}


static void
detach_tab_activated (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       data)
{
  KgxWindow *self = data;

  kgx_pages_detach_page (KGX_PAGES (self->pages));
}


static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
  const char *authors[] = { "Zander Brown <zbrown@gnome.org>", NULL };
  const char *artists[] = { "Tobias Bernard", NULL };
  g_autofree char *copyright = NULL;
  
  /* Translators: %s is the year range */
  copyright = g_strdup_printf (_("© %s Zander Brown"), "2019-2021");

  gtk_show_about_dialog (GTK_WINDOW (data),
                         "authors", authors,
                         "artists", artists,
                         /* Translators: Credit yourself here */
                         "translator-credits", _("translator-credits"),
                         #if IS_GENERIC
                         /* Translators: Don’t attempt to translate KGX,
                          * treat it as a proper noun */
                         "comments", _("KGX Terminal Emulator"),
                         #else
                         "comments", _("Terminal Emulator"),
                         #endif
                         "copyright", copyright,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "kgx-original",
                         #if IS_GENERIC
                         "program-name", _("Terminal"),
                         #else
                         "program-name", _("King’s Cross"),
                         #endif
                         "version", PACKAGE_VERSION,
                         NULL);
}


static void
tab_switcher_activated (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data)
{
  KgxWindow *self = data;

  hdy_tab_switcher_open (HDY_TAB_SWITCHER (self->tab_switcher));
}


static GActionEntry win_entries[] = {
  { "new-window", new_activated, NULL, NULL, NULL },
  { "new-tab", new_tab_activated, NULL, NULL, NULL },
  { "close-tab", close_tab_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL },
  { "tab-switcher", tab_switcher_activated, NULL, NULL, NULL },
};


static GActionEntry tab_entries[] = {
  { "close", close_tab_activated, NULL, NULL, NULL },
  { "detach", detach_tab_activated, NULL, NULL, NULL },
};


static gboolean
update_title (GBinding     *binding,
              const GValue *from_value,
              GValue       *to_value,
              gpointer      data)
{
  const char *title = g_value_get_string (from_value);

  if (!title) {
#if IS_GENERIC
    title = _("Terminal");
#else
    title = _("King’s Cross");
#endif
  }

  g_value_set_string (to_value, title);

  return TRUE;
}


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
  g_autoptr (GtkWindowGroup) group = NULL;
  g_autoptr (GtkTargetList) target_list = NULL;
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

  pact = g_property_action_new ("find",
                                G_OBJECT (self->pages),
                                "search-mode-enabled");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (pact));

  g_object_bind_property_full (self->pages, "title",
                               self, "title",
                               G_BINDING_SYNC_CREATE,
                               update_title,
                               NULL, NULL, NULL);

  g_object_bind_property (self, "title",
                          self->header_bar, "title",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property_full (self->pages, "path",
                               self->header_bar, "subtitle",
                               G_BINDING_SYNC_CREATE,
                               update_subtitle,
                               NULL, NULL, NULL);

  g_object_bind_property (self->pages, "tab-view",
                          self->tab_bar, "view",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->pages, "tab-view",
                          self->tab_button, "view",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->pages, "tab-view",
                          self->tab_switcher, "view",
                          G_BINDING_SYNC_CREATE);

  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);

  hdy_tab_bar_set_extra_drag_dest_targets (HDY_TAB_BAR (self->tab_bar),
                                           target_list);

  group = gtk_window_group_new ();
  gtk_window_group_add_window (group, GTK_WINDOW (self));

  kgx_pages_set_shortcut_widget (KGX_PAGES (self->pages), GTK_WIDGET (self));

  self->tab_actions = G_ACTION_MAP (g_simple_action_group_new ());
  g_action_map_add_action_entries (self->tab_actions,
                                   tab_entries,
                                   G_N_ELEMENTS (tab_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "tab",
                                  G_ACTION_GROUP (self->tab_actions));
}


/**
 * kgx_window_get_working_dir:
 * @self: the #KgxWindow
 * 
 * Get the working directory path of this window, used to open new windows
 * in the same directory
 * 
 * Since: 0.5.0
 */
GFile *
kgx_window_get_working_dir (KgxWindow *self)
{
  GFile *file = NULL;

  g_return_val_if_fail (KGX_IS_WINDOW (self), NULL);

  g_object_get (self->pages, "path", &file, NULL);

  return file;
}


/**
 * kgx_window_get_pages:
 * @self: the #KgxWindow
 * 
 * Get the tabbed widget inside @self
 * 
 * Since: 0.3.0
 */
KgxPages *
kgx_window_get_pages (KgxWindow *self)
{
  g_return_val_if_fail (KGX_IS_WINDOW (self), NULL);

  return KGX_PAGES (self->pages);
}


void
kgx_window_get_size (KgxWindow *self,
                     int       *width,
                     int       *height)
{
  g_return_if_fail (KGX_IS_WINDOW (self));

  if (width)
    *width = self->current_width;
  if (height)
    *height = self->current_height;
}
