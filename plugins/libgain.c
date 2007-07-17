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

#define GENERATOR_CLASS_NAME	"gain"
#define GENERATOR_CLASS_PATH	"Levels/Gain"

#define SIG_INPUT	0
#define SIG_OUTPUT	0

#define EVT_GAIN	0
#define NUM_EVT_INPUTS	1
#define NUM_EVT_OUTPUTS	0

typedef struct Data {
  gdouble gain;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->gain = 1;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->gain = objectstore_item_get_double(item, "gain_gain", 1);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "gain_gain", data->gain);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if (!gen_read_realtime_input(g, SIG_INPUT, -1, buf, buflen))
    return FALSE;

  /* I'm doing this check after reading the input just to keep the
     timing in sync. Just a minor niggle. */
  if (data->gain == 0.0)
    return FALSE;

  if (data->gain != 1.0) {
    for (i = 0; i < buflen; i++)
      buf[i] *= data->gain;
  }

  return TRUE;
}

PRIVATE void evt_gain_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->gain = event->d.number;
  gen_update_controls(g, 0);
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
  { CONTROL_KIND_SLIDER, "gain", 0,1,0.01,0.01, 0,TRUE, 1,EVT_GAIN,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, gain) },
  { CONTROL_KIND_NONE, },
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVT_INPUTS, NUM_EVT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_GAIN, "Gain", evt_gain_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_gain(void) {
  setup_class();
}
