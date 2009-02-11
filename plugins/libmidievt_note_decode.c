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
#include <stddef.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define GENERATOR_CLASS_NAME	"midievt_note_decode"
#define GENERATOR_CLASS_PATH	"MIDI Events/Note Decoder"

#define EVT_MIDI_IN		0
#define EVT_CHANNEL		1
#define NUM_EVENT_INPUTS	2

#define EVT_NOTEON		0
#define EVT_NOTEOFF		1
#define EVT_NOTEVEL		2
#define NUM_EVENT_OUTPUTS	3

typedef struct Data {
    int channel; 
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->channel = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;

  data->channel = objectstore_item_get_double(item, "channel", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;

  objectstore_item_set_double(item, "channel", data->channel);
}

PRIVATE void evt_midi_handler(Generator *g, AEvent *event) {
    Data *data = g->data;

    switch( event->kind ) {
	case AE_MIDIEVENT:
	    if( (event->d.midiev.midistring[0] & 0xf0) == 0x90 ) {
		int note = event->d.midiev.midistring[1];
		int channel = event->d.midiev.midistring[0] & 0x0f;
		int vel  = (double)event->d.midiev.midistring[2];

		if( channel == data->channel ) {
		    event->kind = AE_NUMBER;
		    event->d.number = note;
		    gen_send_events( g, EVT_NOTEON , -1, event );
		    event->d.number = vel;
		    gen_send_events( g, EVT_NOTEVEL , -1, event );
		}
	    }

	    if( (event->d.midiev.midistring[0] & 0xf0) == 0x80 ) {
		int note = event->d.midiev.midistring[1];
		int channel = event->d.midiev.midistring[0] & 0x0f;
		int vel  = (double)event->d.midiev.midistring[2];

		if( channel == data->channel ) {
		    event->kind = AE_NUMBER;
		    event->d.number = note;
		    gen_send_events( g, EVT_NOTEOFF , -1, event );
		    event->d.number = vel;
		    gen_send_events( g, EVT_NOTEVEL , -1, event );
		}
	    }
	    break;
	default:
	    g_warning( "midi_note_demultiplex does not know this eventkind\n" );
	    break;
    }
}

PRIVATE void evt_channel_handler(Generator *g, AEvent *event) {
    Data *data = g->data;
    data->channel = event->d.number;
    gen_update_controls( g, -1 );
}


PRIVATE ControlDescriptor controls[] = {
    /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
      { CONTROL_KIND_KNOB, "channel", 0,15,1,1, 0,TRUE, TRUE, EVT_CHANNEL,
        NULL,NULL, control_int32_updater, (gpointer) offsetof(Data, channel) },
    { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
    GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
	    NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
	    NULL, NULL, controls,
	    init_instance, destroy_instance,
	    unpickle_instance, pickle_instance);

    gen_configure_event_input(k, EVT_MIDI_IN, "midi-in", evt_midi_handler );
    gen_configure_event_input(k, EVT_CHANNEL, "channel", evt_channel_handler );
    gen_configure_event_output(k, EVT_NOTEON, "note-on");
    gen_configure_event_output(k, EVT_NOTEOFF, "note-off");
    gen_configure_event_output(k, EVT_NOTEVEL, "velocity");

    gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
	    NULL,
	    NULL);
}

PUBLIC void init_plugin(void) {
    setup_class();
}
