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
  gdouble tx, ty, tz;
} Data;

PRIVATE gboolean render_function(Generator *g ) {

    Data *data = g->data;

    glPushMatrix();
    glTranslatef( data->tx, data->ty, data->tz );
    gen_render_gl( g, 0, -1 );
    glPopMatrix();

    return TRUE;
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->tx = 0;
  data->ty = 0;
  data->tz = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->tx = objectstore_item_get_double(item, "trans_x", 0);
  data->ty = objectstore_item_get_double(item, "trans_y", 0);
  data->tz = objectstore_item_get_double(item, "trans_z", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "trans_x", data->tx);
  objectstore_item_set_double(item, "trans_y", data->ty);
  objectstore_item_set_double(item, "trans_z", data->tz);
}

PRIVATE void evt_trans_x_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->tx = event->d.number;

  gen_update_controls( g, -1 );
}

PRIVATE void evt_trans_y_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->ty = event->d.number;

  gen_update_controls( g, -1 );
}

PRIVATE void evt_trans_z_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->tz = event->d.number;

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
  { CONTROL_KIND_KNOB, "TransX", -100,100,1,1, 0,TRUE, TRUE,EVT_TRANSX,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, tx) },
  { CONTROL_KIND_KNOB, "TransY", -100,100,1,1, 0,TRUE, TRUE,EVT_TRANSY,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, ty) },
  { CONTROL_KIND_KNOB, "TransZ", -100,100,1,1, 0,TRUE, TRUE,EVT_TRANSZ,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, tz) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("gltranslate", FALSE, NUM_EVT_INPUTS, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input( k, EVT_TRANSX, "TransX", evt_trans_x_handler );
  gen_configure_event_input( k, EVT_TRANSY, "TransY", evt_trans_y_handler );
  gen_configure_event_input( k, EVT_TRANSZ, "TransZ", evt_trans_z_handler );
  
  
  gencomp_register_generatorclass(k, FALSE, "GL/translate", NULL, NULL);
}

PUBLIC void init_plugin_gltranslate(void) {
  setup_class();
}

