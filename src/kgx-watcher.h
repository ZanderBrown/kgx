/* kgx-watcher.h
 *
 * Copyright 2022 Zander Brown
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

#include <glib-object.h>

#include "kgx-process.h"
#include "kgx-tab.h"

G_BEGIN_DECLS

#define KGX_TYPE_WATCHER kgx_watcher_get_type ()
G_DECLARE_FINAL_TYPE (KgxWatcher, kgx_watcher, KGX, WATCHER, GObject)


KgxWatcher           *kgx_watcher_get_default       (void);
void                  kgx_watcher_add               (KgxWatcher     *self,
                                                     GPid            pid,
                                                     KgxTab         *page);
void                  kgx_watcher_remove            (KgxWatcher     *self,
                                                     GPid            pid);
void                  kgx_watcher_push_active       (KgxWatcher     *self);
void                  kgx_watcher_pop_active        (KgxWatcher     *self);

G_END_DECLS
