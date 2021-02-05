/* kgx-tab.c
 *
 * Copyright 2019-2020 Zander Brown
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
 * SECTION:kgx-page
 * @title: KgxTab
 * @short_description: Base for things in a #KgxPages
 * 
 * Since: 0.3.0
 */

#include <glib/gi18n.h>
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

#include "kgx-config.h"
#include "kgx-tab.h"
#include "kgx-pages.h"
#include "kgx-terminal.h"
#include "kgx-application.h"
#include "util.h"


typedef struct _KgxTabPrivate KgxTabPrivate;
struct _KgxTabPrivate {
  guint                 id;

  KgxApplication       *application;

  char                 *title;
  char                 *tooltip;
  GFile                *path;
  KgxStatus             status;
  PangoFontDescription *font;
  double                zoom;
  gboolean              is_active;
  KgxTheme              theme;
  gboolean              opaque;
  gboolean              close_on_quit;

  KgxTerminal          *terminal;
  gulong                term_size_handler;
  gulong                term_font_inc_handler;
  gulong                term_font_dec_handler;
  GBinding             *term_title_bind;
  GBinding             *term_path_bind;
  GBinding             *term_font_bind;
  GBinding             *term_zoom_bind;
  GBinding             *term_theme_bind;
  GBinding             *term_opaque_bind;

  GBinding             *pages_font_bind;
  GBinding             *pages_zoom_bind;
  GBinding             *pages_theme_bind;
  GBinding             *pages_opaque_bind;

  GtkWidget            *revealer;
  GtkWidget            *label;

  /* Remote/root states */
  GHashTable           *root;
  GHashTable           *remote;
  GHashTable           *children;

  char                 *notification_id;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (KgxTab, kgx_tab, GTK_TYPE_BOX)


enum {
  PROP_0,
  PROP_APPLICATION,
  PROP_TAB_TITLE,
  PROP_TAB_PATH,
  PROP_TAB_STATUS,
  PROP_TAB_TOOLTIP,
  PROP_FONT,
  PROP_ZOOM,
  PROP_THEME,
  PROP_IS_ACTIVE,
  PROP_OPAQUE,
  PROP_CLOSE_ON_QUIT,
  LAST_PROP
};
static GParamSpec *pspecs[LAST_PROP] = { NULL, };


enum {
  SIZE_CHANGED,
  ZOOM,
  DIED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];


static void
kgx_tab_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  KgxTab *self = KGX_TAB (object);
  KgxTabPrivate *priv = kgx_tab_get_instance_private (self);

  switch (property_id) {
    case PROP_APPLICATION:
      g_value_set_object (value, priv->application);
      break;
    case PROP_TAB_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_TAB_PATH:
      g_value_set_object (value, priv->path);
      break;
    case PROP_TAB_STATUS:
      g_value_set_flags (value, priv->status);
      break;
    case PROP_TAB_TOOLTIP:
      g_value_set_string (value, priv->tooltip);
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
    case PROP_THEME:
      g_value_set_enum (value, priv->theme);
      break;
    case PROP_OPAQUE:
      g_value_set_boolean (value, priv->opaque);
      break;
    case PROP_CLOSE_ON_QUIT:
      g_value_set_boolean (value, priv->close_on_quit);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
kgx_tab_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  KgxTab *self = KGX_TAB (object);
  KgxTabPrivate *priv = kgx_tab_get_instance_private (self);

  switch (property_id) {
    case PROP_APPLICATION:
      if (priv->application) {
        g_critical ("Application was already set %p", priv->application);
      }
      priv->application = g_value_dup_object (value);
      kgx_application_add_page (priv->application, self);
      break;
    case PROP_TAB_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_TAB_PATH:
      g_clear_object (&priv->path);
      priv->path = g_value_dup_object (value);
      break;
    case PROP_TAB_STATUS:
      priv->status = g_value_get_flags (value);
      break;
    case PROP_TAB_TOOLTIP:
      g_clear_pointer (&priv->tooltip, g_free);
      priv->tooltip = g_value_dup_string (value);
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
    case PROP_THEME:
      priv->theme = g_value_get_enum (value);
      break;
    case PROP_OPAQUE:
      priv->opaque = g_value_get_boolean (value);
      break;
    case PROP_CLOSE_ON_QUIT:
      priv->close_on_quit = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
kgx_tab_finalize (GObject *object)
{
  KgxTab *self = KGX_TAB (object);
  KgxTabPrivate *priv = kgx_tab_get_instance_private (self);

  g_clear_pointer (&priv->root, g_hash_table_unref);
  g_clear_pointer (&priv->remote, g_hash_table_unref);
  g_clear_pointer (&priv->children, g_hash_table_unref);

  g_application_withdraw_notification (G_APPLICATION (priv->application),
                                       priv->notification_id);

  g_clear_object (&priv->application);

  g_clear_pointer (&priv->notification_id, g_free);
}


static void
kgx_tab_real_start (KgxTab              *tab,
                    GAsyncReadyCallback  callback,
                    gpointer             callback_data)
{
  g_critical ("%s doesn't implement start", G_OBJECT_TYPE_NAME (tab));
}


static GPid
kgx_tab_real_start_finish (KgxTab        *tab,
                           GAsyncResult  *res,
                           GError       **error)
{
  g_critical ("%s doesn't implement start_finish", G_OBJECT_TYPE_NAME (tab));

  return 0;
}


static void
kgx_tab_class_init (KgxTabClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS   (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  KgxTabClass    *tab_class    = KGX_TAB_CLASS    (klass);
  
  object_class->get_property = kgx_tab_get_property;
  object_class->set_property = kgx_tab_set_property;
  object_class->finalize = kgx_tab_finalize;

  tab_class->start = kgx_tab_real_start;
  tab_class->start_finish = kgx_tab_real_start_finish;

  /**
   * KgxTab:application:
   * 
   * The #KgxApplication this tab is running under
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_APPLICATION] =
    g_param_spec_object ("application", "Application", "The application",
                         KGX_TYPE_APPLICATION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /**
   * KgxTab:tab-title:
   * 
   * Title of this tab
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_TAB_TITLE] =
    g_param_spec_string ("tab-title", "Page Title", "Title for this tab",
                         NULL,
                         G_PARAM_READWRITE);

  /**
   * KgxTab:tab-path:
   * 
   * Current path of this tab
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_TAB_PATH] =
    g_param_spec_object ("tab-path", "Page Path", "Current path",
                         G_TYPE_FILE,
                         G_PARAM_READWRITE);

  /**
   * KgxTab:tab-status:
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_TAB_STATUS] =
    g_param_spec_flags ("tab-status", "Page Status", "Session status",
                        KGX_TYPE_STATUS,
                        KGX_NONE,
                        G_PARAM_READWRITE);

  pspecs[PROP_TAB_TOOLTIP] =
    g_param_spec_string ("tab-tooltip", "Tab Tooltip",
                         "Extra information to show in the tooltip",
                         NULL,
                         G_PARAM_READWRITE);

  pspecs[PROP_FONT] =
    g_param_spec_boxed ("font", "Font", "Monospace font",
                         PANGO_TYPE_FONT_DESCRIPTION,
                         G_PARAM_READWRITE);

  pspecs[PROP_ZOOM] =
    g_param_spec_double ("zoom", "Zoom", "Font scaling",
                         0.5, 2.0, 1.0,
                         G_PARAM_READWRITE);

  /**
   * KgxTab:is-active:
   * 
   * This is the active tab of the active window
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_IS_ACTIVE] =
    g_param_spec_boolean ("is-active", "Is Active", "Current tab",
                          FALSE,
                          G_PARAM_READWRITE);

  /**
   * KgxTab:theme:
   * 
   * The #KgxTheme to apply to the #KgxTerminal s in the #KgxTab
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_THEME] =
    g_param_spec_enum ("theme", "Theme", "The path of the active tab",
                       KGX_TYPE_THEME,
                       KGX_THEME_NIGHT,
                       G_PARAM_READWRITE);

  /**
   * KgxTab:opaque:
   * 
   * Whether to disable transparency
   * 
   * Bound to #GtkWindow:is-maximized on the #KgxWindow
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_OPAQUE] =
    g_param_spec_boolean ("opaque", "Opaque", "Terminal opaqueness",
                          FALSE,
                          G_PARAM_READWRITE);

  pspecs[PROP_CLOSE_ON_QUIT] =
    g_param_spec_boolean ("close-on-quit", "Close on quit",
                          "Should the tab close when dead",
                          FALSE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[SIZE_CHANGED] = g_signal_new ("size-changed",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        2,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT);

  signals[ZOOM] = g_signal_new ("zoom",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                KGX_TYPE_ZOOM);

  signals[DIED] = g_signal_new ("died",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL,
                                G_TYPE_NONE,
                                3,
                                GTK_TYPE_MESSAGE_TYPE,
                                G_TYPE_STRING,
                                G_TYPE_BOOLEAN);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               RES_PATH "kgx-tab.ui");

  gtk_widget_class_bind_template_child_private (widget_class, KgxTab, revealer);
  gtk_widget_class_bind_template_child_private (widget_class, KgxTab, label);
}


static void
parent_set (KgxTab    *self,
            GtkWidget *old_parent)
{
  KgxTabPrivate *priv;
  GtkWidget *parent;
  KgxPages *pages;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  g_clear_object (&priv->pages_font_bind);
  g_clear_object (&priv->pages_zoom_bind);
  g_clear_object (&priv->pages_theme_bind);
  g_clear_object (&priv->pages_opaque_bind);

  if (parent == NULL) {
    return;
  }

  parent = gtk_widget_get_ancestor (parent, KGX_TYPE_PAGES);

  g_return_if_fail (KGX_IS_PAGES (parent));

  pages = KGX_PAGES (parent);

  priv->pages_font_bind = g_object_bind_property (pages,
                                                  "font",
                                                  self,
                                                  "font",
                                                  G_BINDING_SYNC_CREATE);
  priv->pages_zoom_bind = g_object_bind_property (pages,
                                                  "zoom",
                                                  self,
                                                  "zoom",
                                                  G_BINDING_SYNC_CREATE);
  priv->pages_theme_bind = g_object_bind_property (pages,
                                                   "theme",
                                                   self,
                                                   "theme",
                                                   G_BINDING_SYNC_CREATE);
  priv->pages_opaque_bind = g_object_bind_property (pages,
                                                    "opaque",
                                                    self,
                                                    "opaque",
                                                    G_BINDING_SYNC_CREATE);
}


static void
died (KgxTab         *self,
      GtkMessageType  type,
      const char     *message,
      gboolean        success)
{
  KgxTabPrivate *priv;
  GtkStyleContext *context;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  gtk_label_set_markup (GTK_LABEL (priv->label), message);

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->revealer));

  if (type == GTK_MESSAGE_ERROR) {
    gtk_style_context_add_class (context, "error");
  } else {
    gtk_style_context_remove_class (context, "error");
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);

  if (priv->close_on_quit && success) {
    kgx_pages_remove_page (kgx_tab_get_pages (self), self);
  }
}


static void
kgx_tab_init (KgxTab *self)
{
  static guint last_id = 0;
  KgxTabPrivate *priv = kgx_tab_get_instance_private (self);

  last_id++;

  priv->id = last_id;

  priv->root = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->remote = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->children = g_hash_table_new_full (g_direct_hash,
                                          g_direct_equal,
                                          NULL,
                                          (GDestroyNotify) kgx_process_unref);

  priv->notification_id = g_strdup_printf ("command-completed-%u", priv->id);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "parent-set", G_CALLBACK (parent_set), NULL);
  g_signal_connect (self, "died", G_CALLBACK (died), NULL);
}


static void
size_changed (KgxTerminal *term,
              guint        rows,
              guint        cols,
              KgxTab      *self)
{
  g_signal_emit (self, signals[SIZE_CHANGED], 0, rows, cols);
}


static void
font_increase (KgxTerminal *term,
               KgxTab      *self)
{
  g_signal_emit (self, signals[ZOOM], 0, KGX_ZOOM_IN);
}


static void
font_decrease (KgxTerminal *term,
               KgxTab      *self)
{
  g_signal_emit (self, signals[ZOOM], 0, KGX_ZOOM_OUT);
}


void
kgx_tab_connect_terminal (KgxTab      *self,
                          KgxTerminal *term)
{
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  g_return_if_fail (KGX_IS_TERMINAL (term));
  
  priv = kgx_tab_get_instance_private (self);

  clear_signal_handler (&priv->term_size_handler, priv->terminal);
  clear_signal_handler (&priv->term_font_inc_handler, priv->terminal);
  clear_signal_handler (&priv->term_font_dec_handler, priv->terminal);

  g_clear_object (&priv->term_title_bind);
  g_clear_object (&priv->term_path_bind);
  g_clear_object (&priv->term_font_bind);
  g_clear_object (&priv->term_zoom_bind);
  g_clear_object (&priv->term_theme_bind);
  g_clear_object (&priv->term_opaque_bind);

  g_clear_object (&priv->terminal);

  priv->terminal = g_object_ref (term);

  priv->term_size_handler = g_signal_connect (term,
                                              "size-changed",
                                              G_CALLBACK (size_changed),
                                              self);
  priv->term_font_inc_handler = g_signal_connect (term,
                                                  "increase-font-size",
                                                  G_CALLBACK (font_increase),
                                                  self);
  priv->term_font_dec_handler = g_signal_connect (term,
                                                  "decrease-font-size",
                                                  G_CALLBACK (font_decrease),
                                                  self);

  priv->term_title_bind = g_object_bind_property (term,
                                                  "window-title",
                                                  self,
                                                  "tab-title",
                                                  G_BINDING_SYNC_CREATE);
  priv->term_path_bind = g_object_bind_property (term,
                                                 "path",
                                                 self,
                                                 "tab-path",
                                                 G_BINDING_SYNC_CREATE);
  priv->term_font_bind = g_object_bind_property (self,
                                                 "font",
                                                 term,
                                                 "font-desc",
                                                 G_BINDING_SYNC_CREATE);
  priv->term_zoom_bind = g_object_bind_property (self,
                                                 "zoom",
                                                 term,
                                                 "font-scale",
                                                 G_BINDING_SYNC_CREATE);
  priv->term_theme_bind = g_object_bind_property (self,
                                                  "theme",
                                                  term,
                                                  "theme",
                                                  G_BINDING_SYNC_CREATE);
  priv->term_opaque_bind = g_object_bind_property (self,
                                                   "opaque",
                                                   term,
                                                   "opaque",
                                                   G_BINDING_SYNC_CREATE);

}


void
kgx_tab_focus_terminal (KgxTab *self)
{
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  gtk_widget_grab_focus (GTK_WIDGET (priv->terminal));
}


void
kgx_tab_search_forward (KgxTab *self)
{
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  g_return_if_fail (priv->terminal);

  vte_terminal_search_find_next (VTE_TERMINAL (priv->terminal));
}


void
kgx_tab_search_back (KgxTab *self)
{
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  g_return_if_fail (priv->terminal);

  vte_terminal_search_find_previous (VTE_TERMINAL (priv->terminal));
}

void
kgx_tab_search (KgxTab     *self,
                const char *search)
{
  KgxTabPrivate *priv;
  VteRegex *regex;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  g_return_if_fail (priv->terminal);

  regex = vte_regex_new_for_search (g_regex_escape_string (search, -1),
                                    -1, PCRE2_MULTILINE, &error);

  if (error) {
    g_warning ("Search error: %s", error->message);
    return;
  }

  vte_terminal_search_set_regex (VTE_TERMINAL (priv->terminal),
                                 regex, 0);
}


void
kgx_tab_start (KgxTab              *self,
               GAsyncReadyCallback  callback,
               gpointer             callback_data)
{
  g_return_if_fail (KGX_IS_TAB (self));
  g_return_if_fail (KGX_TAB_GET_CLASS (self)->start);

  KGX_TAB_GET_CLASS (self)->start (self, callback, callback_data);
}


GPid
kgx_tab_start_finish (KgxTab        *self,
                      GAsyncResult  *res,
                      GError       **error)
{
  g_return_val_if_fail (KGX_IS_TAB (self), 0);
  g_return_val_if_fail (KGX_TAB_GET_CLASS (self)->start, 0);

  return KGX_TAB_GET_CLASS (self)->start_finish (self, res, error);
}


void
kgx_tab_died (KgxTab         *self,
              GtkMessageType  type,
              const char     *message,
              gboolean        success)
{
  g_signal_emit (self, signals[DIED], 0, type, message, success);
}


/**
 * kgx_tab_get_pages:
 * @self: the #KgxTab
 * 
 * Find the #KgxTabs @self is (currently) a memember of
 * 
 * Returns: (transfer none): the #KgxTabs
 * 
 * Since: 0.3.0
 */
KgxPages *
kgx_tab_get_pages (KgxTab *self)
{
  GtkWidget *parent;

  parent = gtk_widget_get_ancestor (GTK_WIDGET (self), KGX_TYPE_PAGES);

  g_return_val_if_fail (parent, NULL);
  g_return_val_if_fail (KGX_IS_PAGES (parent), NULL);

  return KGX_PAGES (parent);
}


/**
 * kgx_tab_get_id:
 * @self: the #KgxTab
 * 
 * Get the unique (in the instance) id for the page
 * 
 * Returns: the identifier for the page
 */
guint
kgx_tab_get_id (KgxTab *self)
{
  KgxTabPrivate *priv;

  g_return_val_if_fail (KGX_IS_TAB (self), 0);
  
  priv = kgx_tab_get_instance_private (self);

  return priv->id;
}


static inline KgxStatus
push_type (GHashTable      *table,
           GPid             pid,
           KgxProcess      *process,
           GtkStyleContext *context,
           KgxStatus        status)
{
  g_hash_table_insert (table,
                       GINT_TO_POINTER (pid),
                       process != NULL ? g_rc_box_acquire (process) : NULL);

  g_debug ("Now %i %X", g_hash_table_size (table), status);

  return status;
}


/**
 * kgx_tab_push_child:
 * @self: the #KgxTab
 * @process: the #KgxProcess of the remote process
 * 
 * Registers @pid as a child of @self
 * 
 * Since: 0.3.0
 */
void
kgx_tab_push_child (KgxTab     *self,
                    KgxProcess *process)
{
  GtkStyleContext *context;
  GPid pid = 0;
  const char *exec = NULL;
  KgxStatus new_status = KGX_NONE;
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  pid = kgx_process_get_pid (process);
  exec = kgx_process_get_exec (process);

  if (G_UNLIKELY (g_str_has_prefix (exec, "ssh "))) {
    new_status |= push_type (priv->remote, pid, NULL, context, KGX_REMOTE);
  }

  if (G_UNLIKELY (kgx_process_get_is_root (process))) {
    new_status |= push_type (priv->root, pid, NULL, context, KGX_PRIVILEGED);
  }

  push_type (priv->children, pid, process, context, KGX_NONE);

  if (priv->status != new_status) {
    priv->status = new_status;
    g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_TAB_STATUS]);
  }
}


inline static KgxStatus
pop_type (GHashTable      *table,
          GPid             pid,
          GtkStyleContext *context,
          KgxStatus        status)
{
  guint size = 0;

  g_hash_table_remove (table, GINT_TO_POINTER (pid));

  size = g_hash_table_size (table);

  if (G_LIKELY (size <= 0)) {
    g_debug ("No longer %X", status);

    return KGX_NONE;
  } else {
    g_debug ("%i %X remaining", size, status);
    
    return status;
  }
}


/**
 * kgx_tab_pop_child:
 * @self: the #KgxTab
 * @process: the #KgxProcess of the child process
 * 
 * Remove a child added with kgx_tab_push_child()
 * 
 * Since: 0.3.0
 */
void
kgx_tab_pop_child (KgxTab     *self,
                   KgxProcess *process)
{
  GtkStyleContext *context;
  GPid pid = 0;
  KgxStatus new_status = KGX_NONE;
  KgxTabPrivate *priv;

  g_return_if_fail (KGX_IS_TAB (self));
  
  priv = kgx_tab_get_instance_private (self);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  pid = kgx_process_get_pid (process);
  
  new_status |= pop_type (priv->remote, pid, context, KGX_REMOTE);
  new_status |= pop_type (priv->root, pid, context, KGX_PRIVILEGED);
  pop_type (priv->children, pid, context, KGX_NONE);
  
  if (priv->status != new_status) {
    priv->status = new_status;
    g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_TAB_STATUS]);
  }

  if (!kgx_tab_is_active (self)) {
    g_autoptr (GNotification) noti = NULL;

    noti = g_notification_new (_("Command completed"));
    g_notification_set_body (noti, kgx_process_get_exec (process));

    g_notification_set_default_action_and_target (noti,
                                                  "app.focus-page",
                                                  "u",
                                                  priv->id);

    g_application_send_notification (G_APPLICATION (priv->application),
                                     priv->notification_id,
                                     noti);
  }
}


gboolean
kgx_tab_is_active (KgxTab *self)
{
  KgxTabPrivate *priv;

  g_return_val_if_fail (KGX_IS_TAB (self), FALSE);
  
  priv = kgx_tab_get_instance_private (self);

  return priv->is_active;
}


/**
 * kgx_tab_get_children:
 * @self: the #KgxTab
 * 
 * Get a list of child process running in @self
 * 
 * NOTE: This doesn't include the shell/root itself
 * 
 * Returns: (element-type Kgx.Process) (transfer full): the list of #KgxProcess
 */
GPtrArray *
kgx_tab_get_children (KgxTab *self)
{
  KgxTabPrivate *priv;
  GPtrArray *children;
  GHashTableIter iter;
  gpointer pid, process;

  g_return_val_if_fail (KGX_IS_TAB (self), FALSE);
  
  priv = kgx_tab_get_instance_private (self);

  children = g_ptr_array_new_full (3, (GDestroyNotify) kgx_process_unref);

  g_hash_table_iter_init (&iter, priv->children);
  while (g_hash_table_iter_next (&iter, &pid, &process)) {
    g_ptr_array_add (children, g_rc_box_acquire (process));
  }

  return children;
}