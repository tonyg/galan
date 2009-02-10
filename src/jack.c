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

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "control.h"
#include "msgbox.h"

#include "galan_jack.h"
#include "galan_lash.h"

#include <jack/jack.h>
#ifdef HAVE_JACKMIDI_H
#include <jack/midiport.h>
#endif


PRIVATE jack_client_t *jack_client = NULL;

PRIVATE jack_port_t *midi_control_port = NULL;

PRIVATE AClock *jack_clock = NULL;

PRIVATE GList *transport_clocks = NULL;
PRIVATE GList *jack_process_callbacks = NULL;

PRIVATE SAMPLETIME jack_timestamp = 0;

PRIVATE Control *midi_map[128];
PRIVATE Control *midilearn_target = NULL;
PRIVATE int      midilearn_CC = 0;

// Registering Process Callbacks (used for midi ports)

typedef struct jack_process_callback_t {
  Generator *g;
  jack_process_handler_t handler;
} jack_process_callback_t;

PUBLIC void galan_jack_register_process_handler( Generator *g, jack_process_handler_t handler ) {
    jack_process_callback_t *new_cb = safe_malloc( sizeof( jack_process_callback_t ) );
    new_cb->g = g;
    new_cb->handler = handler;
    jack_process_callbacks = g_list_append( jack_process_callbacks, new_cb );
}

PRIVATE gint jack_process_callback_cmp(jack_process_callback_t *a, jack_process_callback_t *b) {
  return !((a->g == b->g) && (a->handler == b->handler));
}

PUBLIC void galan_jack_deregister_process_handler(Generator *g, jack_process_handler_t func) {
  jack_process_callback_t jpc;
  GList *link;

  jpc.g = g;
  jpc.handler = func;
  link = g_list_find_custom(jack_process_callbacks, &jpc, (GCompareFunc) jack_process_callback_cmp );

  if (link != NULL) {
    free(link->data);
    link->data = NULL;
    jack_process_callbacks = g_list_remove_link(jack_process_callbacks, link);
  }
}


// Registering of transport callbacks.

typedef struct transport_frame_event_t {
  Generator *g;
  transport_frame_event_handler_t handler;
} transport_frame_event_t;

PRIVATE gint transport_event_handler_cmp(transport_frame_event_t *a, transport_frame_event_t *b) {
  return !((a->g == b->g) && (a->handler == b->handler));
}

PUBLIC void galan_jack_register_transport_clock( Generator *g, transport_frame_event_handler_t handler ) {
    transport_frame_event_t *new_cb = safe_malloc( sizeof( transport_frame_event_t ) );
    new_cb->g = g;
    new_cb->handler = handler;
    transport_clocks = g_list_append( transport_clocks, new_cb );
}

PUBLIC void galan_jack_deregister_transport_clock( Generator *g, transport_frame_event_handler_t handler ) {

  transport_frame_event_t jpc;
  GList *link;

  jpc.g = g;
  jpc.handler = handler;
  link = g_list_find_custom(transport_clocks, &jpc, (GCompareFunc) transport_event_handler_cmp );

  if (link != NULL) {
    free(link->data);
    link->data = NULL;
    transport_clocks = g_list_remove_link( transport_clocks, link );
  }
}


// Midi Learn

PUBLIC void midilearn_set_target_control( Control *c ) {
    midilearn_target = c;
}

PUBLIC void midilearn_remove_control( Control *c ) {
    int i;
    for( i=0; i<128; i++ ) {
	if( midi_map[i] == c ) {
	    midi_map[i] = NULL;
	    break;
	}
    }
}

PUBLIC int midilearn_check_result( void ) {
    if( midilearn_target != NULL )
	return -1;

    return midilearn_CC;
}
	

// Helpers.

PUBLIC SAMPLETIME galan_jack_get_timestamp( void ) {
    return jack_timestamp;
}

PUBLIC jack_client_t *galan_jack_get_client(void) {
    return jack_client;
}


PRIVATE void process_midi_control_port( jack_nframes_t nframes ) {

	int i;
	void *port_buffer = jack_port_get_buffer( midi_control_port, nframes );
	jack_nframes_t num_jackevents = jack_midi_get_event_count( port_buffer );
	jack_midi_event_t jackevent;

	for( i=0; i<num_jackevents; i++ ) {
		if( jack_midi_event_get( &jackevent, port_buffer, i ) != 0 )
			break;

		if( (jackevent.buffer[0] & 0xf0) == 0xb0 && midilearn_target ) {
			midilearn_CC = jackevent.buffer[1];
			midi_map[midilearn_CC] = midilearn_target;
			midilearn_target = NULL;
		} else if( (jackevent.buffer[0] & 0xf0) == 0xb0 && midi_map[jackevent.buffer[1]] != NULL ) {
			// midi mapped.
			Control *c = midi_map[jackevent.buffer[1]];
			if( c != NULL ) {
			    // XXX: need to lock control here.
			    // and send out the data.
			    gdouble rng = c->max - c->min;
			    gdouble cc  = jackevent.buffer[2];
			    gdouble val = c->min + rng * cc/127.0;

			    control_emit( c, val );

			}

		} 
	}
}
// Process_CB

PRIVATE int process_callback( jack_nframes_t nframes, void *data ) {

    jack_timestamp = gen_get_sampletime();

    process_midi_control_port( nframes );

    if( transport_clocks ) {
	//jack_transport_info_t trans_info;
	jack_position_t jack_trans_pos;
	jack_transport_state_t jack_trans_state;
	GList *l;

	jack_trans_state = jack_transport_query( jack_client, &jack_trans_pos );

	if( jack_trans_state == JackTransportRolling ) {
	    double bpm;
	    if( jack_trans_pos.valid & JackPositionBBT ) {
		bpm = jack_trans_pos.beats_per_minute;
	    } else {
		bpm = 120.0;
	    }

	    for( l = transport_clocks; l; l = g_list_next( l ) ) {
		transport_frame_event_t *cb = l->data;
		cb->handler( cb->g, jack_trans_pos.frame, nframes, bpm );
	    }
	}
    }

    if( jack_process_callbacks ) {
	GList *l;
	for( l = jack_process_callbacks; l; l = g_list_next( l ) ) {
		jack_process_callback_t *cb = l->data;
		cb->handler( cb->g, nframes );
	}
    }
    gen_clock_mainloop_have_remaining( nframes );

    return 0;
}

PRIVATE void jack_shutdown (void *arg) {
	g_print( "jack exited :(\n" );
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {

    switch (reason) {
	case CLOCK_DISABLE:
	    jack_deactivate( jack_client );
	    break;

	case CLOCK_ENABLE:
	    jack_set_process_callback( jack_client, (JackProcessCallback) process_callback, NULL ); 
	    jack_on_shutdown (jack_client, jack_shutdown, 0);

	    jack_activate( jack_client );

	    lash_event_t *event;
	    if( lash_enabled( galan_lash_get_client() ) ) {
		event = lash_event_new_with_type(LASH_Jack_Client_Name);
		lash_event_set_string(event, jack_get_client_name( jack_client ) );
		lash_send_event( galan_lash_get_client(), event);
	    }
	    break;

	default:
	    g_message("Unreachable code reached (jack_output)... reason = %d", reason);
	    break;
    }
}


// Init/done

PUBLIC void init_jack(void) {

    int i;
    jack_client = jack_client_open( "galan", 0, NULL );
    if (jack_client == NULL) {
	    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
			    "Could not open Jack Client");
	    exit(10);
    }
    midi_control_port = jack_port_register ( jack_client, "control", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    for(i=0; i<128; i++ ) {
	midi_map[i] = NULL;
    }
}

PUBLIC void run_jack(void) {
    jack_clock = gen_register_clock(NULL, "Jack Clock", clock_handler);
    gen_select_clock(jack_clock);
}

PUBLIC void done_jack(void) {
    jack_deactivate( jack_client );
    jack_client_close( jack_client );
}

