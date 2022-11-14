/* kgx-window.h
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

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "kgx-settings.h"
#include "kgx-pages.h"

G_BEGIN_DECLS

#define KGX_WINDOW_STYLE_ROOT "root"
#define KGX_WINDOW_STYLE_REMOTE "remote"
#define KGX_WINDOW_STYLE_TOOLBOX "toolbox"


#define KGX_TYPE_WINDOW (kgx_window_get_type())

/**
 * KgxWindow:
 * @last_cols: the column width last time we received #GtkWidget::size-allocate
 * @last_rows: the row count last time we received #GtkWidget::size-allocate
 * @timeout: the id of the #GSource used to hide the statusbar
 * @close_anyway: ignore running children and close without prompt
 * @header_bar: the #GtkHeaderBar that the styles are applied to
 * @search_entry: the #GtkSearchEntry inside @search_bar
 * @search_bar: the windows #GtkSearchBar
 * @exit_info: the #GtkRevealer hat wraps @exit_message
 * @exit_message: the #GtkLabel for showing important messages
 * @zoom_level: the #GtkLabel in the #GtkPopover showing the current zoom level
 * @pages: the #KgxPages of #KgxPage current in the window
 */
struct _KgxWindow
{
  /*< private >*/
  AdwApplicationWindow  parent_instance;

  /*< public >*/
  KgxSettings          *settings;
  GBindingGroup        *settings_binds;

  /* Size indicator */
  int                   last_cols;
  int                   last_rows;
  guint                 timeout;

  gboolean              close_anyway;

  /* Template widgets */
  GtkWidget            *window_title;
  GtkWidget            *exit_info;
  GtkWidget            *exit_message;
  GtkWidget            *theme_switcher;
  GtkWidget            *zoom_level;
  GtkWidget            *tab_bar;
  GtkWidget            *tab_button;
  GtkWidget            *tab_switcher;
  GtkWidget            *pages;
  GMenu                *primary_menu;

  GActionMap           *tab_actions;
};

G_DECLARE_FINAL_TYPE (KgxWindow, kgx_window, KGX, WINDOW, AdwApplicationWindow)

GFile      *kgx_window_get_working_dir (KgxWindow    *self);
void        kgx_window_show_status     (KgxWindow    *self,
                                        const char   *status);
KgxPages   *kgx_window_get_pages       (KgxWindow    *self);

G_END_DECLS
