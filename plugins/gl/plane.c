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

PRIVATE gboolean render_function(Generator *g ) {
  //int i;

  glColor3f(1,1,0);
  glNormal3f( 0,0,1 );
  glBegin(GL_QUADS);
  glVertex3f(-1,-1,0);
  glVertex3f(-1,1,0);
  glVertex3f(1,1,0);
  glVertex3f(1,-1,0);
  glEnd();

  return TRUE;
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "plane", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("glplane", FALSE, 0, 0,
					     NULL, output_sigs, NULL,
					     NULL, NULL, NULL, NULL);

  gencomp_register_generatorclass(k, FALSE, "GL/plane", NULL, NULL);
}

PUBLIC void init_plugin_plane(void) {
  setup_class();
}

