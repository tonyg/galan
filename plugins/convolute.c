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

#define GENERATOR_CLASS_NAME	"convolute"
#define GENERATOR_CLASS_PATH	"Filter/Convolute"

#define SIG_FILTER		0
#define SIG_INPUT		1

#define SIG_OUTPUT		0

#define NUM_EVENT_INPUTS	0

#define NUM_EVENT_OUTPUTS	0

typedef struct Data {
	gint32 ringpos;  
	gint32 ringsize;
	SAMPLE *ring;
} Data;


PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->ringsize=0;
  data->ringpos=0;


  
  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;

  data->ringpos = objectstore_item_get_integer(item, "convolute_pos", 0);
  data->ringsize = objectstore_item_get_integer(item, "convolute_size", 0);

  data->ring = safe_malloc( sizeof(SAMPLE)*data->ringsize );
  memset( data->ring, 0, sizeof(SAMPLE)*data->ringsize );
  /*array = objectstore_item_get(item, "seqnote_patterns");
  notes = objectstore_item_get(item, "seqnote_notes");

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++) {
      data->pattern[i][j] =
	objectstore_datum_integer_value(
	  objectstore_datum_array_get(array, i * SEQUENCE_LENGTH + j)
	);
      data->note[i][j] =
	objectstore_datum_integer_value(
	  objectstore_datum_array_get(notes, i * SEQUENCE_LENGTH + j)
	);
    }
	*/
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;

  objectstore_item_set_integer(item, "convolute_pos", data->ringpos);
  objectstore_item_set_integer(item, "convolute_size", data->ringsize);
}


PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen)
{
	Data *data = g->data;
	SAMPLE *input_buf;
	SAMPLE *filter_buf;
	SAMPLETIME range;
	int i;

	range = gen_get_randomaccess_input_range(g, 0, 0);
	if( !range )
	{
		//copy input to output.
		return FALSE;
	}

	// Now Alloc all mem we will need.

	input_buf = safe_malloc( sizeof(SAMPLE)*buflen );
	filter_buf= safe_malloc( sizeof(SAMPLE)*range );

	// Does the ringsize change ???
	if( range != data->ringsize )
	{
		if( data->ringsize )
			free( data->ring );
		data->ring = safe_malloc( sizeof(SAMPLE) * range );
		data->ringsize = range;
	}

	// read realtime signal 
	if (!gen_read_realtime_input(g, SIG_INPUT, 0, input_buf, buflen)) 
	{
		free(input_buf);
		free(filter_buf);
		return FALSE;
	}

	// then read_filter:
	if (!gen_read_randomaccess_input(g, SIG_FILTER, 0, 0, filter_buf, range))
	{
		free(input_buf);
		free(filter_buf);
		return FALSE;
	}


	for( i=0; i<buflen; i++ )
	{
		SAMPLE acc=0;
		int j;
		int ringX;
		data->ringpos++;
		if( data->ringpos >= range )
			data->ringpos -= range;
		data->ring[data->ringpos] = input_buf[i];

		ringX=data->ringpos;
		for( j=0; j<range; j++)
		{
			acc += filter_buf[j]*data->ring[ringX];
			ringX++;
			if( ringX >= range )
				ringX -= range;
		}
		buf[i]=acc;
	}
	free(input_buf);
	free(filter_buf);

	return TRUE;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Filter", SIG_FLAG_RANDOMACCESS },
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
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);


  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_convolute(void) {
  setup_class();
}
