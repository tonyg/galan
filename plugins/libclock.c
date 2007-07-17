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

#define GENERATOR_CLASS_NAME	"clock"
#define GENERATOR_CLASS_PATH	"Misc/Clock"

enum EVT_INPUTS {
  EVT_FREQ = 0,
  EVT_RESET,
  EVT_TRIGGER,

  NUM_EVT_INPUTS
};

enum EVT_OUTPUTS {
  EVT_CLOCK = 0,

  NUM_EVT_OUTPUTS
};

typedef struct Data {
  gdouble freq;
  gint period;
  gint lasttrigger;
} Data;

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->period = 0;
  data->lasttrigger = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->period = objectstore_item_get_integer(item, "clock_period", 0);
  data->lasttrigger = gen_get_sampletime();

  if (data->period != 0) {
    AEvent event;
    data->freq = (gdouble) SAMPLE_RATE / data->period;
    gen_init_aevent(&event, AE_NUMBER, NULL, 0, g, EVT_TRIGGER, gen_get_sampletime());
    gen_post_aevent(&event);
  }
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "clock_period", data->period);
}

PRIVATE void evt_freq_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->freq = event->d.number;

  gen_purge_inputevent_queue_refs(g);
  if (event->d.number < 0.0001)
    data->period = 0;
  else
    data->period = SAMPLE_RATE / event->d.number;

  if (data->period != 0) {
      if( (event->time - data->lasttrigger) > data->period )
	  gen_init_aevent(event, AE_NUMBER, NULL, 0, g, EVT_TRIGGER, event->time);
      else
	  gen_init_aevent(event, AE_NUMBER, NULL, 0, g, EVT_TRIGGER, data->lasttrigger + data->period);
    gen_post_aevent(event);
  }
}

PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

      
  if (data->period != 0) {

      data->lasttrigger = event->time;
      gen_init_aevent(event, AE_NUMBER, NULL, 0, g, EVT_TRIGGER, event->time + data->period);
      gen_post_aevent(event);
  }
  gen_init_aevent(event, AE_NUMBER, NULL, 0, NULL, 0, event->time);
  event->d.number = 0;
  gen_send_events(g, EVT_CLOCK, -1, event);
}

PRIVATE void evt_reset_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  gen_purge_inputevent_queue_refs(g);
  if (data->period != 0) {

    gen_init_aevent(event, AE_NUMBER, NULL, 0, g, EVT_TRIGGER, event->time );
    gen_post_aevent(event);
  }

//  gen_init_aevent(event, AE_NUMBER, NULL, 0, NULL, 0, event->time);
//  event->d.number = 0;
//  gen_send_events(g, EVT_CLOCK, -1, event);
}



PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_KNOB, "rate", 0,500,1,1, 0,TRUE, TRUE,EVT_FREQ,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, freq) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVT_INPUTS, NUM_EVT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_FREQ, "Freq", evt_freq_handler);
  gen_configure_event_input(k, EVT_RESET, "Reset", evt_reset_handler);
  gen_configure_event_input(k, EVT_TRIGGER, "Trigger", evt_trigger_handler);
  gen_configure_event_output(k, EVT_CLOCK, "Clock");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_clock(void) {
  setup_class();
}
