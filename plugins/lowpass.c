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

#define GENERATOR_CLASS_NAME	"filter_lp"
#define GENERATOR_CLASS_PATH	"Filter/Lowpass Coeffs"

#define EVT_CUTOFF		0
#define EVT_RESO		1
#define NUM_EVENT_INPUTS	2

#define EVT_COEFFS		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
  gdouble cutoff;
  gdouble reso;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->cutoff = 200;
  data->reso   = 0.5;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->cutoff = objectstore_item_get_double(item, "lp_cutoff", 200);
  data->reso   = objectstore_item_get_double(item, "lp_reso", 0.5);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "lp_cutoff", data->cutoff);
  objectstore_item_set_double(item, "lp_reso", data->reso);
}

#define NUM_COEFFS 6

/**
 * \brief Calculate the coefficients from the state in data
 *        And emit them.
 *
 * \param g The Generator we send from.
 * \param event The event we received.
 *
 * That the event is passed here is only for convenience.
 * i change the type of the event from AE_NUMBER to AE_NUMARRAY
 * this makes eventq_free() believe it must free the number array also
 * so i change the type back to AE_NONE... 
 *
 * This is to remove the segfault for now.
 * but i dont know how i should handle this in the future.
 */


PRIVATE void calc_coeffs_and_send(Generator *g, AEvent *event) {
  Data *data = g->data;
  gdouble cutoff_ratio = data->cutoff / SAMPLE_RATE;
  gdouble tmp;
  gdouble coeffs[NUM_COEFFS];

  gen_init_aevent(event, AE_DBLARRAY, NULL, 0, NULL, 0, event->time);
  
  tmp = -M_PI * cutoff_ratio / data->reso;

  coeffs[1] = 2 * cos(2 * M_PI * cutoff_ratio) * exp(tmp);
  coeffs[2]= -exp(2 * tmp);
  coeffs[0] = 1 - coeffs[1] - coeffs[2];
  
  coeffs[3] = 1.0;
      coeffs[4] = coeffs[5] = 0;

  event->d.darray.len = NUM_COEFFS;

  event->d.darray.numbers = coeffs;


  gen_send_events(g, EVT_COEFFS, -1, event);
  event->kind = AE_NONE;
}

PRIVATE void evt_cutoff_handler(Generator *g, AEvent *event) {
    Data *data = g->data;

    data->cutoff = event->d.number;

    calc_coeffs_and_send( g, event );
    
  //event->d.number += ((Data *) g->data)->addend;
  //gen_send_events(g, EVT_OUTPUT, -1, event);
}

PRIVATE void evt_reso_handler(Generator *g, AEvent *event) {
    Data *data = g->data;

    data->reso = event->d.number;
    
    calc_coeffs_and_send( g, event );
}

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
//  { CONTROL_KIND_KNOB, "bias", -1,1,0.1,0.01, 0,TRUE, TRUE,EVT_ADDEND,
//    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, addend) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_CUTOFF, "Cutoff", evt_cutoff_handler);
  gen_configure_event_input(k, EVT_RESO, "Reso", evt_reso_handler);
  gen_configure_event_output(k, EVT_COEFFS, "Coefficient");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_lowpass(void) {
  setup_class();
}
