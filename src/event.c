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
#include "generator.h"

typedef struct event_callback {
  Generator *g;
  AEvent_handler_t fn;
} event_callback;

typedef struct EventQ EventQ;

struct EventQ {
  EventQ *next;
  AEvent e;
};

PRIVATE EventQ *event_q = NULL;	/* list of AEvents */
PRIVATE GList *rtfuncs = NULL;	/* list of event_callbacks */

PUBLIC SAMPLETIME gen_current_sampletime = 0;	/* current time */


PRIVATE void aevent_copy( AEvent *src, AEvent *dst ) {

    if( dst && src ) {

	*dst = *src;
	switch( src->kind ) {
	    case AE_STRING:
		dst->d.string = safe_string_dup( src->d.string );
		break;
	    case AE_NUMARRAY:
		dst->d.array.numbers = safe_malloc( sizeof(gdouble) * dst->d.array.len );
		memcpy( dst->d.array.numbers, src->d.array.numbers, src->d.array.len * sizeof(gdouble) );
		break;
	    default:
	}
    }
	
}

PRIVATE void aevent_free( AEvent *e ) {

    if( e ) {
	switch( e->kind ) {
	    case AE_STRING:
		if( e->d.string != NULL )
		    free( e->d.string );
		break;
	    case AE_NUMARRAY:
		if( e->d.array.numbers != NULL )
		    free( e->d.array.numbers );
		break;
	    default:
	}
	free( e );
    }
}

PRIVATE void eventq_free( EventQ *evq ) {

    if( evq ) {
	switch( evq->e.kind ) {
	    case AE_STRING:
		if( evq->e.d.string != NULL )
		    free( evq->e.d.string );
		break;
	    case AE_NUMARRAY:
		if( evq->e.d.array.numbers != NULL )
		    free( evq->e.d.array.numbers );
		break;
	    default:
	}
	free( evq );
    }
}

PUBLIC void gen_post_aevent(AEvent *e) {
  EventQ *q = safe_malloc(sizeof(EventQ));
  EventQ *prev = NULL, *curr = event_q;

  //q->e = *e;
  aevent_copy( e, &(q->e) );

  while (curr != NULL) {
    if (q->e.time < curr->e.time)
      break;

    prev = curr;
    curr = curr->next;
  }

  q->next = curr;

  if (prev == NULL)
    event_q = q;
  else
    prev->next = q;
}

PUBLIC void gen_purge_event_queue_refs(Generator *g) {
  EventQ *prev = NULL, *curr = event_q;

  while (curr != NULL) {
    EventQ *next = curr->next;

    if (curr->e.src == g || curr->e.dst == g) {
      if (prev == NULL)
	event_q = next;
      else
	prev->next = next;

      //free(curr);
      eventq_free( curr );
      curr = next;
      continue;
    }

    prev = curr;
    curr = next;
  }
}

PUBLIC void gen_purge_inputevent_queue_refs(Generator *g) {
  EventQ *prev = NULL, *curr = event_q;

  while (curr != NULL) {
    EventQ *next = curr->next;

    if (curr->e.dst == g) {
      if (prev == NULL)
	event_q = next;
      else
	prev->next = next;

      //free(curr);
      eventq_free( curr );
      curr = next;
      continue;
    }

    prev = curr;
    curr = next;
  }
}

PRIVATE void insert_fn(GList **lst, Generator *g, AEvent_handler_t func) {
  event_callback *ec = safe_malloc(sizeof(event_callback));

  ec->g = g;
  ec->fn = func;
  
  *lst = g_list_prepend(*lst, ec);
}

PRIVATE gint event_callback_cmp(event_callback *a, event_callback *b) {
  return !((a->g == b->g) && (a->fn == b->fn));
}

PRIVATE void remove_fn(GList **lst, Generator *g, AEvent_handler_t func) {
  event_callback ec;
  GList *link;

  ec.g = g;
  ec.fn = func;
  link = g_list_find_custom(*lst, &ec, (GCompareFunc) event_callback_cmp);

  if (link != NULL) {
    free(link->data);
    link->data = NULL;
    *lst = g_list_remove_link(*lst, link);
  }
}

PUBLIC void gen_register_realtime_fn(Generator *g, AEvent_handler_t func) {
  insert_fn(&rtfuncs, g, func);
}

PUBLIC void gen_deregister_realtime_fn(Generator *g, AEvent_handler_t func) {
  remove_fn(&rtfuncs, g, func);
}

PRIVATE void send_rt_event(event_callback *ec, AEvent *rtevent) {
  AEvent local_copy = *rtevent;
  ec->fn(ec->g, &local_copy);
}

PUBLIC void gen_send_realtime_fns(AEvent *e) {
  g_list_foreach(rtfuncs, (GFunc) send_rt_event, e);
}

PUBLIC gint gen_mainloop_once(void) {
  while (1) {
    EventQ *e = event_q;

    if (e == NULL || e->e.time > gen_get_sampletime()) {
      SAMPLETIME delta = (e == NULL) ? MAXIMUM_REALTIME_STEP : e->e.time - gen_get_sampletime();
      delta = MIN(delta, MAXIMUM_REALTIME_STEP);

      /* Notice that the clock is *not* advanced here. That's up to the master-clock
	 routine - it should call gen_advance_clock() with the result of this routine. */
      return delta;
    }

    event_q = e->next;
    e->e.dst->klass->in_handlers[e->e.dst_q](e->e.dst, &e->e);
    //free(e);
    eventq_free( e );
  }
}

PUBLIC void gen_advance_clock(gint delta) {
  gen_current_sampletime += delta;
}
