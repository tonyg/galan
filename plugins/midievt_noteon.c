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

#define GENERATOR_CLASS_NAME	"midievt_noteon"
#define GENERATOR_CLASS_PATH	"MIDI Events/NoteOn"

#define EVT_NOTE		0
#define EVT_VELOCITY		1
#define EVT_CHANNEL		2
#define NUM_EVENT_INPUTS	3

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
  char velocity;
  char channel;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->velocity = 127;
  data->channel = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->velocity = objectstore_item_get_double(item, "velocity", 127);
  data->channel = objectstore_item_get_double(item, "channel", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "velocity", data->velocity);
  objectstore_item_set_double(item, "channel", data->channel);
}

PRIVATE void evt_note_handler(Generator *g, AEvent *event) {
    char note;
    Data *data = g->data;

    switch( event->kind ) {
	case AE_NUMBER:
	    note = ((char) event->d.number) & 0x7f;
	    event->kind = AE_MIDIEVENT;
	    event->d.midiev.len = 3;
	    event->d.midiev.midistring[0] = 0x90 | (data->channel & 0x0f);
	    event->d.midiev.midistring[1] = note;
		event->d.midiev.midistring[2] = data->velocity; 
		gen_send_events(g, EVT_OUTPUT, -1, event);
		break;
	    default:
		g_warning( "midi_note does not know this eventkind\n" );
		break;
	}
    }

    PRIVATE void evt_velocity_handler(Generator *g, AEvent *event) {
      ((Data *) g->data)->velocity = (char) event->d.number & 0x7f;
    }

    PRIVATE void evt_channel_handler(Generator *g, AEvent *event) {
      ((Data *) g->data)->channel = (char) event->d.number & 0x0f;
    }

    PRIVATE ControlDescriptor controls[] = {
      /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
	   init,destroy,refresh,refresh_data }, */
    //  { CONTROL_KIND_KNOB, "velocity", -1,1,0.1,0.01, 0,TRUE, TRUE, EVT_ADDEND,
    //    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, addend) },
      { CONTROL_KIND_NONE, }
    };

    PRIVATE void setup_class(void) {
      GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
						 NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
						 NULL, NULL, controls,
						 init_instance, destroy_instance,
						 unpickle_instance, pickle_instance);

      gen_configure_event_input(k, EVT_NOTE, "Note", evt_note_handler);
  gen_configure_event_input(k, EVT_VELOCITY, "Velocity", evt_velocity_handler);
  gen_configure_event_input(k, EVT_CHANNEL, "Channel", evt_channel_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_midievt_noteon(void) {
  setup_class();
}
