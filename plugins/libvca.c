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

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define GENERATOR_CLASS_NAME	"vca"
#define GENERATOR_CLASS_PATH	"Levels/Envelope Gain"

#define SIG_ENVELOPE		0
#define SIG_INPUT		1

#define SIG_OUTPUT		0

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  SAMPLE tmpbuf[MAXIMUM_REALTIME_STEP];
  int i;

  if (!gen_read_realtime_input(g, SIG_ENVELOPE, -1, tmpbuf, buflen) ||
      !gen_read_realtime_input(g, SIG_INPUT, -1, buf, buflen))
    return FALSE;

  for (i = 0; i < buflen; i++)
    buf[i] *= tmpbuf[i];
  return TRUE;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Envelope", SIG_FLAG_REALTIME },
  { "Input", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 0, 0,
					     input_sigs, output_sigs, NULL,
					     NULL, NULL, NULL, NULL);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin(void) {
  setup_class();
}
