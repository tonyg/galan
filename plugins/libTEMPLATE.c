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

/* Template gAlan plugin file. Please distribute your plugins according to
   the GNU General Public License! (You don't have to, but I'd prefer it.) */

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

#define GENERATOR_CLASS_NAME	"template"
#define GENERATOR_CLASS_PATH	"Misc/Template Plugin"
#define GENERATOR_CLASS_PIXMAP	"template.xpm"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_INPUT		0
#define NUM_EVENT_INPUTS	1

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1

typedef struct Data {
  int dummy;
} Data;

PRIVATE void setup_tables(void) {
  /* Put any plugin-wide initialisation here. */
}

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->dummy = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->dummy = objectstore_item_get_integer(item, "TEMPLATE_dummy", 123456);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "TEMPLATE_dummy", data->dummy);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;

  memset(buf, 0, buflen * sizeof(SAMPLE));
  return FALSE;
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_INPUT */
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
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
				  NULL);
}

PUBLIC void init_plugin_TEMPLATE(void) {
  setup_tables();
  setup_class();
}
