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
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define GENERATOR_CLASS_NAME	"patloop"
#define GENERATOR_CLASS_PATH	"Pattern/Pattern Sequencer"

#define SEQUENCE_LENGTH		32
#define NUM_PATTERNS		16

#define EVT_LEN_IN		0
#define EVT_EDIT_IN		1
#define EVT_PLAY_IN		2
#define EVT_CLOCK_IN		3
#define EVT_RESYNC_IN		4
#define EVT_TRANSPORT_IN	5
#define NUM_EVENT_INPUTS	6

#define EVT_LEN_OUT		0
#define EVT_PLAY_OUT		1
#define EVT_STEP_OUT		2
#define NUM_EVENT_OUTPUTS	3

#define PATLOOP_CONTROL_PANEL	0

typedef struct Data {
  int edit, play, next_play;
  GList *sequence[NUM_PATTERNS];
  int step;

  int seq_step;
  int length;

  gboolean list_refresh_needed;
} Data;

typedef struct PanelData {
  GtkCList *list;
  GtkEntry *stepindicator;
  GtkAdjustment *lengthadj;
} PanelData;

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i;

  g->data = data;

  data->edit = data->play = data->next_play = 0;
  data->step = 0;
  data->seq_step = 0;
  data->length = SEQUENCE_LENGTH;
  data->list_refresh_needed = TRUE;

  for (i = 0; i < NUM_PATTERNS; i++)
    data->sequence[i] = NULL;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;
  int i;

  for (i = 0; i < NUM_PATTERNS; i++)
    g_list_free(data->sequence[i]);

  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  ObjectStoreDatum *array;
  int i;

  g->data = data;

  data->edit = objectstore_item_get_integer(item, "patloop_edit", 0);
  data->play = objectstore_item_get_integer(item, "patloop_play", 0);
  data->next_play = data->play;
  data->step = objectstore_item_get_integer(item, "patloop_step", 0);
  data->seq_step = objectstore_item_get_integer(item, "patloop_seq_step", 0);
  data->length = objectstore_item_get_integer(item, "patloop_length", SEQUENCE_LENGTH);
  data->list_refresh_needed = TRUE;
  array = objectstore_item_get(item, "patloop_sequences");

  if (array != NULL) {
    for (i = 0; i < NUM_PATTERNS; i++) {
      ObjectStoreDatum *seq = objectstore_datum_array_get(array, i);
      int len = objectstore_datum_array_length(seq);
      int j;

      data->sequence[i] = NULL;

      for (j = 0; j < len; j++) {
	ObjectStoreDatum *datum = objectstore_datum_array_get(seq, j);

	data->sequence[i] = g_list_append(data->sequence[i],
					  (gpointer) objectstore_datum_integer_value(datum));
      }
    }
  } else {
    for (i = 0; i < NUM_PATTERNS; i++)
      data->sequence[i] = NULL;
  }
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_PATTERNS);
  int i;

  objectstore_item_set_integer(item, "patloop_edit", data->edit);
  objectstore_item_set_integer(item, "patloop_play", data->play);
  objectstore_item_set_integer(item, "patloop_step", data->step);
  objectstore_item_set_integer(item, "patloop_seq_step", data->seq_step);
  objectstore_item_set_integer(item, "patloop_length", data->length);
  objectstore_item_set(item, "patloop_sequences", array);

  for (i = 0; i < NUM_PATTERNS; i++) {
    GList *curr = data->sequence[i];
    int len = g_list_length(curr);
    int j;
    ObjectStoreDatum *seq = objectstore_datum_new_array(len);

    objectstore_datum_array_set(array, i, seq);

    for (j = 0; j < len; j++, curr = g_list_next(curr))
      objectstore_datum_array_set(seq, j, objectstore_datum_new_integer((int) curr->data));
  }
}

PRIVATE void evt_len_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int len = GEN_DOUBLE_TO_INT(event->d.number);

  len = MIN(SEQUENCE_LENGTH, MAX(1, len));
  data->length = len;

  event->d.number = len;
  gen_send_events(g, EVT_LEN_OUT, -1, event);
}

PRIVATE void evt_edit_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->edit = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
  data->list_refresh_needed = TRUE;
  gen_update_controls(g, PATLOOP_CONTROL_PANEL);
}

PRIVATE void evt_play_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  data->next_play = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
}

PRIVATE void evt_clock_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  gen_update_controls(g, PATLOOP_CONTROL_PANEL);

  if (data->seq_step == 0) {
    GList *entry = g_list_nth(data->sequence[data->play], data->step);

    if (entry != NULL) {
      event->d.number = (int) entry->data;
      gen_send_events(g, EVT_PLAY_OUT, -1, event);
    }
  }

  event->d.number = data->seq_step;
  gen_send_events(g, EVT_STEP_OUT, -1, event);

  data->seq_step++;

  if (data->seq_step >= data->length) {
    data->seq_step = 0;
    data->step++;

    if (data->step >= g_list_length(data->sequence[data->play])) {
      data->step = 0;
      data->play = data->next_play;
    }
  }
}

PRIVATE void evt_resync_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->play = data->next_play;
  data->step = 0;
  data->seq_step = 0;
}

PRIVATE void evt_transport_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int tmp;


  data->seq_step = ((int)event->d.number) % (data->length ? data->length : 1);
  data->step = (((int)event->d.number)/data->length) % ((tmp = g_list_length(data->sequence[data->play])) ? tmp : 1);

  gen_update_controls(g, PATLOOP_CONTROL_PANEL);

  if (data->seq_step == 0) {
    GList *entry = g_list_nth(data->sequence[data->play], data->step);

    if (entry != NULL) {
      event->d.number = (int) entry->data;
      gen_send_events(g, EVT_PLAY_OUT, -1, event);
    }
  }

  event->d.number = data->seq_step;
  gen_send_events(g, EVT_STEP_OUT, -1, event);

  //data->seq_step++;

  if (data->seq_step == 0) {

    if (data->step == 0) {
      data->play = data->next_play;
    }
  }
}
PRIVATE void entry_adder(gpointer data, gpointer user_data) {
  int num = (int) data;
  GtkCList *list = user_data;
  gchar buf[32];
  gchar *texts[2] = { buf, NULL };

  sprintf(buf, "%d", num);
  gtk_clist_append(list, texts);
}

PRIVATE void update_list(GtkCList *list) {
  Generator *g = gtk_object_get_user_data(GTK_OBJECT(list));
  Data *data = g->data;

  if (data->list_refresh_needed) {
    gtk_clist_freeze(list);
    gtk_clist_unselect_all(list);
    gtk_clist_clear(list);
    g_list_foreach(data->sequence[data->edit], entry_adder, list);
    gtk_clist_thaw(list);
    data->list_refresh_needed = FALSE;
  }
}

PRIVATE void add_entry(GtkWidget *widget, GtkEntry *entry) {
  Generator *g = gtk_object_get_user_data(GTK_OBJECT(entry));
  Data *data = g->data;
  const char *text = gtk_entry_get_text(entry);
  int num;

  if (sscanf(text, "%d", &num) != 1)
    return;

  //gtk_entry_set_text(entry, "");

  data->sequence[data->edit] = g_list_append(data->sequence[data->edit], (gpointer) num);
  data->list_refresh_needed = TRUE;
  update_list(GTK_CLIST(gtk_object_get_data(GTK_OBJECT(entry), "List")));
}

PRIVATE gint find_row(gconstpointer data, gconstpointer user_data) {
  return !(data == user_data);
}

PRIVATE void del_entry(GtkWidget *widget, GtkCList *list) {
  Generator *g = gtk_object_get_user_data(GTK_OBJECT(list));
  Data *data = g->data;
  GList *newlist = NULL;
  GList *oldlist = data->sequence[data->edit];
  GList *curr = oldlist;
  GList *selection = list->selection;
  int len = g_list_length(oldlist);
  int i;

  for (i = 0; i < len; i++, curr = g_list_next(curr)) {
    GList *cell = g_list_find_custom(selection, (gpointer) i, find_row);

    if (cell == NULL)
      newlist = g_list_append(newlist, curr->data);
  }

  data->sequence[data->edit] = newlist;
  g_list_free(oldlist);
  data->list_refresh_needed = TRUE;
  update_list(list);
}

PRIVATE void length_changed(GtkAdjustment *adj, Generator *g) {
  Data *data = g->data;
  AEvent e;

  data->length = adj->value;

  gen_init_aevent(&e, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime());
  e.d.number = data->length;
  gen_send_events(g, EVT_LEN_OUT, -1, &e);
}

PRIVATE void up_clicked(GtkWidget *widget, GtkCList *list) {
  /* %%% */
}

PRIVATE void down_clicked(GtkWidget *widget, GtkCList *list) {
  /* %%% */
}

PRIVATE void init_panel(Control *control) {
  PanelData *pdata = safe_malloc(sizeof(PanelData));
  GtkWidget *ovb = gtk_vbox_new(FALSE, 5);
  GtkWidget *ivb = gtk_vbox_new(FALSE, 5);
  GtkWidget *thb = gtk_hbox_new(FALSE, 5);
  GtkWidget *bhb = gtk_hbox_new(TRUE, 5);
  GtkWidget *ihb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Sequence Length:");
  GtkWidget *stepindicator = gtk_entry_new();
  GtkWidget *spin;
  GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
  GtkWidget *list = gtk_clist_new(1);
  GtkWidget *entry = gtk_entry_new();
  GtkWidget *addbutton = gtk_button_new_with_label("Add");
  GtkWidget *delbutton = gtk_button_new_with_label("Remove");
  GtkWidget *upbutton = gtk_button_new_with_label("Move up");
  GtkWidget *downbutton = gtk_button_new_with_label("Move down");
  GtkAdjustment *adj = GTK_ADJUSTMENT(gtk_adjustment_new(SEQUENCE_LENGTH,
							 1,
							 SEQUENCE_LENGTH,
							 1,
							 SEQUENCE_LENGTH,
							 0));

  spin = gtk_spin_button_new(adj, 0, 0);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_widget_set_usize(entry, 32, 0);

  gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_MULTIPLE);
  gtk_clist_column_titles_hide(GTK_CLIST(list));
  gtk_clist_set_column_width(GTK_CLIST(list), 0, 32);

  gtk_box_pack_start(GTK_BOX(thb), label, FALSE, FALSE, 0);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(thb), spin, FALSE, FALSE, 0);
  gtk_widget_show(spin);

  gtk_widget_set_usize( stepindicator, 50, 20 );
  gtk_entry_set_text( GTK_ENTRY( stepindicator ), "00:00" );
  gtk_entry_set_editable( GTK_ENTRY( stepindicator ), FALSE );

  gtk_box_pack_start(GTK_BOX(thb), stepindicator, FALSE, FALSE, 0);
  gtk_widget_show(stepindicator);

  gtk_box_pack_start(GTK_BOX(ihb), entry, FALSE, FALSE, 0);
  gtk_widget_show(entry);
  gtk_box_pack_start(GTK_BOX(ihb), addbutton, FALSE, FALSE, 0);
  gtk_widget_show(addbutton);

  gtk_box_pack_start(GTK_BOX(ivb), ihb, FALSE, FALSE, 0);
  gtk_widget_show(ihb);
  gtk_box_pack_start(GTK_BOX(ivb), delbutton, FALSE, FALSE, 0);
  gtk_widget_show(delbutton);
  gtk_box_pack_start(GTK_BOX(ivb), upbutton, FALSE, FALSE, 0);
  gtk_widget_show(upbutton);
  gtk_box_pack_start(GTK_BOX(ivb), downbutton, FALSE, FALSE, 0);
  gtk_widget_show(downbutton);

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), list);
  gtk_widget_show(list);

  gtk_box_pack_start(GTK_BOX(bhb), sw, FALSE, FALSE, 0);
  gtk_widget_show(sw);
  gtk_box_pack_start(GTK_BOX(bhb), ivb, FALSE, FALSE, 0);
  gtk_widget_show(ivb);

  gtk_box_pack_start(GTK_BOX(ovb), thb, FALSE, FALSE, 0);
  gtk_widget_show(thb);
  gtk_box_pack_start(GTK_BOX(ovb), bhb, FALSE, FALSE, 0);
  gtk_widget_show(bhb);

  gtk_object_set_user_data(GTK_OBJECT(entry), control->g);
  gtk_object_set_data(GTK_OBJECT(entry), "List", list);
  gtk_signal_connect(GTK_OBJECT(entry), "activate",
		     GTK_SIGNAL_FUNC(add_entry), entry);
  gtk_signal_connect(GTK_OBJECT(addbutton), "clicked",
		     GTK_SIGNAL_FUNC(add_entry), entry);

  gtk_object_set_user_data(GTK_OBJECT(delbutton), control->g);
  gtk_signal_connect(GTK_OBJECT(delbutton), "clicked",
		     GTK_SIGNAL_FUNC(del_entry), list);

  gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		     GTK_SIGNAL_FUNC(length_changed), control->g);

  gtk_object_set_user_data(GTK_OBJECT(upbutton), control->g);
  gtk_signal_connect(GTK_OBJECT(upbutton), "clicked",
		     GTK_SIGNAL_FUNC(up_clicked), list);

  gtk_object_set_user_data(GTK_OBJECT(downbutton), control->g);
  gtk_signal_connect(GTK_OBJECT(downbutton), "clicked",
		     GTK_SIGNAL_FUNC(down_clicked), list);

  gtk_object_set_user_data(GTK_OBJECT(list), control->g);

  update_list(GTK_CLIST(list));

  pdata->list = GTK_CLIST(list);
  pdata->stepindicator = GTK_ENTRY(stepindicator);
  pdata->lengthadj = adj;

  control->widget = ovb;
  control->data = pdata;
}

PRIVATE void done_panel(Control *control) {
  free(control->data);
}

PRIVATE void refresh_panel(Control *control) {
  PanelData *pdata = control->data;
  Generator *g = control->g;
  Data *data = g->data;
  char text[32];

  update_list(pdata->list);

  sprintf(text, "%02d:%02d", data->step, data->seq_step);
  gtk_entry_set_text(pdata->stepindicator, text);
  gtk_adjustment_set_value(pdata->lengthadj, data->length);
}

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_USERDEF, "panel", 0,0,0,0, 0,FALSE, 0,0, init_panel, done_panel, refresh_panel },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_LEN_IN, "Length In", evt_len_handler);
  gen_configure_event_input(k, EVT_EDIT_IN, "Edit", evt_edit_handler);
  gen_configure_event_input(k, EVT_PLAY_IN, "Play", evt_play_handler);
  gen_configure_event_input(k, EVT_CLOCK_IN, "Clock", evt_clock_handler);
  gen_configure_event_input(k, EVT_RESYNC_IN, "Resync", evt_resync_handler);
  gen_configure_event_input(k, EVT_TRANSPORT_IN, "Transport", evt_transport_handler);
  gen_configure_event_output(k, EVT_LEN_OUT, "Length Out");
  gen_configure_event_output(k, EVT_PLAY_OUT, "Play");
  gen_configure_event_output(k, EVT_STEP_OUT, "Step");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_patloop(void) {
  setup_class();
}
