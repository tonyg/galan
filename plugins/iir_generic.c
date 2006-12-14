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

#define GENERATOR_CLASS_NAME	"iir"
#define GENERATOR_CLASS_PATH	"Filter/Generic IIR Filter"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_COEFFS		0
#define EVT_RESET		1
#define NUM_EVENT_INPUTS	2

#define NUM_EVENT_OUTPUTS	0

typedef struct Data {
    int len;
    gdouble *coeffs;
    gdouble *zcoeffs;
    gdouble *state;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->len = 1;
  data->coeffs = safe_malloc( sizeof( double ) * data->len );
  data->zcoeffs = safe_malloc( sizeof( double ) * data->len );
  //data->state = safe_malloc( sizeof( double ) * (data->len-1) );
	  
  data->coeffs[0] = 1.0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

    Data *data = g->data;
    
    free(data->coeffs);
    free(data->zcoeffs);
    if( data->len > 1 )
	free(data->state);
    free(data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  ObjectStoreDatum *coeffs = objectstore_item_get( item, "iir_coeffs" );
  ObjectStoreDatum *zcoeffs = objectstore_item_get( item, "iir_zcoeffs" );
  ObjectStoreDatum *state = objectstore_item_get( item, "iir_state" );
  int i;

  g->data = data;

  data->len = objectstore_item_get_integer( item, "iir_len", 1 );

  data->coeffs = safe_malloc( sizeof( double ) * data->len );
  data->zcoeffs = safe_malloc( sizeof( double ) * data->len );
  
  if( data->len > 1 )
      data->state = safe_malloc( sizeof( double ) * (data->len-1) );

  for( i=0; i<data->len; i++ )
      data->coeffs[i] = objectstore_datum_double_value( objectstore_datum_array_get( coeffs, i ) );
  for( i=0; i<data->len; i++ )
      if( zcoeffs != NULL)
	  data->zcoeffs[i] = objectstore_datum_double_value( objectstore_datum_array_get( zcoeffs, i ) );
      else
	  data->zcoeffs[i] = 0;

  for( i=0; i < (data->len-1); i++ )
      data->state[i] = objectstore_datum_double_value( objectstore_datum_array_get( state, i ) );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  ObjectStoreDatum *coeffs = objectstore_datum_new_array( data->len );
  ObjectStoreDatum *zcoeffs = objectstore_datum_new_array( data->len );
  ObjectStoreDatum *state  = objectstore_datum_new_array( data->len - 1 );
  int i;

  for( i=0; i<data->len; i++ )
      objectstore_datum_array_set( coeffs, i, objectstore_datum_new_double( data->coeffs[i] ) );
  for( i=0; i<data->len; i++ )
      objectstore_datum_array_set( zcoeffs, i, objectstore_datum_new_double( data->zcoeffs[i] ) );
  for( i=0; i < (data->len-1); i++ )
      objectstore_datum_array_set( state, i, objectstore_datum_new_double( data->state[i] ) );
  
  objectstore_item_set_integer(item, "iir_len", data->len);
  objectstore_item_set(item, "iir_coeffs", coeffs);
  objectstore_item_set(item, "iir_zcoeffs", zcoeffs);
  objectstore_item_set(item, "iir_state", state);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i,j;

  if (!gen_read_realtime_input(g, SIG_INPUT, -1, buf, buflen))
      memset(buf, 0, buflen * sizeof(SAMPLE));

  for (i = 0; i < buflen; i++) {
      SAMPLE tmp = buf[i] * data->coeffs[0]; // This is normally not in canonical form 2
					     // But i include this here for compatibility
					     // TODO: sort this out again.

      for( j=1; j<data->len; j++ )
	  tmp += data->coeffs[j] * data->state[j-1];

      buf[i] = tmp * data->zcoeffs[0];

      for( j=1; j<data->len; j++ )
	  buf[i] += data->zcoeffs[j] * data->state[j-1];

      for( j = data->len-2; j>0; j-- ) 
	  data->state[j] = data->state[j-1];

      if( data->len > 1 )
	  data->state[0] = tmp;
  }
  return TRUE;
}

PRIVATE void evt_coeffs_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int i,len;

  RETURN_UNLESS( event->kind == AE_DBLARRAY );
  RETURN_UNLESS( (event->d.darray.len & 1) == 0 );

  len = event->d.darray.len / 2;

  if( len != data->len ) {
      if( data->len > 1 )
	  free( data->state );
      free( data->coeffs );
      free( data->zcoeffs );

      data->len = len;

      data->coeffs = safe_malloc( sizeof( double ) * data->len );
      data->zcoeffs = safe_malloc( sizeof( double ) * data->len );
      if( data->len > 1 )
	  data->state = safe_malloc( sizeof( double ) * (data->len-1) );
      for( i=0; i<data->len-1; i++ )
	  data->state[i] = 0;
  }
  for( i=0; i<data->len; i++ )
      data->coeffs[i] = event->d.darray.numbers[i];
  for( i=0; i<data->len; i++ )
      data->zcoeffs[i] = event->d.darray.numbers[i+data->len];
}

PRIVATE void evt_reset_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int i;

  for( i=0; i<data->len-1; i++ )
      data->state[i] = 0;
}
PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, },
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_COEFFS, "Coefficients", evt_coeffs_handler);
  gen_configure_event_input(k, EVT_RESET, "Reset", evt_reset_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_iir_generic(void) {
  setup_class();
}
