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

#define GENERATOR_CLASS_NAME	"303vcf"
#define GENERATOR_CLASS_PATH	"Filter/2-pole Low-pass resonant"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_Q			0
#define EVT_CUTOFF		1
#define NUM_EVENT_INPUTS	2

#define NUM_EVENT_OUTPUTS	0

#define VCF_CONTROL_Q		0
#define VCF_CONTROL_CUTOFF	1

typedef struct Data {
  gdouble q;
  gdouble cutoff;

  gdouble a, b, c;
  gdouble d1, d2;
  gdouble d3, d4;
} Data;

PRIVATE void calc_coeffs(Generator *g) {
  Data *data = g->data;
  gdouble cutoff_ratio = data->cutoff / SAMPLE_RATE;
  gdouble tmp;
  
  tmp = -M_PI * cutoff_ratio / data->q;

  data->a = 2 * cos(2 * M_PI * cutoff_ratio) * exp(tmp);
  data->b = -exp(2 * tmp);
  data->c = 1 - data->a - data->b;
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->q = 0.05;
  data->cutoff = 200;

  data->a = data->b = data->c = 0;
  data->d1 = data->d2 = 0;
  data->d3 = data->d4 = 0;

  calc_coeffs(g);

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->q = objectstore_item_get_double(item, "vcf_q", 0.05);
  data->cutoff = objectstore_item_get_double(item, "vcf_cutoff", 200);

  data->a = data->b = data->c = 0;
  data->d1 = data->d2 = 0;
  data->d3 = data->d4 = 0;
  calc_coeffs(g);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "vcf_q", data->q);
  objectstore_item_set_double(item, "vcf_cutoff", data->cutoff);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if (!gen_read_realtime_input(g, SIG_INPUT, -1, buf, buflen))
      memset(buf, 0, buflen * sizeof(SAMPLE));

  for (i = 0; i < buflen; i++) {
    buf[i] = data->a * data->d1 + data->b * data->d2 + data->c * buf[i];
    data->d2 = data->d1;
    data->d1 = buf[i];

    buf[i] = data->a * data->d3 + data->b * data->d4 + data->c * buf[i];
    data->d4 = data->d3;
    data->d3 = buf[i];
  }
  return TRUE;
}

PRIVATE void evt_q_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  data->q = MAX(event->d.number, 0.00001);
  calc_coeffs(g);
  gen_update_controls(g, VCF_CONTROL_Q);
}

PRIVATE void evt_cutoff_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  data->cutoff = MAX(event->d.number, 0.00001);
  calc_coeffs(g);
  gen_update_controls(g, VCF_CONTROL_CUTOFF);
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
  { CONTROL_KIND_KNOB, "reso", 0, 100, 0.5, 0.5, 0,TRUE, 1,EVT_Q,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, q) },
  { CONTROL_KIND_KNOB, "cutoff", 1, 22050, 4, 1, 0,TRUE, 1,EVT_CUTOFF,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, cutoff) },
  { CONTROL_KIND_NONE, },
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_Q, "Resonance", evt_q_handler);
  gen_configure_event_input(k, EVT_CUTOFF, "Cutoff", evt_cutoff_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_vcf(void) {
  setup_class();
}
