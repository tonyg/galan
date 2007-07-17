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

#define GENERATOR_CLASS_NAME	"sel"
#define GENERATOR_CLASS_PATH	"Pattern/Pattern Selector"
#define NUM_ROWS	4
#define NUM_COLS	4

#define EVT_OUTPUT 0

typedef struct Data {
  int pattern;
  GtkWidget *buttons[NUM_ROWS * NUM_COLS];
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->pattern = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->pattern = objectstore_item_get_integer(item, "patsel_pattern", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "patsel_pattern", data->pattern);
}

PRIVATE void button_clicked(GtkWidget *button, gpointer userdata) {
  Control *c = gtk_object_get_data(GTK_OBJECT(button), "Control");
  Data *data = c->g->data;
  int index = (int) userdata;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) && data->pattern != index) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->buttons[data->pattern]), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->buttons[index]), TRUE);
    data->pattern = index;
    control_emit(c, data->pattern);
    gen_update_controls(c->g, 0);
  }
}

PRIVATE void ctrl_init(Control *c) {
  GtkWidget *vbox = gtk_vbox_new(TRUE, 0);
  Data *data = c->g->data;
  int i;
  char curr[2] = { 'A', '\0' };

  for (i = 0; i < NUM_ROWS; i++) {
    GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
    int j;

    for (j = 0; j < NUM_COLS; j++, curr[0]++) {
      GtkWidget *button = gtk_toggle_button_new_with_label(curr);
      int index = i * NUM_COLS + j;

      if (index == data->pattern)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

      data->buttons[index] = button;

      gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
      gtk_widget_show(button);
      gtk_object_set_data(GTK_OBJECT(button), "Control", c);
      gtk_signal_connect(GTK_OBJECT(button), "toggled",
			 GTK_SIGNAL_FUNC(button_clicked), (gpointer) index);
    }

    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);
  }

  c->widget = vbox;
}

PRIVATE void ctrl_done(Control *c) {
}

PRIVATE void ctrl_refresh(Control *c) {
  Data *data = c->g->data;
  int i;

  for (i = 0; i < NUM_ROWS * NUM_COLS; i++) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->buttons[i]),
				 data->pattern == i);
  }
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_USERDEF, "selector", 0,0,0,0, 0,FALSE, 0,EVT_OUTPUT,
    ctrl_init, ctrl_done, ctrl_refresh },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 0, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_output(k, EVT_OUTPUT, "Pattern Number");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_patsel(void) {
  setup_class();
}
