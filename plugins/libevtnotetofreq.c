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

#define GENERATOR_CLASS_NAME	"evtnotetofreq"
#define GENERATOR_CLASS_PATH	"Events/Note2Freq"

#define EVT_VALUE 0
#define EVT_OUTPUT 0

typedef struct Data {
} Data;

PRIVATE int init_instance(Generator *g) {

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

}


PRIVATE void evt_value_handler(Generator *g, AEvent *event) {

  event->d.number = 440 * pow( 2, (event->d.number - 57) / 12.0);
  gen_send_events(g, EVT_OUTPUT, -1, event);
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Note", evt_value_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Freq");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_evtnotetofreq(void) {
  setup_class();
}
