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

#define SEQUENCE_LENGTH	32
#define NUM_PATTERNS	16

#define GENERATOR_CLASS_NAME	"trigseq"
#define GENERATOR_CLASS_PATH	"Pattern/Trigger Sequence"

#define EVT_SEL_EDIT	0	/* input */
#define EVT_SEL_PLAY	1	/* input */
#define EVT_STEP	2	/* input */
#define EVT_OUTPUT	0	/* output */

/* Forward refs. */
PRIVATE void init_pattern(Control *control);
PRIVATE void done_pattern(Control *control);
PRIVATE void refresh_pattern(Control *control);

#define TRIGSEQ_CONTROL_PATTERN		0
PRIVATE ControlDescriptor trigseq_controls[] = {
  { CONTROL_KIND_USERDEF, "pattern", 0,0,0,0, 0,FALSE, 0,0, init_pattern, done_pattern, refresh_pattern },
  { CONTROL_KIND_NONE, }
};

typedef struct Data {
  int edit, play;
  gboolean pattern[NUM_PATTERNS][SEQUENCE_LENGTH];
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i, j;

  g->data = data;

  data->edit = data->play = 0;

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      data->pattern[i][j] = FALSE;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  ObjectStoreDatum *array;
  int i, j;

  g->data = data;

  data->edit = objectstore_item_get_integer(item, "trigseq_edit", 0);
  data->play = objectstore_item_get_integer(item, "trigseq_play", 0);
  array = objectstore_item_get(item, "trigseq_patterns");

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      data->pattern[i][j] =
	objectstore_datum_integer_value(
	  objectstore_datum_array_get(array, i * SEQUENCE_LENGTH + j)
	);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_PATTERNS * SEQUENCE_LENGTH);
  int i, j;

  objectstore_item_set_integer(item, "trigseq_edit", data->edit);
  objectstore_item_set_integer(item, "trigseq_play", data->play);
  objectstore_item_set(item, "trigseq_patterns", array);

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      objectstore_datum_array_set(array, i * SEQUENCE_LENGTH + j,
				  objectstore_datum_new_integer(data->pattern[i][j]));
}

PRIVATE void evt_sel_edit_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->edit = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
  gen_update_controls(g, TRIGSEQ_CONTROL_PATTERN);
}

PRIVATE void evt_sel_play_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->play = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
}

PRIVATE void evt_step_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int step = GEN_DOUBLE_TO_INT(event->d.number) % SEQUENCE_LENGTH;

  if (data->pattern[data->play][step]) {
    gen_init_aevent(event, AE_NUMBER, NULL, 0, NULL, 0, event->time);
    event->d.number = step;
    gen_send_events(g, EVT_OUTPUT, -1, event);
  }
}

PRIVATE void toggled_handler(GtkWidget *widget, gpointer userdata) {
  int step = (int) userdata;
  Control *c = gtk_object_get_data(GTK_OBJECT(widget), "Control");
  Data *data = c->g->data;

  if (c->events_flow) {
    data->pattern[data->edit][step] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    gen_update_controls(c->g, TRIGSEQ_CONTROL_PATTERN);
  }
}

PRIVATE void init_pattern(Control *control) {
  GtkWidget *hb;
  int i;
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * SEQUENCE_LENGTH);

  hb = gtk_hbox_new(TRUE, 5);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    GtkWidget *b = gtk_toggle_button_new();
    gtk_object_set_data(GTK_OBJECT(b), "Control", control);
    gtk_signal_connect(GTK_OBJECT(b), "toggled", GTK_SIGNAL_FUNC(toggled_handler), (gpointer) i);
    gtk_box_pack_start(GTK_BOX(hb), b, FALSE, FALSE, 0);
    gtk_widget_set_usize(b, 12, 16);
    gtk_widget_show(b);
    widgets[i] = b;
  }

  control->widget = hb;
  control->data = widgets;
}

PRIVATE void done_pattern(Control *control) {
  free(control->data);
}

PRIVATE void refresh_pattern(Control *control) {
  Data *data = control->g->data;
  GtkWidget **widgets = control->data;
  int i;

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets[i]),
				 data->pattern[data->edit][i]);
  }
}

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 3, 1,
					     NULL, NULL, trigseq_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_SEL_EDIT, "Edit Seq", evt_sel_edit_handler);
  gen_configure_event_input(k, EVT_SEL_PLAY, "Play Seq", evt_sel_play_handler);
  gen_configure_event_input(k, EVT_STEP, "Step", evt_step_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_trigseq(void) {
  setup_class();
}
