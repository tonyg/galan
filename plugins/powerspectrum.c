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


#define GENERATOR_CLASS_NAME	"power_spec"
#define GENERATOR_CLASS_PATH	"Misc/PowerSpectrum"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_INPUT		0
#define NUM_EVENT_INPUTS	1

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
} Data;

PRIVATE int init_instance(Generator *g) {
  //Data *data = safe_malloc(sizeof(Data));
  //g->data = data;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {

    //Data *data = g->data;
    
    //free(data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  //Data *data = safe_malloc(sizeof(Data));

  //g->data = data;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  //Data *data = g->data;
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
  //Data *data = g->data;
  int i,len;
  SAMPLE *out, *in;
  AEvent send_ev;

  RETURN_UNLESS( event->kind == AE_NUMARRAY );
  //RETURN_UNLESS( (event->d.array.len & 1) == 0 );

  in = event->d.array.numbers;
  len = event->d.array.len;

  out = g_alloca( sizeof( SAMPLE ) * (len/2+1) );
  
  out[0] = in[0] * in [0];
  for( i=1; i<(len+1)/2; i++ )
      out[i] = in[i]*in[i] + in[len-i] * in[len-i];

  if( (len & 1) == 0 )
      out[len/2] = in[len/2] * in[len/2];
  

  gen_init_aevent(&send_ev, AE_NUMARRAY, NULL, 0, NULL, 0, event->time);
  
  send_ev.d.array.len = len/2+1;
  send_ev.d.array.numbers = out;


  gen_send_events(g, EVT_OUTPUT, -1, &send_ev);
}

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, },
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output" );

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_powerspectrum(void) {
  setup_class();
}
