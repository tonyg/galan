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

#define FREQ_C0	16.351598
#define FREQ_A4 440
#define NOTE_C4 48
#define NOTE_A4 57
#define NOTE_TO_FREQ(n)		(FREQ_A4 * pow(2, ((n) - NOTE_A4) / 12.0))

#define SEQUENCE_LENGTH	32
#define NUM_PATTERNS	16

#define GENERATOR_CLASS_NAME	"seqnote"
#define GENERATOR_CLASS_PATH	"Pattern/Note Sequence"

#define EVT_SEL_EDIT	0	/* input */
#define EVT_SEL_PLAY	1	/* input */
#define EVT_STEP	2	/* input */
#define EVT_OUTPUT	0	/* output */
#define EVT_NOTE_OUT	1	/* output */

/* Forward refs. */
PRIVATE void init_pattern(Control *control);
PRIVATE void init_2oct_pattern(Control *control);
PRIVATE void done_pattern(Control *control);
PRIVATE void refresh_pattern(Control *control);

#define SEQNOTE_CONTROL_PATTERN		0
PRIVATE ControlDescriptor seqnote_controls[] = {
  { CONTROL_KIND_USERDEF, "pattern", 0,0,0,0, 0,FALSE, 0,0, init_pattern, done_pattern, refresh_pattern },
  { CONTROL_KIND_USERDEF, "2 Octaves Pattern", 0,0,0,0, 0,FALSE, 0,0, init_2oct_pattern, done_pattern, refresh_pattern },
  { CONTROL_KIND_NONE, }
};

PRIVATE int clipseq[SEQUENCE_LENGTH];
PRIVATE gboolean clipnote[SEQUENCE_LENGTH];

typedef struct Data {
  int edit, play;
  int pattern[NUM_PATTERNS][SEQUENCE_LENGTH];
  gboolean note[NUM_PATTERNS][SEQUENCE_LENGTH];
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i, j;

  g->data = data;

  data->edit = data->play = 0;

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++) {
      data->pattern[i][j] = NOTE_A4;
      data->note[i][j] = FALSE;
    }

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  ObjectStoreDatum *array;
  ObjectStoreDatum *notes;
  int i, j;

  g->data = data;

  data->edit = objectstore_item_get_integer(item, "seqnote_edit", 0);
  data->play = objectstore_item_get_integer(item, "seqnote_play", 0);
  array = objectstore_item_get(item, "seqnote_patterns");
  notes = objectstore_item_get(item, "seqnote_notes");

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++) {
      data->pattern[i][j] =
	objectstore_datum_integer_value(
	  objectstore_datum_array_get(array, i * SEQUENCE_LENGTH + j)
	);
      data->note[i][j] =
	objectstore_datum_integer_value(
	  objectstore_datum_array_get(notes, i * SEQUENCE_LENGTH + j)
	);
    }
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  ObjectStoreDatum *array = objectstore_datum_new_array(NUM_PATTERNS * SEQUENCE_LENGTH);
  ObjectStoreDatum *notes = objectstore_datum_new_array(NUM_PATTERNS * SEQUENCE_LENGTH);
  int i, j;

  objectstore_item_set_integer(item, "seqnote_edit", data->edit);
  objectstore_item_set_integer(item, "seqnote_play", data->play);
  objectstore_item_set(item, "seqnote_patterns", array);
  objectstore_item_set(item, "seqnote_notes", notes);

  for (i = 0; i < NUM_PATTERNS; i++)
    for (j = 0; j < SEQUENCE_LENGTH; j++) {
      objectstore_datum_array_set(array, i * SEQUENCE_LENGTH + j,
				  objectstore_datum_new_integer(data->pattern[i][j]));
      objectstore_datum_array_set(notes, i * SEQUENCE_LENGTH + j,
				  objectstore_datum_new_integer(data->note[i][j]));
    }
}

PRIVATE void evt_sel_edit_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->edit = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
  gen_update_controls(g, -1);
}

PRIVATE void evt_sel_play_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->play = GEN_DOUBLE_TO_INT(event->d.number) % NUM_PATTERNS;
}

PRIVATE void evt_step_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  int step = GEN_DOUBLE_TO_INT(event->d.number) % SEQUENCE_LENGTH;

  if (data->note[data->play][step]) {
    gen_init_aevent(event, AE_NUMBER, NULL, 0, NULL, 0, event->time);
    event->d.number = NOTE_TO_FREQ(data->pattern[data->play][step]);
    gen_send_events(g, EVT_OUTPUT, -1, event);
    event->d.number = data->pattern[data->play][step];
    gen_send_events(g, EVT_NOTE_OUT, -1, event);
  }
}

PRIVATE void update_label(GtkWidget *label, int note) {
  char text[20];
  int octave = note / 12;
  int tone = note % 12;
  int sharp =
    (tone == 1) || (tone == 3) ||
    (tone == 6) || (tone == 8) || (tone == 10);

  sprintf(text, "%c%s%d\n(%d)", "CCDDEFFGGAAB"[tone], sharp ? "#" : "", octave, note);
  gtk_label_set_text(GTK_LABEL(label), text);
}

PRIVATE void value_changed_handler(GtkAdjustment *adj, gpointer userdata) {
  int step = (int) userdata;
  Control *c = gtk_object_get_data(GTK_OBJECT(adj), "Control");
  GtkWidget **widgets = c->data;
  Data *data = c->g->data;

  if (c->events_flow) {
    int note = 127 - adj->value;
    data->pattern[data->edit][step] = note;
    update_label(widgets[SEQUENCE_LENGTH * 2], note);
    gen_update_controls(c->g, -1);
  }
}

PRIVATE void focus_in_handler(GtkRange *b, GdkEventFocus *event, gpointer userdata) {
  Control *c = userdata;
  GtkAdjustment *adj = gtk_range_get_adjustment(b);
  GtkWidget **widgets = c->data;

  if (c->events_flow) {
    int note = 127 - adj->value;
    update_label(widgets[SEQUENCE_LENGTH * 2], note);
  }
}

PRIVATE void toggle_changed_handler(GtkWidget *widget, gpointer userdata) {
  int step = (int) userdata;
  Control *c = gtk_object_get_data(GTK_OBJECT(widget), "Control");
  Data *data = c->g->data;

  if (c->events_flow) {
    data->note[data->edit][step] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    gen_update_controls(c->g, -1);
  }
}

PRIVATE void do_copy(Control *c, guint action, GtkWidget *widget) {
  Data *data = c->g->data;
  int i, end, offset;

  switch( action ) {
      case 0:
	  i=0; end=SEQUENCE_LENGTH;
	  break;
      case 1:
	  i=0; end=SEQUENCE_LENGTH/2;
	  break;
      case 2:
	  i=SEQUENCE_LENGTH/2; end=SEQUENCE_LENGTH;
	  break;
      case 3:
	  i=0; end=SEQUENCE_LENGTH/4;
	  break;
      case 4:
	  i=SEQUENCE_LENGTH/4; end=SEQUENCE_LENGTH*2/4;
	  break;
      case 5:
	  i=SEQUENCE_LENGTH*2/4; end=SEQUENCE_LENGTH*3/4;
	  break;
      case 6:
	  i=SEQUENCE_LENGTH*3/4; end=SEQUENCE_LENGTH*4/4;
	  break;
      default:
	  i=0; end=SEQUENCE_LENGTH;
	  break;
  }

  for( offset=i; i<end; i++ ) {
      clipseq[i-offset] = data->pattern[data->edit][i];
      clipnote[i-offset] = data->note[data->edit][i];
  }
}

PRIVATE void do_paste(Control *c, guint action, GtkWidget *widget) {
  Data *data = c->g->data;
  int i, end, offset;

  switch( action ) {
      case 0:
	  i=0; end=SEQUENCE_LENGTH;
	  break;
      case 1:
	  i=0; end=SEQUENCE_LENGTH/2;
	  break;
      case 2:
	  i=SEQUENCE_LENGTH/2; end=SEQUENCE_LENGTH;
	  break;
      case 3:
	  i=0; end=SEQUENCE_LENGTH/4;
	  break;
      case 4:
	  i=SEQUENCE_LENGTH/4; end=SEQUENCE_LENGTH*2/4;
	  break;
      case 5:
	  i=SEQUENCE_LENGTH*2/4; end=SEQUENCE_LENGTH*3/4;
	  break;
      case 6:
	  i=SEQUENCE_LENGTH*3/4; end=SEQUENCE_LENGTH*4/4;
	  break;
      default:
	  i=0; end=SEQUENCE_LENGTH;
	  break;
  }

  for( offset=i; i<end; i++ ) {
      data->pattern[data->edit][i] = clipseq[i-offset];
      data->note[data->edit][i] = clipnote[i-offset];
  }
  if (c->events_flow) {
    gen_update_controls(c->g, -1);
  }
}

PRIVATE GtkItemFactoryEntry popup_items[] = {
  /* Path, accelerator, callback, extra-numeric-argument, kind */
  { "/Full",			NULL,	NULL,	    0,	"<Branch>" },
  { "/Full/Copy",		NULL,	do_copy,    0,	NULL },
  { "/Full/Paste",		NULL,	do_paste,   0,	NULL },
  { "/1st Half",		NULL,	NULL,	    0,	"<Branch>" },
  { "/1st Half/Copy",		NULL,	do_copy,    1,	NULL },
  { "/1st Half/Paste",		NULL,	do_paste,   1,	NULL },
  { "/2nd Half",		NULL,	NULL,	    0,	"<Branch>" },
  { "/2nd Half/Copy",		NULL,	do_copy,    2,	NULL },
  { "/2nd Half/Paste",		NULL,	do_paste,   2,	NULL },
  { "/1st Quarter",		NULL,	NULL,	    0,	"<Branch>" },
  { "/1st Quarter/Copy",	NULL,	do_copy,    3,	NULL },
  { "/1st Quarter/Paste",	NULL,	do_paste,   3,	NULL },
  { "/2nd Quarter",		NULL,	NULL,	    0,	"<Branch>" },
  { "/2nd Quarter/Copy",	NULL,	do_copy,    4,	NULL },
  { "/2nd Quarter/Paste",	NULL,	do_paste,   4,	NULL },
  { "/3rd Quarter",		NULL,	NULL,	    0,	"<Branch>" },
  { "/3rd Quarter/Copy",	NULL,	do_copy,    5,	NULL },
  { "/3rd Quarter/Paste",	NULL,	do_paste,   5,	NULL },
  { "/4th Quarter",		NULL,	NULL,	    0,	"<Branch>" },
  { "/4th Quarter/Copy",	NULL,	do_copy,    6,	NULL },
  { "/4th Quarter/Paste",	NULL,	do_paste,   6,	NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

PRIVATE GtkWidget *build_popup(Control *c) {
  GtkItemFactory *ifact;
  GtkWidget *result;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<seqnote-popup>", NULL);

  gtk_item_factory_create_items(ifact, nitems, popup_items, c);
  result = gtk_item_factory_get_widget(ifact, "<seqnote-popup>");

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);
  return result;
}

PRIVATE void popup_handler(GtkWidget *widget, Control *c) {
  //Data *data = c->g->data;
  GtkWidget *popup = build_popup(c);

  gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL, 1, 0);
}

PRIVATE void init_pattern(Control *control) {
  GtkWidget *hb;
  int i;
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * (SEQUENCE_LENGTH * 2 + 1));
  Data *data = control->g->data;
  GtkWidget *label, *menu;
  GtkWidget *rightvb;

  hb = gtk_hbox_new(FALSE, 5);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    GtkWidget *vb = gtk_vbox_new(FALSE, 5);
    GtkWidget *b = gtk_vscale_new(NULL);
    GtkWidget *t = gtk_toggle_button_new();
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(b));

    gtk_scale_set_draw_value(GTK_SCALE(b), FALSE);
    gtk_scale_set_digits(GTK_SCALE(b), 2);

    adj->step_increment = 1;
    adj->page_increment = 1;
    adj->lower = 0;
    adj->upper = 127;
    adj->value = 127 - data->pattern[data->edit][i];

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(t), data->note[data->edit][i]);

    gtk_object_set_data(GTK_OBJECT(adj), "Control", control);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		       GTK_SIGNAL_FUNC(value_changed_handler), (gpointer) i);
    gtk_signal_connect(GTK_OBJECT(b), "focus_in_event",
		       GTK_SIGNAL_FUNC(focus_in_handler), control);
    gtk_widget_set_usize(b, 12, 100);
    gtk_box_pack_start(GTK_BOX(vb), b, FALSE, FALSE, 0);
    gtk_widget_show(b);
    widgets[i] = b;

    gtk_object_set_data(GTK_OBJECT(t), "Control", control);
    gtk_signal_connect(GTK_OBJECT(t), "toggled",
		       GTK_SIGNAL_FUNC(toggle_changed_handler), (gpointer) i);
    gtk_widget_set_usize(t, 12, 16);
    gtk_box_pack_start(GTK_BOX(vb), t, FALSE, FALSE, 0);
    gtk_widget_show(t);
    widgets[i + SEQUENCE_LENGTH] = t;

    gtk_box_pack_start(GTK_BOX(hb), vb, FALSE, FALSE, 0);
    gtk_widget_show(vb);
  }
  rightvb = gtk_vbox_new( FALSE, 5 );

  label = gtk_label_new("--");
  gtk_box_pack_start(GTK_BOX(rightvb), label, FALSE, FALSE, 0);
  menu = gtk_button_new_with_label( "M" );
  gtk_box_pack_start(GTK_BOX(rightvb), menu, TRUE, FALSE, 0);


  gtk_signal_connect( GTK_OBJECT(menu), "clicked",
	GTK_SIGNAL_FUNC( popup_handler ), control );

  gtk_box_pack_start(GTK_BOX(hb), rightvb, FALSE, FALSE, 0);
  gtk_widget_show(label);
  gtk_widget_show(menu);
  gtk_widget_show(rightvb);
  widgets[SEQUENCE_LENGTH * 2] = label;

  control->widget = hb;
  control->data = widgets;
}

PRIVATE void init_2oct_pattern(Control *control) {
  GtkWidget *hb;
  int i;
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * (SEQUENCE_LENGTH * 2 + 1));
  Data *data = control->g->data;
  GtkWidget *label, *menu;
  GtkWidget *rightvb;

  hb = gtk_hbox_new(FALSE, 5);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    GtkWidget *vb = gtk_vbox_new(FALSE, 5);
    GtkWidget *b = gtk_vscale_new(NULL);
    GtkWidget *t = gtk_toggle_button_new();
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(b));

    gtk_scale_set_draw_value(GTK_SCALE(b), FALSE);
    gtk_scale_set_digits(GTK_SCALE(b), 2);

    adj->step_increment = 1;
    adj->page_increment = 1;
    adj->lower = 127 - 48;
    adj->upper = 127 - 24;
    adj->value = 127 - data->pattern[data->edit][i];

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(t), data->note[data->edit][i]);

    gtk_object_set_data(GTK_OBJECT(adj), "Control", control);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		       GTK_SIGNAL_FUNC(value_changed_handler), (gpointer) i);
    gtk_signal_connect(GTK_OBJECT(b), "focus_in_event",
		       GTK_SIGNAL_FUNC(focus_in_handler), control);
    gtk_widget_set_usize(b, 12, 100);
    gtk_box_pack_start(GTK_BOX(vb), b, FALSE, FALSE, 0);
    gtk_widget_show(b);
    widgets[i] = b;

    gtk_object_set_data(GTK_OBJECT(t), "Control", control);
    gtk_signal_connect(GTK_OBJECT(t), "toggled",
		       GTK_SIGNAL_FUNC(toggle_changed_handler), (gpointer) i);
    gtk_widget_set_usize(t, 12, 16);
    gtk_box_pack_start(GTK_BOX(vb), t, FALSE, FALSE, 0);
    gtk_widget_show(t);
    widgets[i + SEQUENCE_LENGTH] = t;

    gtk_box_pack_start(GTK_BOX(hb), vb, FALSE, FALSE, 0);
    gtk_widget_show(vb);
  }
  rightvb = gtk_vbox_new( FALSE, 5 );

  label = gtk_label_new("--");
  gtk_box_pack_start(GTK_BOX(rightvb), label, FALSE, FALSE, 0);
  menu = gtk_button_new_with_label( "M" );
  gtk_box_pack_start(GTK_BOX(rightvb), menu, TRUE, FALSE, 0);


  gtk_signal_connect( GTK_OBJECT(menu), "clicked",
	GTK_SIGNAL_FUNC( popup_handler ), control );

  gtk_box_pack_start(GTK_BOX(hb), rightvb, FALSE, FALSE, 0);
  gtk_widget_show(label);
  gtk_widget_show(menu);
  gtk_widget_show(rightvb);
  widgets[SEQUENCE_LENGTH * 2] = label;

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
			     127 - data->pattern[data->edit][i]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets[i + SEQUENCE_LENGTH]),
				 data->note[data->edit][i]);
  }
}

PRIVATE void setup_class(void) {
  int i;
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 3, 2,
					     NULL, NULL, seqnote_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_SEL_EDIT, "Edit Seq", evt_sel_edit_handler);
  gen_configure_event_input(k, EVT_SEL_PLAY, "Play Seq", evt_sel_play_handler);
  gen_configure_event_input(k, EVT_STEP, "Step", evt_step_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Freq");
  gen_configure_event_output(k, EVT_NOTE_OUT, "Note");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);

  for( i=0; i<SEQUENCE_LENGTH; i++ ) {
      clipseq[i] = NOTE_A4;
      clipnote[i] = FALSE;
  }
}

PUBLIC void init_plugin_seqnote(void) {
  setup_class();
}
