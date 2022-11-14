/* kgx-tab.h
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

#pragma once

#include <gtk/gtk.h>

#include "kgx-terminal.h"
#include "kgx-process.h"
#include "kgx-enums.h"

G_BEGIN_DECLS


/**
 * KgxStatus:
 * @KGX_NONE: It's a regular #KgxTab
 * @KGX_REMOTE: The #KgxTab is connected to a "remote" session
 * @KGX_PRIVILEGED: The #KgxTab is running as someone other than the current
 *                  user
 *
 * Indicates the status of the session the #KgxTab represents
 *
 * Stability: Private
 */
typedef enum /*< flags,prefix=KGX >*/ {
  KGX_NONE = 0,              /*< nick=none >*/
  KGX_REMOTE = (1 << 0),     /*< nick=remote >*/
  KGX_PRIVILEGED = (1 << 1), /*< nick=privileged >*/
  KGX_TOOLBOX = (1 << 2),    /*< nick=toolbox >*/
} KgxStatus;


/**
 * KgxZoom:
 * @KGX_ZOOM_IN: Make text bigger
 * @KGX_ZOOM_OUT: Shrink text
 *
 * Indicates the zoom direction the zoom action was triggered for
 *
 * See #KgxPage::zoom, #KgxPages::zoom
 */
typedef enum /*< enum,prefix=KGX >*/ {
  KGX_ZOOM_IN = 0,  /*< nick=in >*/
  KGX_ZOOM_OUT = 1, /*< nick=out >*/
} KgxZoom;


#ifndef __GTK_DOC_IGNORE__
typedef struct _KgxPages KgxPages;
#endif

#define KGX_TYPE_TAB kgx_tab_get_type ()

G_DECLARE_DERIVABLE_TYPE (KgxTab, kgx_tab, KGX, TAB, GtkBox)


/**
 * KgxTabClass:
 * @start: start whatever it is that run in the tab
 * @start_finish: complete @start
 *
 * Stability: Private
 */
struct _KgxTabClass
{
  /*< private >*/
  GtkBoxClass parent;

  /*< public >*/
  void (*start)        (KgxTab               *tab,
                        GAsyncReadyCallback   callback,
                        gpointer              callback_data);
  GPid (*start_finish) (KgxTab               *tab,
                        GAsyncResult         *res,
                        GError              **error);

  void (*died)         (KgxTab               *self,
                        GtkMessageType        type,
                        const char           *message,
                        gboolean              success);
};


guint       kgx_tab_get_id           (KgxTab               *self);
void        kgx_tab_start            (KgxTab               *self,
                                      GAsyncReadyCallback   callback,
                                      gpointer              callback_data);
GPid        kgx_tab_start_finish     (KgxTab               *self,
                                      GAsyncResult         *res,
                                      GError              **error);
void        kgx_tab_died             (KgxTab               *self,
                                      GtkMessageType        type,
                                      const char           *message,
                                      gboolean              success);
KgxPages   *kgx_tab_get_pages        (KgxTab               *self);
void        kgx_tab_push_child       (KgxTab               *self,
                                      KgxProcess           *process);
void        kgx_tab_pop_child        (KgxTab               *self,
                                      KgxProcess           *process);
gboolean    kgx_tab_is_active        (KgxTab               *self);
GPtrArray  *kgx_tab_get_children     (KgxTab               *self);
void        kgx_tab_accept_drop      (KgxTab               *self,
                                      const GValue         *value);
void        kgx_tab_set_initial_title (KgxTab              *self,
                                       const char          *title,
                                       GFile               *path);

G_END_DECLS
