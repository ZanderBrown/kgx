/* kgx-application.h
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

#include "kgx-process.h"
#include "kgx-window.h"
#include "kgx-terminal.h"
#include "kgx-page.h"

G_BEGIN_DECLS

/**
 * DESKTOP_INTERFACE_SETTINGS_SCHEMA:
 * The schema that defines the system fonts
 */
#define DESKTOP_INTERFACE_SETTINGS_SCHEMA "org.gnome.desktop.interface"

/**
 * MONOSPACE_FONT_KEY_NAME:
 * The name of the key in %DESKTOP_INTERFACE_SETTINGS_SCHEMA for the monospace
 * font
 */
#define MONOSPACE_FONT_KEY_NAME "monospace-font-name"

#define KGX_TYPE_APPLICATION (kgx_application_get_type())

/**
 * ProcessWatch:
 * @page: the #KgxPage the #KgxProcess is in
 * @process: what we are watching
 * 
 * Stability: Private
 */
struct ProcessWatch {
  KgxPage    *page;
  KgxProcess *process;
};

/**
 * KgxApplication:
 * @theme: the colour palette in use
 * @scale: the font scaling used
 * @desktop_interface: the #GSettings storing the system monospace font
 * @watching: ~ (element-type GLib.Pid ProcessWatch) the shells running in windows
 * @children: ~ (element-type GLib.Pid ProcessWatch) the processes running in shells
 * @pages: ~ (element-type uint Kgx.Page) the global page id / page map
 * @active: counter of #KgxWindow's with #GtkWindow:is-active = %TRUE,
 *          obviously this should only ever be 1 or but we can't be certain
 * @timeout: the current #GSource id of the watcher
 * 
 * Stability: Private
 */
struct _KgxApplication
{
  /*< private >*/
  GtkApplication            parent_instance;

  /*< public >*/
  KgxTheme                  theme;
  gdouble                   scale;

  
  GSettings                *desktop_interface;

  GTree                    *watching;
  GTree                    *children;
  GTree                    *pages;

  guint                     timeout;
  int                       active;
};

G_DECLARE_FINAL_TYPE (KgxApplication, kgx_application, KGX, APPLICATION, GtkApplication)


void                  kgx_application_add_watch       (KgxApplication *self,
                                                       GPid            pid,
                                                       KgxPage        *page);
void                  kgx_application_remove_watch    (KgxApplication *self,
                                                       GPid            pid);
PangoFontDescription *kgx_application_get_font        (KgxApplication *self);
void                  kgx_application_push_active     (KgxApplication *self);
void                  kgx_application_pop_active      (KgxApplication *self);
void                  kgx_application_add_page        (KgxApplication *self,
                                                       KgxPage        *page);
KgxPage              *kgx_application_lookup_page     (KgxApplication *self,
                                                       guint           id);

G_END_DECLS
