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

PRIVATE gboolean lights[8];

typedef struct Data {
  gint32 lightnr;
} Data;

PRIVATE gint32 alloc_light_number( void ) {
    int i;
    for( i=0; i<8; i++ )
	if( !lights[i] ) {
	    lights[i] = TRUE;
	    return i;
	}

    return -1;
}

PRIVATE void free_light_number( gint32 i ) {
    
    RETURN_UNLESS( (i>=0) && (i<8) );
    lights[i] = FALSE;
}
    
PRIVATE gboolean render_function(Generator *g ) {

    Data *data = g->data;
    GLfloat pos[4] = { 0,0,0,1 };
    GLfloat col[4] = { 2,2,2,0 };

    RETURN_VAL_UNLESS( (data->lightnr>=0) && (data->lightnr<8), FALSE );
    
    glLightfv(GL_LIGHT0 + data->lightnr, GL_POSITION, pos);
    glLightfv(GL_LIGHT0 + data->lightnr, GL_DIFFUSE,  col);  
    
    glEnable( GL_LIGHT0 + data->lightnr );

    return TRUE;
}


PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->lightnr = alloc_light_number();
  if( data->lightnr == -1 ) {
      free( g->data );
      return FALSE;
  }

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
    free_light_number( data->lightnr );
    free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->lightnr = alloc_light_number();
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "light", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("gllight", FALSE, 0, 0,
					     NULL, output_sigs, NULL,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gencomp_register_generatorclass(k, FALSE, "GL/light", NULL, NULL);
}

PUBLIC void init_plugin_light(void) {
    int i;

    for( i=0; i<8; i++ )
	lights[i] = FALSE;
    
    setup_class();
}

