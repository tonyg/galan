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

#define GENERATOR_CLASS_NAME	"midievt_cc_demultiplex"
#define GENERATOR_CLASS_PATH	"MIDI Events/CC demultiplexer"

#define EVT_MIDI_IN		0
#define EVT_LEARN1		1
#define EVT_LEARN2		2
#define EVT_LEARN3		3
#define EVT_LEARN4		4
#define EVT_LEARN5		5
#define EVT_LEARN6		6
#define EVT_LEARN7		7
#define EVT_LEARN8		8
#define NUM_EVENT_INPUTS	9

#define EVT_OUTPUT1		0
#define EVT_OUTPUT2		1
#define EVT_OUTPUT3		2
#define EVT_OUTPUT4		3
#define EVT_OUTPUT5		4
#define EVT_OUTPUT6		5
#define EVT_OUTPUT7		6
#define EVT_OUTPUT8		7
#define NUM_EVENT_OUTPUTS	8

typedef struct learned { 
  int controler;
  int channel;
} learned;

typedef struct Data {
    learned le[8];
    int learn_target;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i;
  g->data = data;

  for( i=0; i<8; i++ ) {
      data->le[i].controler = -1;
      data->le[i].channel = -1;
  }
  data->learn_target = -1;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int i;

  ObjectStoreDatum *controlers = objectstore_item_get( item, "controlers" );
  ObjectStoreDatum *channels = objectstore_item_get( item, "channels" );

  g->data = data;

  for( i=0; i<8; i++ ) {
      data->le[i].controler = objectstore_datum_integer_value( objectstore_datum_array_get(controlers, i) );
      data->le[i].channel = objectstore_datum_integer_value( objectstore_datum_array_get(channels, i) );
  }
  data->learn_target=-1;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  int i;

  ObjectStoreDatum *controlers = objectstore_datum_new_array(8);
  ObjectStoreDatum *channels = objectstore_datum_new_array(8);

  objectstore_item_set(item, "controlers", controlers);
  objectstore_item_set(item, "channels", channels);

  for( i=0; i<8; i++ ) {
      objectstore_datum_array_set( controlers, i, objectstore_datum_new_integer(data->le[i].controler) );
      objectstore_datum_array_set( channels, i, objectstore_datum_new_integer(data->le[i].channel) );
  }
 
}

PRIVATE void evt_midi_handler(Generator *g, AEvent *event) {
    Data *data = g->data;

    switch( event->kind ) {
	case AE_MIDIEVENT:
	    if( (event->d.midiev.midistring[0] & 0xf0) == 0xb0 ) {
		int control = event->d.midiev.midistring[1];
		int channel = event->d.midiev.midistring[0] & 0x0f;
		double val  = (double)event->d.midiev.midistring[2]/64.0 - 1.0;
		int i;

		if( data->learn_target < 0 ) {

		    for( i=0; i<8; i++ ) {
			if( (data->le[i].controler == control) && (data->le[i].channel == channel) ) {
			    event->kind = AE_NUMBER;
			    event->d.number = val;
			    gen_send_events( g, i, -1, event );
			}
		    }
		} else if( data->learn_target < 8 ) {
		    data->le[data->learn_target].controler = control;
		    data->le[data->learn_target].channel = channel;

		    event->kind = AE_NUMBER;
		    event->d.number = val;
		    gen_send_events( g, data->learn_target, -1, event );

		    data->learn_target = -1;
		}
	    }

	    break;
	default:
	    g_warning( "midi_cc does not know this eventkind\n" );
	    break;
    }
}

PRIVATE void evt_learn_handler(Generator *g, AEvent *event) {
    Data *data = g->data;
    data->learn_target = MAX( 0, MIN( 7,event->dst_q - 1 ) );
}


PRIVATE ControlDescriptor controls[] = {
    /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
    //  { CONTROL_KIND_KNOB, "velocity", -1,1,0.1,0.01, 0,TRUE, TRUE, EVT_ADDEND,
    //    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, addend) },
    { CONTROL_KIND_BUTTON, "l1", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN1, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l2", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN2, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l3", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN3, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l4", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN4, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l5", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN5, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l6", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN6, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l7", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN7, NULL,NULL,NULL },
    { CONTROL_KIND_BUTTON, "l8", 0,0,0,0, 0,FALSE, TRUE,EVT_LEARN8, NULL,NULL,NULL },
    { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
    GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
	    NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
	    NULL, NULL, controls,
	    init_instance, destroy_instance,
	    unpickle_instance, pickle_instance);

    gen_configure_event_input(k, EVT_MIDI_IN, "midi-in", evt_midi_handler );
    gen_configure_event_input(k, EVT_LEARN1, "l1", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN2, "l2", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN3, "l3", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN4, "l4", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN5, "l5", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN6, "l6", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN7, "l7", evt_learn_handler);
    gen_configure_event_input(k, EVT_LEARN8, "l8", evt_learn_handler);
    gen_configure_event_output(k, EVT_OUTPUT1, "out1");
    gen_configure_event_output(k, EVT_OUTPUT2, "out2");
    gen_configure_event_output(k, EVT_OUTPUT3, "out3");
    gen_configure_event_output(k, EVT_OUTPUT4, "out4");
    gen_configure_event_output(k, EVT_OUTPUT5, "out5");
    gen_configure_event_output(k, EVT_OUTPUT6, "out6");
    gen_configure_event_output(k, EVT_OUTPUT7, "out7");
    gen_configure_event_output(k, EVT_OUTPUT8, "out8");

    gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
	    NULL,
	    NULL);
}

PUBLIC void init_plugin_midievt_cc_demultiplex(void) {
    setup_class();
}
