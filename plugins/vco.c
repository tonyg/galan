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

#define EVT_TRIGGER	0
#define EVT_KIND	1

#define SIG_FREQ	0

typedef enum OscKind {
  OSC_MIN_KIND = 0,

  OSC_KIND_SIN = 0,
  OSC_KIND_SQR,
  OSC_KIND_SAW,

  OSC_NUM_KINDS
} OscKind;

typedef struct Data {
  gint32 kind;
  SAMPLE *sample_table;
  gdouble phase;
} Data;

PRIVATE SAMPLE sample_table[OSC_NUM_KINDS][SAMPLE_RATE];

PRIVATE void setup_tables(void) {
  const gdouble rad_per_sample = 2.0 * M_PI / SAMPLE_RATE;
  const gdouble saw_step = (SAMPLE_MAX - SAMPLE_MIN) / SAMPLE_RATE;
  int i;

  for (i = 0; i < SAMPLE_RATE; i++)
    sample_table[OSC_KIND_SIN][i] = SAMPLE_MUL(SAMPLE_UNIT, sin(i * rad_per_sample));

  for (i = 0; i < SAMPLE_RATE / 2; i++)
    sample_table[OSC_KIND_SQR][i] = SAMPLE_MAX;
  for (i = SAMPLE_RATE / 2; i < SAMPLE_RATE; i++)
    sample_table[OSC_KIND_SQR][i] = SAMPLE_MIN;

  for (i = 0; i < SAMPLE_RATE; i++)
    sample_table[OSC_KIND_SAW][i] = SAMPLE_MIN + i * saw_step;
}

PRIVATE gboolean init_instance(Generator *g) {
  Data *d = safe_malloc(sizeof(Data));
  g->data = d;

  d->kind = OSC_KIND_SIN;
  d->sample_table = sample_table[d->kind];
  d->phase = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *d = safe_malloc(sizeof(Data));
  g->data = d;

  d->kind = objectstore_item_get_integer(item, "vco_kind", OSC_KIND_SAW);
  d->sample_table = sample_table[d->kind];
  d->phase = objectstore_item_get_double(item, "vco_phase", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *d = g->data;
  objectstore_item_set_integer(item, "vco_kind", d->kind);
  objectstore_item_set_double(item, "vco_phase", d->phase);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  int i;
  SAMPLE freqbuf[MAXIMUM_REALTIME_STEP];
  Data *d = g->data;

  if (!gen_read_realtime_input(g, SIG_FREQ, -1, freqbuf, buflen))
    memset(freqbuf, 0, sizeof(SAMPLE) * buflen);

  for (i = 0; i < buflen; i++) {
    buf[i] = d->sample_table[(int) d->phase];
    d->phase += MIN(MAX(freqbuf[i], 0), (SAMPLE_RATE>>1));
    if (d->phase >= SAMPLE_RATE)
      d->phase -= SAMPLE_RATE;
  }

  return TRUE;
}

PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->phase = 0;
}

PRIVATE void evt_kind_handler(Generator *g, AEvent *event) {
  Data *d = g->data;
  gint32 k = GEN_DOUBLE_TO_INT(event->d.number);

  if (k >= OSC_MIN_KIND && k < OSC_NUM_KINDS) {
    d->kind = k;
    d->sample_table = sample_table[k];
  }
  gen_update_controls(g, 0);
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Frequency", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE ControlDescriptor osc_controls[] = {
  { CONTROL_KIND_KNOB, "Waveform", 0,2,0,1, 0,FALSE, TRUE,EVT_KIND,
    NULL,NULL, control_int32_updater, (gpointer) offsetof(Data, kind) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("varosc", FALSE, 2, 0,
					     input_sigs, output_sigs, osc_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_TRIGGER, "Trigger", evt_trigger_handler);
  gen_configure_event_input(k, EVT_KIND, "Oscillator Kind", evt_kind_handler);

  gencomp_register_generatorclass(k, FALSE, "Sources/Variable Oscillator",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_vco(void) {
  setup_tables();
  setup_class();
}
