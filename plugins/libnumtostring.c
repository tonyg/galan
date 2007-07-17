/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 * Copyright (C) 2006 Torben Hohn
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

#define GENERATOR_CLASS_NAME	"numtostr"
#define GENERATOR_CLASS_PATH	"Events/Num To String"

#define EVT_VALUE 0
#define EVT_OUTPUT 0

typedef struct Data {
    char *fmt;
} Data;


PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
  
    free(data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = safe_malloc(sizeof(Data));

  g->data = data;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

}






PRIVATE void evt_value_handler(Generator *g, AEvent *event) {

    Data *data = g->data;

    if( event->kind == AE_NUMBER ) {

	gchar str[30];
	snprintf( str, 29, "%f", (float) event->d.number );
	gen_init_aevent(event, AE_STRING, NULL, 0, NULL, 0, gen_get_sampletime() );
	event->d.string = safe_string_dup(str);
	gen_send_events(g, EVT_OUTPUT, -1, event);
    }
}


PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     NULL, NULL, NULL,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Num", evt_value_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "StrOutput");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_numtostring(void) {
  setup_class();
}
