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

#define GENERATOR_CLASS_NAME	"delay"
#define GENERATOR_CLASS_PATH	"Delay/Delay with feedback"

#define SIG_INPUT	0
#define SIG_OUTPUT	0

enum EVT_INPUTS {
  EVT_DELAY = 0,
  EVT_FEEDBACK,
  EVT_LIMIT,

  NUM_EVT_INPUTS
};

#define NUM_EVT_OUTPUTS	0

typedef struct Data {
  SAMPLE *delay_buf;

  gdouble delay_sec;
  int delay;
  gdouble feedback;
  gdouble limit;

  int offset;
} Data;

#define DELAY_CONTROL_DELAY	0
#define DELAY_CONTROL_FEEDBACK	1
#define DELAY_CONTROL_LIMIT	2
PRIVATE ControlDescriptor delay_controls[] = {
  { CONTROL_KIND_KNOB, "delay", 0,1,0.0025,0.001, 0,TRUE, 1,EVT_DELAY,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, delay_sec) },
  { CONTROL_KIND_KNOB, "feedback", 0,1,0.01,0.01, 0,TRUE, 1,EVT_FEEDBACK,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, feedback) },
  { CONTROL_KIND_SLIDER, "limit", 0,10,0.01,0.01, 0,TRUE, 1,EVT_LIMIT,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, limit) },
  { CONTROL_KIND_NONE, }
};

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->delay_buf = NULL;
  data->delay = 0;
  data->delay_sec = 0;
  data->feedback = 0.0;
  data->limit = 1.0;
  data->offset = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data->delay_buf != NULL)
    free(data->delay_buf);

  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->delay = objectstore_item_get_integer(item, "delay_delay", 0);
  data->delay_sec = data->delay / ((gdouble) SAMPLE_RATE);

  data->delay_buf = calloc(data->delay, sizeof(SAMPLE));
  data->feedback = objectstore_item_get_double(item, "delay_feedback", 0);
  data->limit = objectstore_item_get_double(item, "delay_limit", 1);
  data->offset = 0;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "delay_delay", data->delay);
  objectstore_item_set_double(item, "delay_feedback", data->feedback);
  objectstore_item_set_double(item, "delay_limit", data->limit);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;
  SAMPLE tmpbuf[MAXIMUM_REALTIME_STEP];

  if (data->delay == 0)
    return gen_read_realtime_input(g, SIG_INPUT, -1, buf, buflen);
  else {
    if (!gen_read_realtime_input(g, SIG_INPUT, -1, tmpbuf, buflen))
      memset(tmpbuf, 0, buflen * sizeof(SAMPLE));

    for (i = 0; i < buflen; i++) {
      SAMPLE tmp;

      if (data->offset >= data->delay)
	data->offset = 0;

      buf[i] = data->delay_buf[data->offset];

      tmp = tmpbuf[i] + data->delay_buf[data->offset] * data->feedback;
      data->delay_buf[data->offset] = MAX(MIN(tmp, data->limit), -(data->limit));
      data->offset++;
    }

    return TRUE;
  }
}

PRIVATE void evt_delay_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int olddelay = data->delay;

  data->delay_sec = MAX(0, event->d.number);
  data->delay = GEN_DOUBLE_TO_INT(data->delay_sec * SAMPLE_RATE);
  gen_update_controls(g, DELAY_CONTROL_DELAY);

  if (data->delay_buf != NULL)
    data->delay_buf = realloc(data->delay_buf, data->delay * sizeof(SAMPLE));
  else {
    data->delay_buf = calloc(data->delay, sizeof(SAMPLE));
    return;
  }

  if (data->delay > olddelay) {
    if (olddelay > 0) {
      int i;
      SAMPLE val = data->delay_buf[olddelay - 1];

      for (i = olddelay; i < data->delay; i++)
	data->delay_buf[i] = val;
    } else
      memset(data->delay_buf, 0, data->delay * sizeof(SAMPLE));
  }
}

PRIVATE void evt_feedback_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->feedback = event->d.number;
  gen_update_controls(g, DELAY_CONTROL_FEEDBACK);
}

PRIVATE void evt_limit_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->limit = event->d.number;
  gen_update_controls(g, DELAY_CONTROL_LIMIT);
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVT_INPUTS, NUM_EVT_OUTPUTS,
					     input_sigs, output_sigs, delay_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_DELAY, "Delay", evt_delay_handler);
  gen_configure_event_input(k, EVT_FEEDBACK, "Feedback", evt_feedback_handler);
  gen_configure_event_input(k, EVT_LIMIT, "Limit", evt_limit_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_delay(void) {
  setup_class();
}
