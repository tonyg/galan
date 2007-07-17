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
#include "msgbox.h"

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
  gdouble min, max, step, page;
  gdouble pattern[NUM_PATTERNS][SEQUENCE_LENGTH];
} Data;

PRIVATE gdouble clipseq[SEQUENCE_LENGTH];

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int i, j;

  g->data = data;

  data->edit = data->play = 0;
  data->min = 0;
  data->max = 1;
  data->step = 0.1;
  data->page = 0.1;

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

  data->min = objectstore_item_get_double(item, "seqnum_min", 0);
  data->max = objectstore_item_get_double(item, "seqnum_max", 1);
  data->step = objectstore_item_get_double(item, "seqnum_step", 0.01);
  data->page = objectstore_item_get_double(item, "seqnum_page", 0.01);

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
  objectstore_item_set_double(item, "seqnum_min", data->min);
  objectstore_item_set_double(item, "seqnum_max", data->max);
  objectstore_item_set_double(item, "seqnum_step", data->step);
  objectstore_item_set_double(item, "seqnum_page", data->page);
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

PRIVATE void update_label(GtkWidget *label, float val) {
  char text[20];

  sprintf(text, "%3.2f", val);
  gtk_label_set_text(GTK_LABEL(label), text);
}

PRIVATE void value_changed_handler(GtkAdjustment *adj, gpointer userdata) {
  int step = (int) userdata;
  Control *c = gtk_object_get_data(GTK_OBJECT(adj), "Control");
  GtkWidget **widgets = c->data;
  Data *data = c->g->data;

  if (c->events_flow) {
    data->pattern[data->edit][step] = data->max - (adj->value - data->min);
    update_label(widgets[SEQUENCE_LENGTH], data->max - (adj->value - data->min));
    gen_update_controls(c->g, SEQNUM_CONTROL_PATTERN);
  }
}

PRIVATE void focus_in_handler(GtkRange *b, GdkEventFocus *event, gpointer userdata) {
  Control *c = userdata;
  GtkWidget **widgets = c->data;
  Data *data = c->g->data;
  GtkAdjustment *adj = gtk_range_get_adjustment(b);

  if (c->events_flow) {
    update_label(widgets[SEQUENCE_LENGTH], data->max - (adj->value - data->min));
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
  }
  if (c->events_flow) {
    gen_update_controls(c->g, SEQNUM_CONTROL_PATTERN);
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

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<seqnum-popup>", NULL);

  gtk_item_factory_create_items(ifact, nitems, popup_items, c);
  result = gtk_item_factory_get_widget(ifact, "<seqnum-popup>");

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
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * (SEQUENCE_LENGTH + 1));
  Data *data = control->g->data;
  GtkWidget *label, *menu;
  GtkWidget *rightvb;

  hb = gtk_hbox_new(FALSE, 5);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
    GtkWidget *b = gtk_vscale_new(NULL);
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(b));

    gtk_scale_set_draw_value(GTK_SCALE(b), FALSE);
    gtk_scale_set_digits(GTK_SCALE(b), 2);

    adj->step_increment = data->step;
    adj->page_increment = data->page;
    adj->lower = data->min;
    adj->upper = data->max;
    adj->value = data->max - (data->pattern[data->edit][i] + data->min);

    gtk_object_set_data(GTK_OBJECT(adj), "Control", control);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
		       GTK_SIGNAL_FUNC(value_changed_handler), (gpointer) i);
    gtk_signal_connect(GTK_OBJECT(b), "focus_in_event",
		       GTK_SIGNAL_FUNC(focus_in_handler), control);
    gtk_box_pack_start(GTK_BOX(hb), b, FALSE, FALSE, 0);
    gtk_widget_set_usize(b, 12, 100);
    gtk_widget_show(b);
    widgets[i] = b;
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
  widgets[SEQUENCE_LENGTH] = label;

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
			     data->max - (data->pattern[data->edit][i] - data->min));
  }
}

PRIVATE void seqnum_setrange(Control *c) {
  Data *data= c->g->data;
  GtkWidget **widgets = c->data;
  int i,j;

  for( i=0; i<NUM_PATTERNS; i++ )
      for( j=0; j<SEQUENCE_LENGTH; j++ )
	  data->pattern[i][j] = MAX( data->min, MIN( data->max, data->pattern[i][j] ) );

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
      GtkAdjustment *adj = gtk_range_get_adjustment( GTK_RANGE(widgets[i]) );

      adj->lower = data->min;
      adj->upper = data->max;
      adj->step_increment = data->step;
      adj->page_increment = data->page;
      adj->value = data->max - (data->pattern[data->edit][i] - data->min);

      gtk_adjustment_changed( adj );
  }
}

PRIVATE GtkWidget *build_entry(GtkWidget *vbox, char *text, gdouble value) {
  GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
  GtkWidget *label = gtk_label_new(text);
  GtkWidget *entry = gtk_entry_new();
  char buf[128];

  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(hbox);

  sprintf(buf, "%g", value);
  gtk_entry_set_text(GTK_ENTRY(entry), buf);

  return entry;
}

PRIVATE void props(Component *c, Generator *g) {
  Data *data = g->data;
  GtkWidget *min, *max, *step, *page;
  GtkWidget *vbox;

  vbox = gtk_vbox_new(FALSE, 2);

  min = build_entry(vbox, "Range Minimum:", data->min);
  max = build_entry(vbox, "Range Maximum:", data->max);
  step = build_entry(vbox, "Step Increment:", data->step);
  page = build_entry(vbox, "Page Increment:", data->page);

  if (popup_dialog(g->name, MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, vbox, NULL, 0) == MSGBOX_OK) {
    data->min = atof(gtk_entry_get_text(GTK_ENTRY(min)));
    data->max = atof(gtk_entry_get_text(GTK_ENTRY(max)));
    data->step = atof(gtk_entry_get_text(GTK_ENTRY(step)));
    data->page = atof(gtk_entry_get_text(GTK_ENTRY(page)));

    g_list_foreach(g->controls, (GFunc) seqnum_setrange, NULL);
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

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, props);
}

PUBLIC void init_plugin_seqnum(void) {
  setup_class();
}
