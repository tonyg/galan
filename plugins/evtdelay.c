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

#define GENERATOR_CLASS_NAME	"evtdelay"
#define GENERATOR_CLASS_PATH	"Events/Delay"

#define EVT_INPUT 0
#define EVT_DELAY 1
#define EVT_OUTPUT 0

typedef struct Data {
    gdouble delay_sec;
    SAMPLETIME delay;
} Data;

PRIVATE int init_instance(Generator *g) {

  Data *data = safe_malloc( sizeof( Data ) );
  data->delay_sec = 0.0;
  data->delay = 0;
  g->data = data;
  
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

    free( g->data );
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->delay_sec = objectstore_item_get_double(item, "delay_sec", 0);
  data->delay = data->delay_sec * SAMPLE_RATE;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = g->data;
  objectstore_item_set_double(item, "delay_sec", data->delay_sec);
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  event->time += data->delay;
  gen_send_events(g, EVT_OUTPUT, -1, event);
}


PRIVATE void evt_delay_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  data->delay_sec = event->d.number;
  data->delay = data->delay_sec * SAMPLE_RATE;
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 2, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_input(k, EVT_DELAY, "Delay", evt_delay_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_evtdelay(void) {
  setup_class();
}
