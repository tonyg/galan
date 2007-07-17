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

#define POLYPHONY	4

typedef struct Data {
  SAMPLETIME location[POLYPHONY];
  int num_playing;
} Data;

#define LOCATION(g)	((SAMPLETIME) ((g)->data))

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));

  data->num_playing = 0;
  /* location[] doesn't need initialising because num_playing is zero. */

  g->data = data;
  return TRUE;
}

PRIVATE void done_instance(Generator *g) {
  free(g->data);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  SAMPLE tmpbuf[MAXIMUM_REALTIME_STEP];
  SAMPLETIME range;
  int i;

  if (data->num_playing == 0)
    return FALSE;

  memset(buf, 0, sizeof(SAMPLE) * buflen);
  range = gen_get_randomaccess_input_range(g, 0, 0);

  i = 0;
  while (i < data->num_playing) {
    if (gen_read_randomaccess_input(g, 0, 0, data->location[i], tmpbuf, buflen)) {
      int j;
      for (j = 0; j < buflen; j++)
	buf[j] += tmpbuf[j];
    }

    data->location[i] += buflen;

    if (data->location[i] >= range) {
      data->num_playing--;
      data->location[i] = data->location[data->num_playing];
    } else {
      i++;
    }
  }

  return TRUE;
}

PRIVATE void evt_reset_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  if (data->num_playing >= POLYPHONY) {
    int i;

    for (i = 0; i < POLYPHONY - 1; i++) {
      data->location[i] = data->location[i+1];
    }

    data->location[POLYPHONY - 1] = 0;
  } else {
    data->location[data->num_playing++] = 0;
  }
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_RANDOMACCESS },
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
  GeneratorClass *k = gen_new_generatorclass("rart", FALSE, 1, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, done_instance,
					     (AGenerator_pickle_t) init_instance, NULL);

  gen_configure_event_input(k, 0, "Reset", evt_reset_handler);

  gencomp_register_generatorclass(k, FALSE, "Misc/Randomaccess to Realtime Converter", NULL, NULL);
}

PUBLIC void init_plugin_rart(void) {
  setup_class();
}
