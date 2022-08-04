/* kgx-fullscreen-box.h
 *
 * Copyright 2021 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *..
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

G_BEGIN_DECLS

#define KGX_TYPE_FULLSCREEN_BOX (kgx_fullscreen_box_get_type())

G_DECLARE_FINAL_TYPE (KgxFullscreenBox, kgx_fullscreen_box, KGX, FULLSCREEN_BOX, GtkWidget)

KgxFullscreenBox *kgx_fullscreen_box_new            (void);

gboolean          kgx_fullscreen_box_get_fullscreen (KgxFullscreenBox *self);
void              kgx_fullscreen_box_set_fullscreen (KgxFullscreenBox *self,
                                                     gboolean          fullscreen);

gboolean          kgx_fullscreen_box_get_autohide   (KgxFullscreenBox *self);
void              kgx_fullscreen_box_set_autohide   (KgxFullscreenBox *self,
                                                     gboolean          autohide);

GtkWidget        *kgx_fullscreen_box_get_titlebar   (KgxFullscreenBox *self);
void              kgx_fullscreen_box_set_titlebar   (KgxFullscreenBox *self,
                                                     GtkWidget        *titlebar);

GtkWidget        *kgx_fullscreen_box_get_content    (KgxFullscreenBox *self);
void              kgx_fullscreen_box_set_content    (KgxFullscreenBox *self,
                                                     GtkWidget        *content);

G_END_DECLS
