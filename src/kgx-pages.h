/* kgx-pages.h
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

#include "kgx-tab.h"

G_BEGIN_DECLS

#define KGX_TYPE_PAGES (kgx_pages_get_type())

/**
 * KgxPagesClass:
 *
 * Since: 0.3.0
 */
struct _KgxPagesClass
{
  /*< private >*/
  AdwBinClass parent;

  /*< public >*/
};


G_DECLARE_DERIVABLE_TYPE (KgxPages, kgx_pages, KGX, PAGES, AdwBin)


void        kgx_pages_add_page       (KgxPages   *self,
                                      KgxTab     *page);
void        kgx_pages_remove_page    (KgxPages   *self,
                                      KgxTab     *page);
GPtrArray  *kgx_pages_get_children   (KgxPages   *self);
void        kgx_pages_focus_page     (KgxPages   *self,
                                      KgxTab     *page);
KgxStatus   kgx_pages_current_status (KgxPages   *self);
void        kgx_pages_set_shortcut_widget (KgxPages  *self,
                                           GtkWidget *widget);
//gboolean    kgx_pages_key_press_event     (KgxPages  *self,
//                                           GdkEvent  *event);
void        kgx_pages_close_page          (KgxPages  *self);
void        kgx_pages_detach_page         (KgxPages  *self);

G_END_DECLS
