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

PRIVATE GAsyncQueue *event_queue;
PRIVATE GAsyncQueue *addrt_queue;

PUBLIC SAMPLETIME gen_current_sampletime = 0;	/* current time */


PRIVATE void aevent_copy( AEvent *src, AEvent *dst ) {

    if( dst && src ) {

	*dst = *src;
	switch( src->kind ) {
	    case AE_STRING:
		dst->d.string = safe_string_dup( src->d.string );
		break;
	    case AE_NUMARRAY:
		dst->d.array.numbers = safe_malloc( sizeof(SAMPLE) * dst->d.array.len );
		memcpy( dst->d.array.numbers, src->d.array.numbers, src->d.array.len * sizeof(SAMPLE) );
		break;
	    case AE_DBLARRAY:
		dst->d.darray.numbers = safe_malloc( sizeof(gdouble) * dst->d.darray.len );
		memcpy( dst->d.darray.numbers, src->d.darray.numbers, src->d.darray.len * sizeof(gdouble) );
		break;
	    default:
	    	break;
	}
    }
	
}

//PRIVATE void aevent_free( AEvent *e ) {
//
//    if( e ) {
//	switch( e->kind ) {
//	    case AE_STRING:
//		if( e->d.string != NULL )
//		    safe_free( e->d.string );
//		break;
//	    case AE_NUMARRAY:
//		if( e->d.array.numbers != NULL )
//		    safe_free( e->d.array.numbers );
//		break;
//	    default:
//	    	break;
//	}
//	safe_free( e );
//    }
//}

PRIVATE void eventq_free( EventQ *evq ) {

    if( evq ) {
	switch( evq->e.kind ) {
	    case AE_STRING:
		if( evq->e.d.string != NULL )
		    safe_free( evq->e.d.string );
		break;
	    case AE_NUMARRAY:
		if( evq->e.d.array.numbers != NULL )
		    safe_free( evq->e.d.array.numbers );
		break;
	    case AE_DBLARRAY:
		if( evq->e.d.darray.numbers != NULL )
		    safe_free( evq->e.d.darray.numbers );
		break;
	    default:
	    	break;
	}
	safe_free( evq );
    }
}

/**
 * \brief Put an AEvent into the main event queue.
 *
 * \param e The Event which shall be copied into the EventQueue
 *
 * this would use an async queue to make this operation thread safe.
 * the main event processor will pop all events from the queue and
 * sort them into the main list.
 *
 * with this function threadsafe control_emit() gets threadsafe as well.
 *
 * i believe this is the problem XXX XXX
 */

PUBLIC void gen_post_aevent(AEvent *e) {
  EventQ *q = safe_malloc(sizeof(EventQ));

  aevent_copy( e, &(q->e) );

  g_async_queue_push( event_queue, q );
}

/**
 * \brief sort in all posted aevents
 *
 * XXX: ok... this has to be modified, to support for
 *      local event queues on the generators...
 *
 *      if( g->has_local_queue )
 *         deliver_directly( e );
 *      else
 *         deliver normally( e );
 *
 *      this seems thread safe... lets see if that
 *      brings us some advantage...
 */

PRIVATE void gen_sortin_aevents(void) {
  EventQ *q;

  while( (q=g_async_queue_try_pop( event_queue )) != NULL )
  {
      EventQ *prev = NULL, *curr = event_q;

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
}
/**
 * \brief Remove all events for Generator \a g from the global queue
 *
 * \param g The Generator
 *
 * To make this function threadsafe i need to add a command queue.
 * The mainloop will execute all commands in the queue when its time.
 *
 * the event insertion queue should be the same as the command queue.
 * i also need commands for inserting and removing Eventlinks.
 * 
 * also a command for removing generators is needed because
 * i dont know when the Removal of an EventLink occurs.
 *
 * At the Moment this function may only be called from the audiothread.
 * This is ok for all current plugins.
 *
 */

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

/**
 * \brief insert a function into \a list
 *
 * \param list pointer to a GList *
 * \param g The Generator which will be a paramter of \a func
 *
 * If this was threadsafe gen_register_realtime_fn() would be threadsafe
 * also.
 * 
 * i need a Mutex for the rtfuncs struct.
 */

//PRIVATE void insert_fn(GList **lst, Generator *g, AEvent_handler_t func) {
//  event_callback *ec = safe_malloc(sizeof(event_callback));
//
//  ec->g = g;
//  ec->fn = func;
//  
//  *lst = g_list_prepend(*lst, ec);
//}

/**
 * \brief insert an event_callback into \a list
 *
 * \param list pointer to a GList *
 * \param ec pointer to the eventcallback.
 *
 * If this was threadsafe gen_register_realtime_fn() would be threadsafe
 * also.
 * 
 * i need a Mutex for the rtfuncs struct.
 *
 * no ... make it a GSList.
 */

PRIVATE void insert_fn_ec(GList **lst, event_callback *ec) {

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

/**
 * \brief Register a realtime function
 *
 * \param g The Genrator wishing to receive Realtime Events
 * \param func The function which should be called on receive of a Realtime Event
 *
 * The Realtime functions are the entry points into the graph.
 *
 * this function is sometimes (scope, outputs) called by the init_instance()
 * of a Generator to have it realtime_handler installed.
 *
 * This function must be threadsafe.
 * an async queue is a good way to accomplish this.
 *
 * should the Destruction of an instance be threadsafe also ?
 * it must be,,,
 * 
 */

PUBLIC void gen_register_realtime_fn(Generator *g, AEvent_handler_t func) {
    event_callback *ec = safe_malloc( sizeof(event_callback) );
    ec->g = g;
    ec->fn = func;

    g_async_queue_push( addrt_queue, ec );

  //insert_fn(&rtfuncs, g, func);
}

/**
 * \brief deregister a realtime function
 *
 * \param g The Generator
 * \param func The callback that should be removed
 */

PUBLIC void gen_deregister_realtime_fn(Generator *g, AEvent_handler_t func) {
  remove_fn(&rtfuncs, g, func);
}

PRIVATE void sortin_rtevents(void) {
  event_callback *ec;

  while( (ec=g_async_queue_try_pop( addrt_queue )) != NULL )
      insert_fn_ec( &rtfuncs, ec );
}

PRIVATE void send_rt_event(event_callback *ec, AEvent *rtevent) {
    // why a local copy ?
    // warning this only works for REALTIME events.
    AEvent local_copy = *rtevent;
    ec->fn(ec->g, &local_copy);
}

/**
 * \brief Call all Realtime functions which have registered themselves
 *        with gen_register_realtime_fn()
 *
 * \param e an AEvent of type AE_REALTIME
 *
 * this is called when the eventprocessing is done
 * and the data must be computed.
 *
 * needs to insert realtime functions from the queue
 * deletion of functions will be done from the audio thread so
 * it does not need to be safe.
 */

PUBLIC void gen_send_realtime_fns(AEvent *e) {
    sortin_rtevents();

    g_list_foreach(rtfuncs, (GFunc) send_rt_event, e);
}

/**
 * \brief process AEvents pending
 *
 * \return time until next event has to be processed
 *         (At Maximum MAXIMUM_REALTIME_STEP)
 *
 * This Functions inserts all new aevents and then processes all
 * pending events from the queue.
 * it also calls do_checks to make the changes to the connection graph.
 */

PUBLIC gint gen_mainloop_once(void) {

  gen_sortin_aevents();
  gen_mainloop_do_checks();

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
    gen_sortin_aevents();
  }
}

PUBLIC void gen_advance_clock(gint delta) {
  gen_current_sampletime += delta;
}

PUBLIC void init_event( void ) {
    if (!g_thread_supported ()) g_thread_init (NULL);
    event_queue = g_async_queue_new();
    addrt_queue = g_async_queue_new();
}
