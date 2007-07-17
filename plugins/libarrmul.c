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
#include <glib.h>
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define GENERATOR_CLASS_NAME	"arrmul"
#define GENERATOR_CLASS_PATH	"Array/Multiply"

#define EVT_INPUT		0
#define EVT_FACTOR		1
#define NUM_EVENT_INPUTS	2

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
  int len;
  gdouble *factor;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->factor = NULL;
  data->len = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
    if( data->factor )
	free( data->factor );
    free(data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  ObjectStoreDatum *array;
  int i;

  g->data = data;

  array = objectstore_item_get(item, "arrmul_factor");

  data->len = objectstore_item_get_double(item, "arrmul_len", 0);
  data->factor = safe_malloc( sizeof( gdouble ) * data->len );
  for(i=0; i<data->len; i++ )
      data->factor[i] = objectstore_datum_double_value( 
	      objectstore_datum_array_get(array, i)      );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

    ObjectStoreDatum *array;
    Data *data = g->data;
    int i;

    array = objectstore_datum_new_array(data->len);
    objectstore_item_set_integer(item, "arrmul_len", data->len);
    objectstore_item_set(item, "arrmul_factor", array);
    for (i = 0; i < data->len; i++) {
      objectstore_datum_array_set(array, i,
	      objectstore_datum_new_double( data->factor[i]) );
    }
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
    int i;
    Data *data = g->data;

    switch( event->kind ) {
	case AE_NUMARRAY:
	    if( event->d.array.len != data->len )
		
		g_print( "arrmul dimension mismatch (data->len, event->len ) = ( %d, %d )\n", data->len, event->d.array.len );
	    else {

		for( i=0; i<data->len; i++ )
		    event->d.array.numbers[i] *= data->factor[i];

		gen_send_events(g, EVT_OUTPUT, -1, event);
	    }
	    break;
	default:
	    g_warning( "arrmul does not support this eventkind\n" );
	    break;
    }
}

PRIVATE void evt_factor_handler(Generator *g, AEvent *event) {
    Data *data = g->data;
    int i;

    switch( event->kind ) {
	case AE_NUMARRAY:
	    if( event->d.array.len != data->len ) {
		free( data->factor );
		data->len = event->d.array.len;
		data->factor = safe_malloc( sizeof( gdouble ) * data->len );
	    }

	    // use memcpy here;
	    for( i=0; i<data->len; i++ )
		data->factor[i] = event->d.array.numbers[i];

	    break;
	default:
	    g_warning( "arrmul does not support this eventkind\n" );
	    break;
    }
}

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_input(k, EVT_FACTOR, "Factor", evt_factor_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_arrmul(void) {
  setup_class();
}
