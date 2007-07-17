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
#include <math.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"evtsgn"
#define GENERATOR_CLASS_PATH	"Events/Sgn"

#define EVT_VALUE 0
#define EVT_OUTPUT 0

typedef struct Data {
    gdouble oldvalue;
} Data;

PRIVATE int init_instance(Generator *g) {

    Data *data = safe_malloc( sizeof(Data) );

    data->oldvalue = 0.0;

    g->data = data;

    return 1;
}

PRIVATE void destroy_instance(Generator *g) {
    if( g->data )
	free(g->data);

}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->oldvalue = objectstore_item_get_double(item, "sgn_oldvalue", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "sgn_oldvalue", data->oldvalue);

}


PRIVATE void evt_value_handler(Generator *g, AEvent *event) {

    Data *data = g->data;

    event->d.number = event->d.number == 0 ? 0 : (event->d.number < 0 ? -1 : 1);
    if( event->d.number != data->oldvalue ) {
	data->oldvalue = event->d.number;
	gen_send_events(g, EVT_OUTPUT, -1, event);
    }
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Input", evt_value_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Sgn");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_evtsgn(void) {
  setup_class();
}
