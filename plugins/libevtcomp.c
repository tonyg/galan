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

#define GENERATOR_CLASS_NAME	"evtcomp"
#define GENERATOR_CLASS_PATH	"Events/Compare"

#define EVT_INPUT		0
#define EVT_COMP		1
#define NUM_EVENT_INPUTS	2

#define EVT_OUTPUT		0
#define EVT_NEOUT		1
#define NUM_EVENT_OUTPUTS	2

typedef struct Data {
  gdouble comp;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->comp = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->comp = objectstore_item_get_double(item, "evtcomp_comp", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "evtcomp_comp", data->comp);
}


PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
  
  Data *data = (Data *) g->data;
  if( event->d.number == data->comp ) {
      event->d.number = 1;
      gen_send_events(g, EVT_OUTPUT, -1, event);
  } else {
      gen_send_events(g, EVT_NEOUT, -1, event);
  }
}

PRIVATE void evt_comp_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->comp = event->d.number;
}

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "comp", 0,30,1,1, 0,TRUE, TRUE,EVT_COMP,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, comp) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_input(k, EVT_COMP, "Compare", evt_comp_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");
  gen_configure_event_output(k, EVT_NEOUT, "Not Eq Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_evtcomp(void) {
  setup_class();
}
