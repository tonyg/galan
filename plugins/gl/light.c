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
#include <GL/gl.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

PRIVATE gboolean lights[8];

enum EVT_INPUTS {
  EVT_RED = 0,
  EVT_GREEN,
  EVT_BLUE,
  EVT_ALPHA,

  NUM_EVT_INPUTS
};

typedef struct Data {
  gint32 lightnr;
  gdouble red,green,blue;
  gdouble alpha;
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
    GLfloat col[4];

    RETURN_VAL_UNLESS( (data->lightnr>=0) && (data->lightnr<8), FALSE );
    
    col[0] = data->red;
    col[1] = data->green;
    col[2] = data->blue;
    col[3] = data->alpha;

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

  data->red = data->green = data->blue = 0.6;
  data->alpha = 1;

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
  data->red = objectstore_item_get_double(item, "red", 0.6);
  data->green = objectstore_item_get_double(item, "green", 0.6);
  data->blue = objectstore_item_get_double(item, "blue", 0.6);
  data->alpha = objectstore_item_get_double(item, "alpha", 1);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;

  objectstore_item_set_double(item, "red", data->red);
  objectstore_item_set_double(item, "green", data->green);
  objectstore_item_set_double(item, "blue", data->blue);
  objectstore_item_set_double(item, "alpha", data->alpha);
}

PRIVATE void evt_red_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->red = event->d.number;

  gen_update_controls( g, 0 );
}

PRIVATE void evt_green_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->green = event->d.number;

  gen_update_controls( g, 1 );
}

PRIVATE void evt_blue_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->blue = event->d.number;

  gen_update_controls( g, 2 );
}

PRIVATE void evt_alpha_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->alpha = event->d.number;

  gen_update_controls( g, 3 );
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "light", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "Red", 0,1,0.01,0.01, 0,TRUE, TRUE,EVT_RED,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, red) },
  { CONTROL_KIND_KNOB, "Green", 0,1,0.01,0.01, 0,TRUE, TRUE,EVT_GREEN,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, green) },
  { CONTROL_KIND_KNOB, "Blue", 0,1,0.01,0.01, 0,TRUE, TRUE,EVT_BLUE,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, blue) },
  { CONTROL_KIND_KNOB, "Alpha(?)", 0,1,0.01,0.01, 0,TRUE, TRUE,EVT_ALPHA,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, alpha) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("gllight", FALSE, NUM_EVT_INPUTS, 0,
					     NULL, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input( k, EVT_RED, "Red", evt_red_handler );
  gen_configure_event_input( k, EVT_GREEN, "Green", evt_green_handler );
  gen_configure_event_input( k, EVT_BLUE, "Blue", evt_blue_handler );
  gen_configure_event_input( k, EVT_ALPHA, "Alpha", evt_alpha_handler );
  
  gencomp_register_generatorclass(k, FALSE, "GL/light", NULL, NULL);
}

PUBLIC void init_plugin_light(void) {
    int i;

    for( i=0; i<8; i++ )
	lights[i] = FALSE;
    
    setup_class();
}

