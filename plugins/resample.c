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

#define GENERATOR_CLASS_NAME	"resample"
#define GENERATOR_CLASS_PATH	"Timebase/Resample"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_FACTOR		0
#define EVT_NOTE		1

typedef struct Data {
  gdouble factor;
} Data;

#define FREQ_C0	16.351598
#define FREQ_A4 440
#define NOTE_C4 48
#define NOTE_A4 57
#define NOTE_TO_FACTOR(n, centre)		(pow(2, ((n) - (centre)) / 12.0))

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->factor = 1;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->factor = objectstore_item_get_double(item, "resample_factor", 1);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "resample_factor", data->factor);
}

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  Data *data = g->data;

  if (data->factor == 0)
    return 0;	/* GIGO */

  return gen_get_randomaccess_input_range(g, SIG_INPUT, 0) / data->factor;
}

PRIVATE gboolean output_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  SAMPLE tmpbuf[MAXIMUM_REALTIME_STEP*5];
  int i;
  gdouble o;

  if (!gen_read_randomaccess_input(g, SIG_INPUT, 0,
				   offset*data->factor, tmpbuf, buflen*data->factor + 1))
    return FALSE;

  for (i = 0, o = 0; i < buflen; i++, o += data->factor) {
    int oi = floor(o);
    gdouble fract = o - oi;
    buf[i] = ( tmpbuf[oi]*(1-fract) + tmpbuf[oi+1]*fract ) / 2;
  }

  return TRUE;
}

PRIVATE void evt_factor_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->factor = MAX(MIN(event->d.number, 4), 0);
}

PRIVATE void evt_note_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->factor = MAX(MIN(NOTE_TO_FACTOR(event->d.number, NOTE_C4), 4), 0);
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_RANDOMACCESS },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, output_generator } } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "factor", 0.5,4,0.01,0.01, 0,TRUE, TRUE,EVT_FACTOR,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, factor) },
  { CONTROL_KIND_KNOB, "note", 0,127,1,1, 0,TRUE, TRUE,EVT_NOTE,
    NULL,NULL, NULL },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 2, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_FACTOR, "Factor", evt_factor_handler);
  gen_configure_event_input(k, EVT_NOTE, "Note", evt_note_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_resample(void) {
  setup_class();
}
