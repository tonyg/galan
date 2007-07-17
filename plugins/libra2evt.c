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

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define SIG_INPUT	0
#define EVT_TRIGGER	0
#define EVT_OUTPUT	0

PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  SAMPLE *buffer;
  SAMPLETIME now = event->time;

  int len = gen_get_randomaccess_input_range( g, SIG_INPUT, 0 );


  if( len != 0 ) {
      buffer = g_alloca( sizeof( SAMPLE ) * len );

      if (!gen_read_randomaccess_input(g, SIG_INPUT, 0, 0, buffer, len))
	  g_warning( "Error\n" );

      gen_init_aevent(event, AE_NUMARRAY, NULL, 0, NULL, 0, now);
      event->d.array.len = len;
      event->d.array.numbers = buffer;
      gen_send_events(g, EVT_OUTPUT, -1, event);
      event->kind = AE_NUMBER;
  }
}


PRIVATE gboolean init_instance(Generator *g) {
  return TRUE;
}

PRIVATE void done_instance(Generator *g) {
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_RANDOMACCESS },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("ra2evt", FALSE, 1, 1,
					     input_sigs, NULL, NULL,
					     init_instance, done_instance,
					     (AGenerator_pickle_t) init_instance, NULL);

  gen_configure_event_input(k, EVT_TRIGGER, "Trigger", evt_trigger_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, "Misc/Randomacces-to-event Converter", NULL, NULL);
}

PUBLIC void init_plugin(void) {
  setup_class();
}
