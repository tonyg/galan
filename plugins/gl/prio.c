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
#include <GL/gl.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"


enum EVT_INPUTS {
  NUM_EVT_INPUTS = 0
};

typedef struct Data {
} Data;

PRIVATE gboolean render_function(Generator *g ) {

    gen_render_gl( g, 0, -1 );
    gen_render_gl( g, 1, -1 );

    return TRUE;
}

PRIVATE int init_instance(Generator *g) {
  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
}




PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input1", SIG_FLAG_OPENGL },
  { "Input2", SIG_FLAG_OPENGL },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("glprio", FALSE, NUM_EVT_INPUTS, 0,
					     input_sigs, output_sigs, NULL,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  
  
  gencomp_register_generatorclass(k, FALSE, "GL/prio", NULL, NULL);
}

PUBLIC void init_plugin_prio(void) {
  setup_class();
}

