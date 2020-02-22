/* kgx-page.c
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
 * SECTION:kgx-page
 * @title: KgxPage
 * @short_description: Base for things in a #KgxPages
 * 
 * Since: 0.3.0
 */

#include <glib/gi18n.h>
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

#include "kgx-config.h"
#include "kgx-page.h"
#include "kgx-pages.h"
#include "kgx-pages-tab.h"
#include "kgx-terminal.h"
#include "kgx-window.h"
#include "util.h"


typedef struct _KgxPagePrivate KgxPagePrivate;
struct _KgxPagePrivate {
  char                 *title;
  char                 *description;
  GFile                *path;
  PangoFontDescription *font;
  double                zoom;
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

  KgxPagesTab          *tab;
  GBinding             *tab_title_bind;
  GBinding             *tab_description_bind;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (KgxPage, kgx_page, GTK_TYPE_BOX)


enum {
  PROP_0,
  PROP_PAGE_TITLE,
  PROP_PAGE_PATH,
  PROP_DESCRIPTION,
  PROP_FONT,
  PROP_ZOOM,
  PROP_THEME,
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
kgx_page_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  KgxPage *self = KGX_PAGE (object);
  KgxPagePrivate *priv = kgx_page_get_instance_private (self);

  switch (property_id) {
    case PROP_PAGE_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_PAGE_PATH:
      g_value_set_object (value, priv->path);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_FONT:
      g_value_set_boxed (value, priv->font);
      break;
    case PROP_ZOOM:
      g_value_set_double (value, priv->zoom);
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
kgx_page_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  KgxPage *self = KGX_PAGE (object);
  KgxPagePrivate *priv = kgx_page_get_instance_private (self);

  switch (property_id) {
    case PROP_PAGE_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_PAGE_PATH:
      g_clear_object (&priv->path);
      priv->path = g_value_dup_object (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
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
kgx_page_real_start (KgxPage             *page,
                     GAsyncReadyCallback  callback,
                     gpointer             callback_data)
{
  g_critical ("%s doesn't implement start", G_OBJECT_TYPE_NAME (page));
}


static GPid
kgx_page_real_start_finish (KgxPage             *page,
                            GAsyncResult        *res,
                            GError             **error)
{
  g_critical ("%s doesn't implement start_finish", G_OBJECT_TYPE_NAME (page));

  return 0;
}


static void
kgx_page_class_init (KgxPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS   (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  KgxPageClass   *page_class   = KGX_PAGE_CLASS   (klass);
  
  object_class->get_property = kgx_page_get_property;
  object_class->set_property = kgx_page_set_property;

  page_class->start = kgx_page_real_start;
  page_class->start_finish = kgx_page_real_start_finish;

  /**
   * KgxPage:page-title:
   * 
   * Title of this page
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_PAGE_TITLE] =
    g_param_spec_string ("page-title", "Page Title", "Title for this page",
                         NULL,
                         G_PARAM_READWRITE);

  /**
   * KgxPage:page-path:
   * 
   * Current path of this page
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_PAGE_PATH] =
    g_param_spec_object ("page-path", "Page Path", "Current path",
                         G_TYPE_FILE,
                         G_PARAM_READWRITE);

  pspecs[PROP_DESCRIPTION] =
    g_param_spec_string ("description", "Description", "Description of the page",
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
   * KgxPage:theme:
   * 
   * The #KgxTheme to apply to the #KgxTerminal s in the #KgxPage
   * 
   * Stability: Private
   * 
   * Since: 0.3.0
   */
  pspecs[PROP_THEME] =
    g_param_spec_enum ("theme", "Theme", "The path of the active page",
                       KGX_TYPE_THEME,
                       KGX_THEME_NIGHT,
                       G_PARAM_READWRITE);

  /**
   * KgxPage:opaque:
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
                          "Should the page close when dead",
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
                                               RES_PATH "kgx-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, KgxPage, revealer);
  gtk_widget_class_bind_template_child_private (widget_class, KgxPage, label);
}


static void
parent_set (KgxPage   *self,
            GtkWidget *old_parent)
{
  KgxPagePrivate *priv;
  GtkWidget *parent;
  KgxPages *pages;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

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
died (KgxPage        *self,
      GtkMessageType  type,
      const char     *message,
      gboolean        success)
{
  KgxPagePrivate *priv;
  GtkStyleContext *context;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

  gtk_label_set_markup (GTK_LABEL (priv->label), message);

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->revealer));

  if (type == GTK_MESSAGE_ERROR) {
    gtk_style_context_add_class (context, "error");
  } else {
    gtk_style_context_remove_class (context, "error");
  }

  gtk_revealer_set_reveal_child (GTK_REVEALER (priv->revealer), TRUE);

  if (priv->close_on_quit && success) {
    kgx_pages_remove_page (kgx_page_get_pages (self), self);
  }
}


static void
kgx_page_init (KgxPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self, "parent-set", G_CALLBACK (parent_set), NULL);
  g_signal_connect (self, "died", G_CALLBACK (died), NULL);
}


void
kgx_page_connect_tab (KgxPage     *self,
                      KgxPagesTab *tab)
{
  KgxPagePrivate *priv;

  g_return_if_fail (KGX_IS_PAGE (self));
  g_return_if_fail (KGX_IS_PAGES_TAB (tab));
  
  priv = kgx_page_get_instance_private (self);

  g_clear_object (&priv->tab);
  priv->tab = g_object_ref (tab);

  g_clear_object (&priv->tab_title_bind);
  priv->tab_title_bind = g_object_bind_property (self, "page-title",
                                                 tab, "title",
                                                 G_BINDING_SYNC_CREATE);

  g_clear_object (&priv->tab_description_bind);
  priv->tab_description_bind = g_object_bind_property (self, "description",
                                                       tab, "description",
                                                       G_BINDING_SYNC_CREATE);
}


static void
size_changed (KgxTerminal *term,
              guint        rows,
              guint        cols,
              KgxPage     *self)
{
  g_signal_emit (self, signals[SIZE_CHANGED], 0, rows, cols);
}


static void
font_increase (KgxTerminal *term,
               KgxPage     *self)
{
  g_signal_emit (self, signals[ZOOM], 0, KGX_ZOOM_IN);
}


static void
font_decrease (KgxTerminal *term,
               KgxPage     *self)
{
  g_signal_emit (self, signals[ZOOM], 0, KGX_ZOOM_OUT);
}


void
kgx_page_connect_terminal (KgxPage     *self,
                           KgxTerminal *term)
{
  KgxPagePrivate *priv;

  g_return_if_fail (KGX_IS_PAGE (self));
  g_return_if_fail (KGX_IS_TERMINAL (term));
  
  priv = kgx_page_get_instance_private (self);

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
                                                  "page-title",
                                                  G_BINDING_SYNC_CREATE);
  priv->term_path_bind = g_object_bind_property (term,
                                                 "path",
                                                 self,
                                                 "page-path",
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
kgx_page_focus_terminal (KgxPage *self)
{
  KgxPagePrivate *priv;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

  gtk_widget_grab_focus (GTK_WIDGET (priv->terminal));
}


void
kgx_page_search_forward (KgxPage *self)
{
  KgxPagePrivate *priv;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

  g_return_if_fail (priv->terminal);

  vte_terminal_search_find_next (VTE_TERMINAL (priv->terminal));
}


void
kgx_page_search_back (KgxPage *self)
{
  KgxPagePrivate *priv;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

  g_return_if_fail (priv->terminal);

  vte_terminal_search_find_previous (VTE_TERMINAL (priv->terminal));
}

void
kgx_page_search (KgxPage    *self,
                 const char *search)
{
  KgxPagePrivate *priv;
  VteRegex *regex;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (KGX_IS_PAGE (self));
  
  priv = kgx_page_get_instance_private (self);

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
kgx_page_start (KgxPage             *self,
                GAsyncReadyCallback  callback,
                gpointer             callback_data)
{
  g_return_if_fail (KGX_IS_PAGE (self));
  g_return_if_fail (KGX_PAGE_GET_CLASS (self)->start);

  KGX_PAGE_GET_CLASS (self)->start (self, callback, callback_data);
}


GPid
kgx_page_start_finish (KgxPage             *self,
                       GAsyncResult        *res,
                       GError             **error)
{
  g_return_val_if_fail (KGX_IS_PAGE (self), 0);
  g_return_val_if_fail (KGX_PAGE_GET_CLASS (self)->start, 0);

  return KGX_PAGE_GET_CLASS (self)->start_finish (self, res, error);
}


void
kgx_page_died (KgxPage        *self,
               GtkMessageType  type,
               const char     *message,
               gboolean        success)
{
  g_signal_emit (self, signals[DIED], 0, type, message, success);
}


/**
 * kgx_page_get_pages:
 * @self: the #KgxPage
 * 
 * Find the #KgxPages @self is (currently) a memember of
 * 
 * Returns: (transfer none): the #KgxPages
 * 
 * Since: 0.3.0
 */
KgxPages *
kgx_page_get_pages (KgxPage *self)
{
  GtkWidget *parent;

  parent = gtk_widget_get_ancestor (GTK_WIDGET (self), KGX_TYPE_PAGES);

  g_return_val_if_fail (parent, NULL);
  g_return_val_if_fail (KGX_IS_PAGES (parent), NULL);

  return KGX_PAGES (parent);
}