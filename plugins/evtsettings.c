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

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"


#define NUM_MEMLINES 16

#define GENERATOR_CLASS_NAME	"evtsettings"
#define GENERATOR_CLASS_PATH	"Misc/Settings"

#define EVT_INPUT0 0
#define EVT_INPUT1 1
#define EVT_INPUT2 2
#define EVT_INPUT3 3
#define EVT_INPUT4 4
#define EVT_INPUT5 5
#define EVT_INPUT6 6
#define EVT_INPUT7 7
#define EVT_MEM    8
#define NUM_INPUT_EVENTS 9

#define EVT_OUTPUT0 0
#define EVT_OUTPUT1 1
#define EVT_OUTPUT2 2
#define EVT_OUTPUT3 3
#define EVT_OUTPUT4 4
#define EVT_OUTPUT5 5
#define EVT_OUTPUT6 6
#define EVT_OUTPUT7 7
#define NUM_OUTPUT_EVENTS 8

typedef struct Data {
    gint    activeline;
    gdouble mem[NUM_MEMLINES][NUM_OUTPUT_EVENTS];
} Data;

PRIVATE int init_instance(Generator *g) {

  int i,j;
  Data *data = safe_malloc( sizeof( Data ) );

  data->activeline = 0;

  for( i=0; i<NUM_MEMLINES; i++ )
      for( j=0; j<NUM_OUTPUT_EVENTS; j++ )
	  data->mem[i][j] = 0.0;

  
  g->data = data;
  
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

    free( g->data );
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  int i,j;
  ObjectStoreDatum *array;
  Data *data = safe_malloc(sizeof(Data));

  data->activeline = objectstore_item_get_integer(item, "activeline", 0);
  array = objectstore_item_get(item, "mem");

  for (i = 0; i < NUM_MEMLINES; i++)
    for (j = 0; j < NUM_OUTPUT_EVENTS; j++)
      data->mem[i][j] =
	objectstore_datum_double_value(
	  objectstore_datum_array_get(array, i * NUM_OUTPUT_EVENTS + j)
	);

  g->data = data;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_MEMLINES * NUM_OUTPUT_EVENTS);
  int i, j;

  objectstore_item_set_integer(item, "activeline", data->activeline);
  objectstore_item_set(item, "mem", array);

  for (i = 0; i < NUM_MEMLINES; i++)
    for (j = 0; j < NUM_OUTPUT_EVENTS; j++)
      objectstore_datum_array_set(array, i * NUM_OUTPUT_EVENTS + j,
				  objectstore_datum_new_double(data->mem[i][j]));
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {

  Data *data = g->data;
  
  data->mem[data->activeline][event->dst_q] = event->d.number;

  
  gen_send_events(g, event->dst_q, -1, event);
}


PRIVATE void evt_mem_handler(Generator *g, AEvent *event) {

    int i;
    Data *data = g->data;

    if( (event->d.number >=0) && (event->d.number < NUM_MEMLINES) ) {

	data->activeline = event->d.number;
	for( i=0; i<NUM_OUTPUT_EVENTS; i++ ) {
	    event->d.number = data->mem[data->activeline][i];
	    gen_send_events( g, i, -1, event );
	}
    }
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, NUM_INPUT_EVENTS, NUM_OUTPUT_EVENTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT0, "Input0", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT1, "Input1", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT2, "Input2", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT3, "Input3", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT4, "Input4", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT5, "Input5", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT6, "Input6", evt_input_handler);
  gen_configure_event_input(k, EVT_INPUT7, "Input7", evt_input_handler);
  gen_configure_event_input(k, EVT_MEM, "MemLine", evt_mem_handler);
  
  gen_configure_event_output(k, EVT_OUTPUT0, "Output0");
  gen_configure_event_output(k, EVT_OUTPUT1, "Output1");
  gen_configure_event_output(k, EVT_OUTPUT2, "Output2");
  gen_configure_event_output(k, EVT_OUTPUT3, "Output3");
  gen_configure_event_output(k, EVT_OUTPUT4, "Output4");
  gen_configure_event_output(k, EVT_OUTPUT5, "Output5");
  gen_configure_event_output(k, EVT_OUTPUT6, "Output6");
  gen_configure_event_output(k, EVT_OUTPUT7, "Output7");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_evtsettings(void) {
  setup_class();
}
