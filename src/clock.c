/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "global.h"
//#include "event.h"
#include "generator.h"

PRIVATE GList *clocks = NULL;
PRIVATE AClock *selected = NULL;
PRIVATE GList *listeners = NULL;

PRIVATE AClock *default_clock = NULL;

typedef struct ClockListener {
  GHookFunc hook;
  gpointer data;
} ClockListener;

PUBLIC void gen_register_clock_listener(GHookFunc hook, gpointer data) {
  ClockListener *l = safe_malloc(sizeof(ClockListener));

  l->hook = hook;
  l->data = data;
  listeners = g_list_prepend(listeners, l);
}

PRIVATE gint listener_cmp(ClockListener *a, ClockListener *b) {
  return !((a->hook == b->hook) && (a->data == b->data));
}

PUBLIC void gen_deregister_clock_listener(GHookFunc hook, gpointer data) {
  ClockListener l;
  GList *link;

  l.hook = hook;
  l.data = data;
  link = g_list_find_custom(listeners, &l, (GCompareFunc) listener_cmp);
  
  if (link != NULL) {
    free(link->data);
    link->data = NULL;
    listeners = g_list_remove_link(listeners, link);
  }
}

PRIVATE void notify_clock_listeners(ClockListener *l, gpointer user_data) {
  l->hook(l->data);
}

PUBLIC AClock *gen_register_clock(Generator *gen, char *name, AClock_handler_t handler) {
  AClock *clock = safe_malloc(sizeof(AClock));

  clock->gen = gen;
  clock->name = safe_string_dup(name);
  clock->handler = handler;
  clock->selected = FALSE;

  clocks = g_list_append(clocks, clock);
  g_list_foreach(listeners, (GFunc) notify_clock_listeners, NULL);

  return clock;
}

PUBLIC void gen_deregister_clock(AClock *clock) {
  if (clock->selected)
    gen_select_clock(default_clock);
  clocks = g_list_remove(clocks, clock);
  g_list_foreach(listeners, (GFunc) notify_clock_listeners, NULL);
  free(clock->name);
  free(clock);
}

PUBLIC void gen_set_default_clock(AClock *clock) {
  default_clock = clock;
}

PUBLIC AClock **gen_enumerate_clocks(void) {
  int len = g_list_length(clocks);
  AClock **array = safe_malloc((len + 1) * sizeof(AClock *));
  GList *curr = clocks;
  int i;

  for (i = 0; i < len; i++, curr = g_list_next(curr))
    array[i] = curr->data;

  array[len] = NULL;
  return array;
}

PUBLIC void gen_select_clock(AClock *clock) {
  if (selected == clock)
    return;

  if (selected != NULL) {
    selected->handler(selected, CLOCK_DISABLE);
    selected->selected = FALSE;
  }

  selected = clock;

  if (selected != NULL) {
    selected->selected = TRUE;
    selected->handler(selected, CLOCK_ENABLE);
  }
}

PUBLIC void gen_stop_clock(void) {
  gen_select_clock(NULL);
}

PUBLIC void gen_clock_mainloop_have_remaining( gint remaining ) {
  gint delta;

  while (remaining > 0) {
    AEvent e;

    delta = MIN(MIN( MAXIMUM_REALTIME_STEP, remaining ), gen_mainloop_once());
    remaining -= delta;

    e.kind = AE_REALTIME;
    e.d.integer = delta;
    gen_send_realtime_fns(&e);

    gen_advance_clock(delta);
  }
}

PUBLIC void gen_clock_mainloop(void) {
  gen_clock_mainloop_have_remaining( MAXIMUM_REALTIME_STEP );
}

PUBLIC void init_clock(void) {
}

PUBLIC void done_clock(void) {
}
