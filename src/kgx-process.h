/* kgx-process.h
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

#include <glib.h>
#include <glib-object.h>

#include "kgx-config.h"

G_BEGIN_DECLS

typedef struct _KgxProcess KgxProcess;

#define KGX_TYPE_PROCESS (kgx_process_get_type ())

GTree      *kgx_process_get_list    (void);
KgxProcess *kgx_process_new         (GPid        pid);
GPid        kgx_process_get_pid     (KgxProcess *self);
gint32      kgx_process_get_uid     (KgxProcess *self);
gboolean    kgx_process_get_is_root (KgxProcess *self);
GPid        kgx_process_get_parent  (KgxProcess *self);
const char *kgx_process_get_exec    (KgxProcess *self);
GType       kgx_process_get_type    (void);
void        kgx_process_unref       (KgxProcess *self);

int         kgx_pid_cmp             (gconstpointer a,
                                     gconstpointer b,
                                     gpointer      data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (KgxProcess, kgx_process_unref)

G_END_DECLS
