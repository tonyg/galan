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


#define NUM_VOICES 8

#define GENERATOR_CLASS_NAME	"voice-allocator"
#define GENERATOR_CLASS_PATH	"Misc/Voice Alocator"


typedef struct Data {
    gint    note[NUM_VOICES];
    gint    alloc_ring;
} Data;

PRIVATE int init_instance(Generator *g) {

  int i;
  Data *data = safe_malloc( sizeof( Data ) );

  for( i=0; i<NUM_VOICES; i++ )
	  data->note[i] = -1;

  data->alloc_ring = 0;
  
  g->data = data;
  
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

    free( g->data );
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  int i;
  ObjectStoreDatum *array;
  Data *data = safe_malloc(sizeof(Data));

  data->alloc_ring = 0;
  array = objectstore_item_get(item, "note");

  for (i = 0; i < NUM_VOICES; i++) {
      ObjectStoreDatum *dat = objectstore_datum_array_get(array, i );
      if( dat )
	  data->note[i] = objectstore_datum_integer_value( dat );
      else
	  data->note[i] = -1;
  }

  g->data = data;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_VOICES );
  int i;

  objectstore_item_set(item, "note", array);

  for (i = 0; i < NUM_VOICES; i++)
      objectstore_datum_array_set(array, i,
				  objectstore_datum_new_integer(data->note[i]));
}

PRIVATE void evt_note_on_handler(Generator *g, AEvent *event) {

  Data *data = g->data;
  
  gint note = event->d.number;
  int i;

  for( i=0; i<NUM_VOICES; i++ ) {
      if( data->note[i] == note ) {
	  gen_send_events(g, i*2, -1, event);
	  return;
      }
  }

  for( i=data->alloc_ring; i<NUM_VOICES+data->alloc_ring; i++ ) {
      if( data->note[i%NUM_VOICES] == -1 ) {
	  data->note[i%NUM_VOICES] = note;
	  gen_send_events(g, (i%NUM_VOICES)*2, -1, event);
	  data->alloc_ring++;
	  return;
      }
  }

  printf( "No more free voices and no lru function yet: dropping note\n" ); 
}

PRIVATE void evt_note_off_handler(Generator *g, AEvent *event) {

  Data *data = g->data;
  
  gint note = event->d.number;
  int i;

  for( i=0; i<NUM_VOICES; i++ ) {


      if( data->note[i] == note ) {
	  data->note[i] = -1;
	  gen_send_events(g, i*2+1, -1, event);
	  return;
      }
  }

      printf( "Spurious Note off\n" ); 
}

PRIVATE void evt_reset_handler(Generator *g, AEvent *event) {

  Data *data = g->data;
  
  int i;

  for( i=0; i<NUM_VOICES; i++ ) {
	  data->note[i] = -1;
	  gen_send_events(g, i*2+1, -1, event);
  }
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
    int i;

    GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 3, NUM_VOICES * 2,
	    NULL, NULL, controls,
	    init_instance, destroy_instance,
	    unpickle_instance, pickle_instance);

    gen_configure_event_input(k, 0, "Note On", evt_note_on_handler);
    gen_configure_event_input(k, 1, "Note Off", evt_note_off_handler);
    gen_configure_event_input(k, 2, "Reset", evt_reset_handler);

    for( i=0; i<NUM_VOICES; i++ ) {
	gen_configure_event_output(k, i*2, g_strdup_printf( "Note On %d", i ) );
	gen_configure_event_output(k, i*2+1, g_strdup_printf( "Note Off %d", i ) );
    }

    gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_voice_allocator(void) {
  setup_class();
}
