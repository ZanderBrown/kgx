/* kgx-watcher.c
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

#include "kgx-config.h"

#include "kgx-watcher.h"


/**
 * ProcessWatch:
 * @page: the #KgxTab the #KgxProcess is in
 * @process: what we are watching
 *
 * Stability: Private
 */
struct ProcessWatch {
  KgxTab /*weak*/ *page;
  KgxProcess *process;
};


/**
 * KgxWatcher:
 * @watching: (element-type GLib.Pid ProcessWatch) the shells running in windows
 * @children: (element-type GLib.Pid ProcessWatch) the processes running in shells
 * @active: counter of #KgxWindow's with #GtkWindow:is-active = %TRUE,
 *          obviously this should only ever be 1 or but we can't be certain
 * @timeout: the current #GSource id of the watcher
 * 
 * Used to monitor processes running in pages
 */
struct _KgxWatcher {
  GObject                   parent_instance;

  GTree                    *watching;
  GTree                    *children;

  guint                     timeout;
  int                       active;
};


G_DEFINE_TYPE (KgxWatcher, kgx_watcher, G_TYPE_OBJECT)


static void
kgx_watcher_dispose (GObject *object)
{
  KgxWatcher *self = KGX_WATCHER (object);

  g_clear_pointer (&self->watching, g_tree_unref);
  g_clear_pointer (&self->children, g_tree_unref);

  G_OBJECT_CLASS (kgx_watcher_parent_class)->dispose (object);
}


static void
kgx_watcher_class_init (KgxWatcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = kgx_watcher_dispose;
}


static void
clear_watch (struct ProcessWatch *watch)
{
  g_return_if_fail (watch != NULL);

  g_clear_pointer (&watch->process, kgx_process_unref);
  g_clear_weak_pointer (&watch->page);

  g_clear_pointer (&watch, g_free);
}


static gboolean
handle_watch_iter (gpointer pid,
                   gpointer val,
                   gpointer user_data)
{
  KgxProcess *process = val;
  KgxWatcher *self = user_data;
  GPid parent = kgx_process_get_parent (process);
  struct ProcessWatch *watch = NULL;

  watch = g_tree_lookup (self->watching, GINT_TO_POINTER (parent));

  // There are far more processes on the system than there are children
  // of watches, thus lookup are unlikly
  if (G_UNLIKELY (watch != NULL)) {

    /* If the page died we stop caring about its processes */
    if (G_UNLIKELY (watch->page == NULL)) {
      g_tree_remove (self->watching, GINT_TO_POINTER (parent));
      g_tree_remove (self->children, pid);

      return FALSE;
    }

    if (!g_tree_lookup (self->children, pid)) {
      struct ProcessWatch *child_watch = g_new0 (struct ProcessWatch, 1);

      child_watch->process = g_rc_box_acquire (process);
      g_set_weak_pointer (&child_watch->page, watch->page);

      g_debug ("Hello %i!", GPOINTER_TO_INT (pid));

      g_tree_insert (self->children, pid, child_watch);
    }

    kgx_tab_push_child (watch->page, process);
  }

  return FALSE;
}


struct RemoveDead {
  GTree     *plist;
  GPtrArray *dead;
};


static gboolean
remove_dead (gpointer pid,
             gpointer val,
             gpointer user_data)
{
  struct RemoveDead *data = user_data;
  struct ProcessWatch *watch = val;

  if (!g_tree_lookup (data->plist, pid)) {
    g_debug ("%i marked as dead", GPOINTER_TO_INT (pid));

    kgx_tab_pop_child (watch->page, watch->process);

    g_ptr_array_add (data->dead, pid);
  }

  return FALSE;
}


static gboolean
watch (gpointer data)
{
  KgxWatcher *self = KGX_WATCHER (data);
  g_autoptr (GTree) plist = NULL;
  struct RemoveDead dead;

  plist = kgx_process_get_list ();

  g_tree_foreach (plist, handle_watch_iter, self);

  dead.plist = plist;
  dead.dead = g_ptr_array_new_full (1, NULL);

  g_tree_foreach (self->children, remove_dead, &dead);

  // We can't modify self->chilren whilst walking it
  for (int i = 0; i < dead.dead->len; i++) {
    g_tree_remove (self->children, g_ptr_array_index (dead.dead, i));
  }

  g_ptr_array_unref (dead.dead);

  return G_SOURCE_CONTINUE;
}


static inline void
set_watcher (KgxWatcher *self, gboolean focused)
{
  g_debug ("updated watcher focused? %s", focused ? "yes" : "no");

  if (self->timeout != 0) {
    g_source_remove (self->timeout);
  }

  // Slow down polling when nothing is focused
  self->timeout = g_timeout_add (focused ? 500 : 2000, watch, self);
  g_source_set_name_by_id (self->timeout, "[kgx] child watcher");
}


static void
kgx_watcher_init (KgxWatcher *self)
{
  self->watching = g_tree_new_full (kgx_pid_cmp,
                                    NULL,
                                    NULL,
                                    (GDestroyNotify) clear_watch);
  self->children = g_tree_new_full (kgx_pid_cmp,
                                    NULL,
                                    NULL,
                                    (GDestroyNotify) clear_watch);

  self->active = 0;
  self->timeout = 0;

  set_watcher (self, TRUE);
}


/**
 * kgx_watcher_get_default:
 * 
 * Returns: (transfer none): the #KgxWatcher singleton
 */
KgxWatcher *
kgx_watcher_get_default (void)
{ 
  static KgxWatcher *instance;

  if (instance == NULL) {
    instance = g_object_new (KGX_TYPE_WATCHER, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }

  return instance;
}


/**
 * kgx_watcher_add:
 * @self: the #KgxWatcher
 * @pid: the shell process to watch
 * @page: the #KgxTab the shell is running in
 *
 * Registers a new shell process with the pid watcher
 */
void
kgx_watcher_add (KgxWatcher *self,
                 GPid        pid,
                 KgxTab     *page)
{
  struct ProcessWatch *watch;

  g_return_if_fail (KGX_IS_WATCHER (self));
  g_return_if_fail (KGX_IS_TAB (page));

  watch = g_new0 (struct ProcessWatch, 1);
  watch->process = kgx_process_new (pid);
  g_set_weak_pointer (&watch->page, page);

  g_debug ("Started watching %i", pid);

  g_tree_insert (self->watching, GINT_TO_POINTER (pid), watch);
}


/**
 * kgx_watcher_remove:
 * @self: the #KgxWatcher
 * @pid: the shell process to stop watch watching
 *
 * unregisters the shell with #GPid pid
 */
void
kgx_watcher_remove (KgxWatcher *self,
                    GPid        pid)
{
  g_return_if_fail (KGX_IS_WATCHER (self));

  if (G_LIKELY (g_tree_lookup (self->watching, GINT_TO_POINTER (pid)))) {
    g_tree_remove (self->watching, GINT_TO_POINTER (pid));
    g_debug ("Stopped watching %i", pid);
  } else {
    g_warning ("Unknown process %i", pid);
  }
}


/**
 * kgx_watcher_push_active:
 * @self: the #KgxWatcher
 *
 * Increase the active window count
 */
void
kgx_watcher_push_active (KgxWatcher *self)
{
  g_return_if_fail (KGX_IS_WATCHER (self));

  self->active++;

  g_debug ("push_active");

  if (G_LIKELY (self->active > 0)) {
    set_watcher (self, TRUE);
  } else {
    set_watcher (self, FALSE);
  }
}


/**
 * kgx_watcher_pop_active:
 * @self: the #KgxWatcher
 *
 * Decrease the active window count
 */
void
kgx_watcher_pop_active (KgxWatcher *self)
{
  g_return_if_fail (KGX_IS_WATCHER (self));

  self->active--;

  g_debug ("pop_active");

  if (G_LIKELY (self->active < 1)) {
    set_watcher (self, FALSE);
  } else {
    set_watcher (self, TRUE);
  }
}

