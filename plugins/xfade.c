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

#define GENERATOR_CLASS_NAME	"xfade"
#define GENERATOR_CLASS_PATH	"Levels/Cross-fader"

#define SIG_LEFT		0
#define SIG_RIGHT		1

#define SIG_OUTPUT		0

#define EVT_AMOUNT		0
#define NUM_EVENT_INPUTS	1

#define NUM_EVENT_OUTPUTS	0

typedef struct Data {
  gdouble amount;
} Data;

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->amount = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->amount = objectstore_item_get_double(item, "xfade_amount", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "xfade_amount", data->amount);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  SAMPLE bufl[MAXIMUM_REALTIME_STEP];
  SAMPLE bufr[MAXIMUM_REALTIME_STEP];
  gdouble lfact, rfact;
  int i;
  gboolean resl, resr;

  resl = gen_read_realtime_input(g, SIG_LEFT, -1, bufl, buflen);
  resr = gen_read_realtime_input(g, SIG_RIGHT, -1, bufr, buflen);

  if (!resl && !resr)
    return FALSE;

  if (!resl)
    memset(bufl, 0, sizeof(SAMPLE) * buflen);

  if (!resr)
    memset(bufr, 0, sizeof(SAMPLE) * buflen);

  if (data->amount > 0) {
    rfact = 1;
    lfact = 1 - data->amount;
  } else {
    lfact = 1;
    rfact = 1 + data->amount;
  }

  for (i = 0; i < buflen; i++)
    buf[i] = bufl[i] * lfact + bufr[i] * rfact;
  return TRUE;
}

PRIVATE void evt_amount_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->amount = MAX(-1, MIN(1, event->d.number));
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Left", SIG_FLAG_REALTIME },
  { "Right", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_KNOB, "fade", -1,1,0.01,0.01, 0,TRUE, 1,EVT_AMOUNT,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, amount) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_AMOUNT, "Fade", evt_amount_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_xfade(void) {
  setup_class();
}
