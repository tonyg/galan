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

#define GENERATOR_CLASS_NAME	"filter_diff"
#define GENERATOR_CLASS_PATH	"Filter/Differentiator Coeffs"

#define EVT_TYPE		0
#define NUM_EVENT_INPUTS	1

#define EVT_COEFFS		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
} Data;



#define NUM_COEFFS 4

/**
 * \brief Calculate the coefficients for a normal Integrator
 *        And emit them.
 *
 * \param g The Generator we send from.
 * \param event The event we received.
 *
 * The intgrator has its coefficients [1,1,1,0] 
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
  gdouble coeffs[NUM_COEFFS] = { 1,0,1,-1 };

  gen_init_aevent(event, AE_DBLARRAY, NULL, 0, NULL, 0, event->time);
  
  event->d.darray.len = NUM_COEFFS;
  event->d.darray.numbers = coeffs;

  gen_send_events(g, EVT_COEFFS, -1, event);
  event->kind = AE_NONE;
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
					     NULL, NULL, NULL, NULL); 

  gen_configure_event_input(k, EVT_TYPE, "Type", calc_coeffs_and_send);
  gen_configure_event_output(k, EVT_COEFFS, "Coefficients");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_diff(void) {
  setup_class();
}
