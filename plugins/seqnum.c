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

#define GENERATOR_CLASS_NAME	"seqnum"
#define GENERATOR_CLASS_PATH	"Pattern/Numeric Sequence"

#define EVT_SEL_EDIT	0	/* input */
#define EVT_SEL_PLAY	1	/* input */
#define EVT_STEP	2	/* input */
#define EVT_OUTPUT	0	/* output */

/* Forward refs. */
PRIVATE void init_pattern(Control *control);
PRIVATE void done_pattern(Control *control);
PRIVATE void refresh_pattern(Control *control);

#define SEQNUM_CONTROL_PATTERN		0
PRIVATE ControlDescriptor seqnum_controls[] = {
  { CONTROL_KIND_USERDEF, "pattern", 0,0,0,0, 0,FALSE, 0,0, init_pattern, done_pattern, refresh_pattern },
  { CONTROL_KIND_NONE, }
};

typedef struct Data {
  int edit, play;
  gdouble pattern[NUM_PATTERNS][SEQUENCE_LENGTH];
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i, j;

  g->data = data;

  data->edit = data->play = 0;

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      data->pattern[i][j] = 0;

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

  data->edit = objectstore_item_get_integer(item, "seqnum_edit", 0);
  data->play = objectstore_item_get_integer(item, "seqnum_play", 0);
  array = objectstore_item_get(item, "seqnum_patterns");

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      data->pattern[i][j] =
	objectstore_datum_double_value(
	  objectstore_datum_array_get(array, i * SEQUENCE_LENGTH + j)
	);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_PATTERNS * SEQUENCE_LENGTH);
  int i, j;

  objectstore_item_set_integer(item, "seqnum_edit", data->edit);
  objectstore_item_set_integer(item, "seqnum_play", data->play);
  objectstore_item_set(item, "seqnum_patterns", array);

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++)
      objectstore_datum_array_set(array, i * SEQUENCE_LENGTH + j,
				  objectstore_datum_new_double(data->pattern[i][j]));
}

PRIVATE void evt_sel_edit_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->edit = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
  gen_update_controls(g, SEQNUM_CONTROL_PATTERN);
}

PRIVATE void evt_sel_play_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->play = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
}

PRIVATE void evt_step_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int step = GEN_DOUBLE_TO_INT(event->d.number) % SEQUENCE_LENGTH;

  gen_init_aevent(event, AE_NUMBER, NULL, 0, NULL, 0, event->time);
  event->d.number = data->pattern[data->play][step];
  gen_send_events(g, EVT_OUTPUT, -1, event);
}

PRIVATE void value_changed_handler(GtkAdjustment *adj, gpointer userdata) {
  int step = (int) userdata;
  Control *c = gtk_object_get_data(GTK_OBJECT(adj), "Control");
  Data *data = c->g->data;

  if (c->events_flow) {
    data->pattern[data->edit][step] = 1 - adj->value;
    gen_update_controls(c->g, SEQNUM_CONTROL_PATTERN);
  }
}

PRIVATE void init_pattern(Control *control) {
  GtkWidget *hb;
  int i;
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * SEQUENCE_LENGTH);
  Data *data = control->g->data;

  hb = gtk_hbox_new(TRUE, 5);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    GtkWidget *b = gtk_vscale_new(NULL);
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(b));

    gtk_scale_set_draw_value(GTK_SCALE(b), FALSE);
    gtk_scale_set_digits(GTK_SCALE(b), 2);

    adj->step_increment = 0.01;
    adj->page_increment = 0.01;
    adj->lower = 0;
    adj->upper = 1;
    adj->value = 1 - data->pattern[data->edit][i];

    gtk_object_set_data(GTK_OBJECT(adj), "Control", control);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		       GTK_SIGNAL_FUNC(value_changed_handler), (gpointer) i);
    gtk_box_pack_start(GTK_BOX(hb), b, FALSE, FALSE, 0);
    gtk_widget_set_usize(b, 12, 100);
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
    gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(widgets[i])),
			     1 - data->pattern[data->edit][i]);
  }
}

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 3, 1,
					     NULL, NULL, seqnum_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_SEL_EDIT, "Edit Seq", evt_sel_edit_handler);
  gen_configure_event_input(k, EVT_SEL_PLAY, "Play Seq", evt_sel_play_handler);
  gen_configure_event_input(k, EVT_STEP, "Step", evt_step_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_seqnum(void) {
  setup_class();
}
