/* kgx-pages.c
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
 * SECTION:kgx-pages
 * @title: KgxPages
 * @short_description: Container of #KgxTab s
 *
 * The container of open #KgxTab (uses #AdwTabView internally)
 */

#include <glib/gi18n.h>

#include "kgx-close-dialog.h"
#include "kgx-config.h"
#include "kgx-pages.h"
#include "kgx-tab.h"
#include "kgx-window.h"
#include "kgx-terminal.h"


typedef struct _KgxPagesPrivate KgxPagesPrivate;
struct _KgxPagesPrivate {
  GtkWidget            *view;

  GtkWidget            *status;
  GtkWidget            *status_revealer;

  int                   last_cols;
  int                   last_rows;
  guint                 timeout;

  gulong                size_watcher;

  KgxTab               *active_page;

  char                 *title;
  GBinding             *title_bind;
  GFile                *path;
  GBinding             *path_bind;

  KgxStatus             page_status;
  GBinding             *page_status_bind;

  gboolean              is_active;
  GBinding             *is_active_bind;

  gboolean              search_mode_enabled;
  GBinding             *search_bind;

  PangoFontDescription *font;
  double                zoom;
  KgxTheme              theme;
  gint64                scrollback_lines;

  AdwTabPage           *action_page;
};


G_DEFINE_TYPE_WITH_PRIVATE (KgxPages, kgx_pages, ADW_TYPE_BIN)


enum {
  PROP_0,
  PROP_TAB_VIEW,
  PROP_TAB_COUNT,
  PROP_TITLE,
  PROP_PATH,
  PROP_THEME,
  PROP_FONT,
  PROP_ZOOM,
  PROP_IS_ACTIVE,
  PROP_STATUS,
  PROP_SEARCH_MODE_ENABLED,
  PROP_SCROLLBACK_LINES,
  LAST_PROP
};
static GParamSpec *pspecs[LAST_PROP] = { NULL, };


enum {
  ZOOM,
  N_SIGNALS
};
static guint signals[N_SIGNALS];


static void
kgx_pages_dispose (GObject *object)
{
  KgxPages *self = KGX_PAGES (object);
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  g_clear_handle_id (&priv->timeout, g_source_remove);

  if (priv->active_page) {
    g_clear_signal_handler (&priv->size_watcher, priv->active_page);
  }

  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->path);

  g_clear_pointer (&priv->font, pango_font_description_free);

  G_OBJECT_CLASS (kgx_pages_parent_class)->dispose (object);
}


static void
kgx_pages_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  KgxPages *self = KGX_PAGES (object);
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  switch (property_id) {
    case PROP_TAB_COUNT:
      g_value_set_uint (value, adw_tab_view_get_n_pages (ADW_TAB_VIEW (priv->view)));
      break;
    case PROP_TAB_VIEW:
      g_value_set_object (value, priv->view);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_PATH:
      g_value_set_object (value, priv->path);
      break;
    case PROP_THEME:
      g_value_set_enum (value, priv->theme);
      break;
    case PROP_FONT:
      g_value_set_boxed (value, priv->font);
      break;
    case PROP_ZOOM:
      g_value_set_double (value, priv->zoom);
      break;
    case PROP_IS_ACTIVE:
      g_value_set_boolean (value, priv->is_active);
      break;
    case PROP_STATUS:
      g_value_set_flags (value, priv->page_status);
      break;
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, priv->search_mode_enabled);
      break;
    case PROP_SCROLLBACK_LINES:
      g_value_set_int64 (value, priv->scrollback_lines);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
kgx_pages_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  KgxPages *self = KGX_PAGES (object);
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  switch (property_id) {
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_PATH:
      g_clear_object (&priv->path);
      priv->path = g_value_dup_object (value);
      break;
    case PROP_THEME:
      priv->theme = g_value_get_enum (value);
      break;
    case PROP_FONT:
      if (priv->font) {
        g_boxed_free (PANGO_TYPE_FONT_DESCRIPTION, priv->font);
      }
      priv->font = g_value_dup_boxed (value);
      break;
    case PROP_ZOOM:
      priv->zoom = g_value_get_double (value);
      break;
    case PROP_IS_ACTIVE:
      priv->is_active = g_value_get_boolean (value);
      break;
    case PROP_STATUS:
      priv->page_status = g_value_get_flags (value);
      break;
    case PROP_SEARCH_MODE_ENABLED:
      priv->search_mode_enabled = g_value_get_boolean (value);
      break;
    case PROP_SCROLLBACK_LINES:
      priv->scrollback_lines = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static gboolean
size_timeout (KgxPages *self)
{
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  priv->timeout = 0;

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->status_revealer), FALSE);

  return G_SOURCE_REMOVE;
}


static void
size_changed (KgxTab   *tab,
              guint     rows,
              guint     cols,
              KgxPages *self)
{
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);
  g_autofree char *label = NULL;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  if (cols == priv->last_cols && rows == priv->last_rows) {
    return;
  }

  priv->last_cols = cols;
  priv->last_rows = rows;

  if (gtk_window_is_maximized (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))))) {
    // Don't show when maximised as it isn't very interesting
    return;
  }

  g_clear_handle_id (&priv->timeout, g_source_remove);
  priv->timeout = g_timeout_add (800, G_SOURCE_FUNC (size_timeout), self);
  g_source_set_name_by_id (priv->timeout, "[kgx] resize label timeout");

  label = g_strdup_printf ("%i × %i", cols, rows);

  gtk_label_set_label (GTK_LABEL (priv->status), label);
  gtk_widget_show (priv->status_revealer);
  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->status_revealer), TRUE);
}


static void
page_changed (GObject *object, GParamSpec *pspec, KgxPages *self)
{
  KgxPagesPrivate *priv;
  AdwTabPage *page = NULL;
  KgxTab *tab;

  priv = kgx_pages_get_instance_private (self);
  page = adw_tab_view_get_selected_page (ADW_TAB_VIEW (priv->view));

  if (!page) {
    return;
  }

  tab = KGX_TAB (adw_tab_page_get_child (page));

  g_clear_signal_handler (&priv->size_watcher, priv->active_page);
  priv->size_watcher = g_signal_connect (tab,
                                         "size-changed", G_CALLBACK (size_changed),
                                         self);

  g_clear_object (&priv->title_bind);
  priv->title_bind = g_object_bind_property (tab, "tab-title",
                                             self, "title",
                                             G_BINDING_SYNC_CREATE);

  g_clear_object (&priv->path_bind);
  priv->path_bind = g_object_bind_property (tab, "tab-path",
                                            self, "path",
                                            G_BINDING_SYNC_CREATE);

  if (priv->active_page) {
    g_object_set (priv->active_page, "is-active", FALSE, NULL);
  }

  g_clear_object (&priv->is_active_bind);
  priv->is_active_bind = g_object_bind_property (self, "is-active",
                                                 tab, "is-active",
                                                 G_BINDING_SYNC_CREATE);

  g_clear_object (&priv->page_status_bind);
  priv->page_status_bind = g_object_bind_property (tab, "tab-status",
                                                   self, "status",
                                                   G_BINDING_SYNC_CREATE);

  g_clear_object (&priv->search_bind);
  priv->search_bind = g_object_bind_property (tab, "search-mode-enabled",
                                              self, "search-mode-enabled",
                                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  priv->active_page = KGX_TAB (tab);
}


static void
died (KgxTab         *page,
      GtkMessageType  type,
      const char     *message,
      gboolean        success,
      KgxPages       *self)
{
  KgxPagesPrivate *priv;
  AdwTabPage *tab_page;
  gboolean close_on_quit;
  int tab_count;

  priv = kgx_pages_get_instance_private (self);
  tab_page = adw_tab_view_get_page (ADW_TAB_VIEW (priv->view), GTK_WIDGET (page));

  g_object_get (page, "close-on-quit", &close_on_quit, NULL);

  if (!close_on_quit) {
    return;
  }

  g_object_get (self, "tab-count", &tab_count, NULL);

  if (tab_count < 1) {
    return;
  }

  adw_tab_view_close_page (ADW_TAB_VIEW (priv->view), tab_page);
}


static void
zoom (KgxTab   *tab,
      KgxZoom   dir,
      KgxPages *self)
{
  g_signal_emit (self, signals[ZOOM], 0, dir);
}


static void
page_attached (AdwTabView *view,
               AdwTabPage *page,
               int         position,
               KgxPages   *self)
{
  KgxTab *tab;

  g_return_if_fail (ADW_IS_TAB_PAGE (page));

  tab = KGX_TAB (adw_tab_page_get_child (page));

  g_object_connect (tab,
                    "signal::died", G_CALLBACK (died), self,
                    "signal::zoom", G_CALLBACK (zoom), self,
                    NULL);

  kgx_tab_set_pages (tab, self);
}


static void
page_detached (AdwTabView *view,
               AdwTabPage *page,
               int         position,
               KgxPages   *self)
{
  KgxTab *tab;
  KgxPagesPrivate *priv;
  GtkRoot *root;

  g_return_if_fail (ADW_IS_TAB_PAGE (page));

  tab = KGX_TAB (adw_tab_page_get_child (page));

  priv = kgx_pages_get_instance_private (self);

  kgx_tab_set_pages (tab, NULL);

  g_signal_handlers_disconnect_by_data (tab, self);

  if (adw_tab_view_get_n_pages (ADW_TAB_VIEW (priv->view)) == 0) {
    root = gtk_widget_get_root (GTK_WIDGET (self));

    if (GTK_IS_WINDOW (root)) {
      /* Not a massive fan, would prefer it if window observed pages is empty */
      gtk_window_close (GTK_WINDOW (root));
    }

    priv->active_page = NULL;
    priv->size_watcher = 0;
  }
}


static AdwTabView *
create_window (AdwTabView *view,
               KgxPages   *self)
{
  /* Perhaps this should be handled via KgxWindow? */
  GtkWindow *window;
  KgxWindow *new_window;
  GtkApplication *app;
  KgxPages *new_pages;
  KgxPagesPrivate *priv;
  int width, height;

  window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  app = gtk_window_get_application (window);

  kgx_window_get_size (KGX_WINDOW (window), &width, &height);

  new_window = g_object_new (KGX_TYPE_WINDOW,
                             "application", app,
                             "default-width", width,
                             "default-height", height,
                             NULL);

  new_pages = kgx_window_get_pages (new_window);
  priv = kgx_pages_get_instance_private (new_pages);

  gtk_window_present (GTK_WINDOW (new_window));

  return ADW_TAB_VIEW (priv->view);
}


static void
close_response (AdwTabPage *page,
                const char *response)
{
  KgxTab *tab = KGX_TAB (adw_tab_page_get_child (page));
  KgxPages *self = kgx_tab_get_pages (tab);
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  adw_tab_view_close_page_finish (ADW_TAB_VIEW (priv->view), page,
                                  !g_strcmp0 (response, "close"));
}


static gboolean
close_page (AdwTabView *view,
            AdwTabPage *page,
            KgxPages   *self)
{
  GtkWidget *dlg;
  g_autoptr (GPtrArray) children = NULL;
  GtkRoot *root;

  children = kgx_tab_get_children (KGX_TAB (adw_tab_page_get_child (page)));

  if (children->len < 1) {
    return FALSE; // Aka no, I don’t want to block closing
  }

  root = gtk_widget_get_root (GTK_WIDGET (self));

  dlg = kgx_close_dialog_new (KGX_CONTEXT_TAB, children);

  gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (root));

  g_signal_connect_swapped (dlg, "response", G_CALLBACK (close_response), page);

  gtk_widget_show (dlg);

  return TRUE; // Block the close
}


static void
setup_menu (AdwTabView *view,
            AdwTabPage *page,
            KgxPages   *self)
{
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  priv->action_page = page;
}


static void
check_revealer (GtkRevealer *revealer,
                GParamSpec  *pspec,
                KgxPages    *self)
{
  if (!gtk_revealer_get_child_revealed (revealer))
    gtk_widget_hide (GTK_WIDGET (revealer));
}


static gboolean
status_to_icon (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  KgxStatus status = g_value_get_flags (from_value);

  if (status & KGX_REMOTE)
    g_value_take_object (to_value, g_themed_icon_new ("status-remote-symbolic"));
  else if (status & KGX_PRIVILEGED)
    g_value_take_object (to_value, g_themed_icon_new ("status-privileged-symbolic"));
  else
    g_value_set_object (to_value, NULL);

  return TRUE;
}


static void
kgx_pages_class_init (KgxPagesClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = kgx_pages_dispose;
  object_class->get_property = kgx_pages_get_property;
  object_class->set_property = kgx_pages_set_property;

  /**
   * KgxPages:tab-view:
   *
   * The #AdwTabView
   *
   * Stability: Private
   */
  pspecs[PROP_TAB_VIEW] =
    g_param_spec_object ("tab-view", "Tab View", "The tab view",
                         ADW_TYPE_TAB_VIEW,
                         G_PARAM_READABLE);

  /**
   * KgxPages:tab-count:
   *
   * The number of open pages
   *
   * Stability: Private
   */
  pspecs[PROP_TAB_COUNT] =
    g_param_spec_uint ("tab-count", "Page Count", "Number of pages",
                       0,
                       G_MAXUINT32,
                       0,
                       G_PARAM_READABLE);

  /**
   * KgxPages:title:
   *
   * The #KgxTab:tab-title of the current #KgxTab
   *
   * Note the writability of this property in an implementation detail, DO NOT
   * set this property
   *
   * Stability: Private
   */
  pspecs[PROP_TITLE] =
    g_param_spec_string ("title", "Title", "The title of the active page",
                         NULL,
                         G_PARAM_READWRITE);

  /**
   * KgxPages:path:
   *
   * The #KgxTab:tab-path of the current #KgxTab
   *
   * Note the writability of this property in an implementation detail, DO NOT
   * set this property
   *
   * Stability: Private
   */
  pspecs[PROP_PATH] =
    g_param_spec_object ("path", "Path", "The path of the active page",
                         G_TYPE_FILE,
                         G_PARAM_READWRITE);

  /**
   * KgxPages:theme:
   *
   * The #KgxTheme to apply to the #KgxTerminal s in the #KgxTab s
   *
   * Stability: Private
   */
  pspecs[PROP_THEME] =
    g_param_spec_enum ("theme", "Theme", "The path of the active page",
                       KGX_TYPE_THEME,
                       KGX_THEME_NIGHT,
                       G_PARAM_READWRITE);

  pspecs[PROP_FONT] =
    g_param_spec_boxed ("font", "Font", "Monospace font",
                         PANGO_TYPE_FONT_DESCRIPTION,
                         G_PARAM_READWRITE);

  pspecs[PROP_ZOOM] =
    g_param_spec_double ("zoom", "Zoom", "Font scaling",
                         KGX_FONT_SCALE_MIN,
                         KGX_FONT_SCALE_MAX,
                         KGX_FONT_SCALE_DEFAULT,
                         G_PARAM_READWRITE);

  pspecs[PROP_IS_ACTIVE] =
    g_param_spec_boolean ("is-active", "Is Active", "Is active pages",
                          FALSE,
                          G_PARAM_READWRITE);

  pspecs[PROP_STATUS] =
    g_param_spec_flags ("status", "Status", "Active page status",
                        KGX_TYPE_STATUS,
                        KGX_NONE,
                        G_PARAM_READWRITE);

  pspecs[PROP_SEARCH_MODE_ENABLED] =
    g_param_spec_boolean ("search-mode-enabled", "Search mode enabled",
                          "Whether the search mode is enabled for active page",
                          FALSE,
                          G_PARAM_READWRITE);

  /**
   * KgxTab:scrollback-lines:
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

  signals[ZOOM] = g_signal_new ("zoom",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                KGX_TYPE_ZOOM);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               KGX_APPLICATION_PATH "kgx-pages.ui");

  gtk_widget_class_bind_template_child_private (widget_class, KgxPages, view);
  gtk_widget_class_bind_template_child_private (widget_class, KgxPages, status);
  gtk_widget_class_bind_template_child_private (widget_class, KgxPages, status_revealer);

  gtk_widget_class_bind_template_callback (widget_class, page_changed);
  gtk_widget_class_bind_template_callback (widget_class, page_attached);
  gtk_widget_class_bind_template_callback (widget_class, page_detached);
  gtk_widget_class_bind_template_callback (widget_class, create_window);
  gtk_widget_class_bind_template_callback (widget_class, close_page);
  gtk_widget_class_bind_template_callback (widget_class, setup_menu);
  gtk_widget_class_bind_template_callback (widget_class, check_revealer);

  gtk_widget_class_set_css_name (widget_class, "pages");
}


static void
kgx_pages_init (KgxPages *self)
{
  KgxPagesPrivate *priv = kgx_pages_get_instance_private (self);

  priv->font = NULL;
  priv->zoom = KGX_FONT_SCALE_DEFAULT;
  priv->theme = KGX_THEME_NIGHT;

  gtk_widget_init_template (GTK_WIDGET (self));

  adw_tab_view_remove_shortcuts (ADW_TAB_VIEW (priv->view),
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_HOME |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_END |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_PAGE_UP |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_PAGE_DOWN |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_HOME |
                                 ADW_TAB_VIEW_SHORTCUT_CONTROL_SHIFT_END);
}


void
kgx_pages_add_page (KgxPages *self,
                    KgxTab   *tab)
{
  KgxPagesPrivate *priv;
  AdwTabPage *page;

  g_return_if_fail (KGX_IS_PAGES (self));

  priv = kgx_pages_get_instance_private (self);

  kgx_tab_set_initial_title (tab, priv->title, priv->path);

  page = adw_tab_view_add_page (ADW_TAB_VIEW (priv->view), GTK_WIDGET (tab), NULL);
  g_object_bind_property (tab, "tab-title", page, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (tab, "tab-tooltip", page, "tooltip", G_BINDING_SYNC_CREATE);
  g_object_bind_property (tab, "needs-attention", page, "needs-attention", G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (tab, "tab-status", page, "icon", G_BINDING_SYNC_CREATE,
                               status_to_icon, NULL, NULL, NULL);
}


void
kgx_pages_remove_page (KgxPages *self,
                       KgxTab   *page)
{
  KgxPagesPrivate *priv;
  AdwTabPage *tab_page;

  g_return_if_fail (KGX_IS_PAGES (self));

  priv = kgx_pages_get_instance_private (self);

  if (!page)
    {
      tab_page = adw_tab_view_get_selected_page (ADW_TAB_VIEW (priv->view));
      adw_tab_view_close_page (ADW_TAB_VIEW (priv->view), tab_page);
      return;
    }

  g_return_if_fail (KGX_IS_TAB (page));

  tab_page = adw_tab_view_get_page (ADW_TAB_VIEW (priv->view), GTK_WIDGET (page));
  adw_tab_view_close_page (ADW_TAB_VIEW (priv->view), tab_page);
}


/**
 * kgx_pages_focus_page:
 * @self: the #KgxPages
 * @page: the #KgxTab to focus
 *
 * Switch to a page
 */
void
kgx_pages_focus_page (KgxPages *self,
                      KgxTab  *page)
{
  KgxPagesPrivate *priv;
  AdwTabPage *index;

  g_return_if_fail (KGX_IS_PAGES (self));
  g_return_if_fail (KGX_IS_TAB (page));

  priv = kgx_pages_get_instance_private (self);

  index = adw_tab_view_get_page (ADW_TAB_VIEW (priv->view),
                                 GTK_WIDGET (page));

  g_return_if_fail (index != NULL);

  adw_tab_view_set_selected_page (ADW_TAB_VIEW (priv->view), index);

  gtk_widget_grab_focus (GTK_WIDGET (page));
}


/**
 * kgx_pages_current_status:
 * @self: the #KgxPages
 *
 * Get the #KgxStatus of the current #KgxTab
 */
KgxStatus
kgx_pages_current_status (KgxPages *self)
{
  KgxPagesPrivate *priv;

  g_return_val_if_fail (KGX_IS_PAGES (self), KGX_NONE);

  priv = kgx_pages_get_instance_private (self);

  return priv->page_status;
}


/**
 * kgx_pages_count:
 * @self: the #KgxPages
 *
 * Returns: number of #KgxTab s in @self
 */
int
kgx_pages_count (KgxPages *self)
{
  KgxPagesPrivate *priv;

  g_return_val_if_fail (KGX_IS_PAGES (self), KGX_NONE);

  priv = kgx_pages_get_instance_private (self);

  return adw_tab_view_get_n_pages (ADW_TAB_VIEW (priv->view));
}


/**
 * kgx_pages_get_children:
 * @self: the #KgxPages
 *
 * Call kgx_tab_get_children on all #KgxTab s in @self building a
 * combined list
 *
 * Returns: (element-type Kgx.Process) (transfer full): the list of #KgxProcess
 */
GPtrArray *
kgx_pages_get_children (KgxPages *self)
{
  KgxPagesPrivate *priv;
  GPtrArray *children;
  guint n;

  g_return_val_if_fail (KGX_IS_PAGES (self), NULL);

  priv = kgx_pages_get_instance_private (self);

  children = g_ptr_array_new_full (10, (GDestroyNotify) kgx_process_unref);

  n = adw_tab_view_get_n_pages (ADW_TAB_VIEW (priv->view));

  for (guint i = 0; i < n; i++) {
    AdwTabPage *page = adw_tab_view_get_nth_page (ADW_TAB_VIEW (priv->view), i);
    g_autoptr (GPtrArray) page_children = NULL;

    page_children = kgx_tab_get_children (KGX_TAB (adw_tab_page_get_child (page)));

    for (int j = 0; j < page_children->len; j++) {
      g_ptr_array_add (children, g_ptr_array_steal_index (page_children, j));
    }

    // 2.62: g_ptr_array_extend_and_steal (children, page_children);
  };

  return children;
}


void
kgx_pages_close_page (KgxPages *self)
{
  KgxPagesPrivate *priv;
  AdwTabPage *page;

  g_return_if_fail (KGX_IS_PAGES (self));

  priv = kgx_pages_get_instance_private (self);
  page = priv->action_page;

  if (!page)
    page = adw_tab_view_get_selected_page (ADW_TAB_VIEW (priv->view));

  adw_tab_view_close_page (ADW_TAB_VIEW (priv->view), page);
}


void
kgx_pages_detach_page (KgxPages *self)
{
  KgxPagesPrivate *priv;
  AdwTabPage *page;
  AdwTabView *new_view;

  g_return_if_fail (KGX_IS_PAGES (self));

  priv = kgx_pages_get_instance_private (self);
  page = priv->action_page;

  if (!page)
    page = adw_tab_view_get_selected_page (ADW_TAB_VIEW (priv->view));

  new_view = create_window (ADW_TAB_VIEW (priv->view), self);
  adw_tab_view_transfer_page (ADW_TAB_VIEW (priv->view), page, new_view, 0);
}
