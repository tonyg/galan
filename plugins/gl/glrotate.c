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


enum EVT_INPUTS {
  EVT_TRANSX = 0,
  EVT_TRANSY,
  EVT_TRANSZ,

  NUM_EVT_INPUTS
};

typedef struct Data {
  gdouble rx, ry, rz;
} Data;

PRIVATE gboolean render_function(Generator *g ) {

    Data *data = g->data;

    glPushMatrix();
    glRotatef( data->rx, 1,0,0 );
    glRotatef( data->ry, 0,1,0 );
    glRotatef( data->rz, 0,0,1 );
    gen_render_gl( g, 0, -1 );
    glPopMatrix();

    return TRUE;
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->rx = 0;
  data->ry = 0;
  data->rz = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->rx = objectstore_item_get_double(item, "rot_x", 0);
  data->ry = objectstore_item_get_double(item, "rot_y", 0);
  data->rz = objectstore_item_get_double(item, "rot_z", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "rot_x", data->rx);
  objectstore_item_set_double(item, "rot_y", data->ry);
  objectstore_item_set_double(item, "rot_z", data->rz);
}

PRIVATE void evt_rot_x_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->rx = event->d.number;

  gen_update_controls( g, -1 );
}

PRIVATE void evt_rot_y_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->ry = event->d.number;

  gen_update_controls( g, -1 );
}

PRIVATE void evt_rot_z_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->rz = event->d.number;

  gen_update_controls( g, -1 );
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_OPENGL },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "RotX", 0,360,1,1, 0,TRUE, TRUE,EVT_TRANSX,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, rx) },
  { CONTROL_KIND_KNOB, "RotY", 0,360,1,1, 0,TRUE, TRUE,EVT_TRANSY,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, ry) },
  { CONTROL_KIND_KNOB, "RotZ", 0,360,1,1, 0,TRUE, TRUE,EVT_TRANSZ,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, rz) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("glrotate", FALSE, NUM_EVT_INPUTS, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input( k, EVT_TRANSX, "RotX", evt_rot_x_handler );
  gen_configure_event_input( k, EVT_TRANSY, "RotY", evt_rot_y_handler );
  gen_configure_event_input( k, EVT_TRANSZ, "RotZ", evt_rot_z_handler );
  
  
  gencomp_register_generatorclass(k, FALSE, "GL/rotate", NULL, NULL);
}

PUBLIC void init_plugin_glrotate(void) {
  setup_class();
}

