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

#define GENERATOR_CLASS_NAME	"reverse"
#define GENERATOR_CLASS_PATH	"Timebase/Reverse"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  return gen_get_randomaccess_input_range(g, SIG_INPUT, 0);
}

PRIVATE gboolean output_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  SAMPLETIME inoffset;
  SAMPLETIME len = gen_get_randomaccess_input_range(g, SIG_INPUT, 0);
  int inbuflen = buflen;
  SAMPLE *inbuf = buf;
  int i, j;

  inoffset = len - offset - buflen;

  if (inoffset <= -buflen)
    return FALSE;

  if (inoffset < 0) {
    memset(buf, 0, -inoffset * sizeof(SAMPLE));
    inbuflen += inoffset;		/* *decrease* inbuflen */
    inbuf += (-inoffset);		/* *advance* inbuf */
    inoffset = 0;
  }

  if (!gen_read_randomaccess_input(g, SIG_INPUT, 0, inoffset, inbuf, inbuflen))
    return FALSE;

  for (i = 0, j = (buflen - 1); i < j; i++, j--) {
    SAMPLE tmp = buf[i];
    buf[i] = buf[j];
    buf[j] = tmp;
  }

  return TRUE;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_RANDOMACCESS },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, output_generator } } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 0, 0,
					     input_sigs, output_sigs, controls,
					     NULL, NULL, NULL, NULL);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_reverse(void) {
  setup_class();
}
