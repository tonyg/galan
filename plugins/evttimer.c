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

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"evttimer"
#define GENERATOR_CLASS_PATH	"Events/Timer"

#define EVT_VALUE 0
#define EVT_OUTPUT 0

typedef struct Data {
  gdouble min, max;
  gdouble page, step;
  gdouble value;
  SAMPLETIME lastevt;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->min = data->value = 0;
  data->max = 100;
  data->step = data->page = 1;
  data->lastevt = 0;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->min = objectstore_item_get_double(item, "evttimer_min", 0);
  data->max = objectstore_item_get_double(item, "evttimer_max", 0);
  data->step = objectstore_item_get_double(item, "evttimer_step", 0);
  data->page = objectstore_item_get_double(item, "evttimer_page", 0);
  data->value = objectstore_item_get_double(item, "evttimer_value", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "evttimer_min", data->min);
  objectstore_item_set_double(item, "evttimer_max", data->max);
  objectstore_item_set_double(item, "evttimer_step", data->step);
  objectstore_item_set_double(item, "evttimer_page", data->page);
  objectstore_item_set_double(item, "evttimer_value", data->value);
}

PRIVATE void evttimer_setrange(Control *c) {
  Data *data= c->g->data;

  c->min = data->min;
  c->max = data->max;
  c->step = data->step;
  c->page = data->page;
}

PRIVATE void evt_value_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  gdouble diff = (gdouble) (event->time - data->lastevt) / SAMPLE_RATE;
  
  if( data->lastevt && (diff <= data->max) && (diff >= data->min) ) {

      data->value = diff;
      data->lastevt = event->time;
      event->d.number = data->value;

      gen_update_controls(g, -1);
      gen_send_events(g, EVT_OUTPUT, -1, event);
  }
  else {
      if( !data->lastevt || diff >=data->min )
	  data->lastevt = event->time;
  }
}


PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_KNOB, "knob", 0,0,0,0,0,TRUE, 1,EVT_VALUE,
    evttimer_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_SLIDER, "slider", 0,0,0,0,0,TRUE, 1,EVT_VALUE,
    evttimer_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_TOGGLE, "toggle", 0,0,0,0,0,FALSE, 1,EVT_VALUE,
    evttimer_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_BUTTON, "button", 0,0,0,0,0,FALSE, 1,EVT_VALUE,
    evttimer_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_NONE, }
};

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

    g_list_foreach(g->controls, (GFunc) evttimer_setrange, NULL);
    g_list_foreach(g->controls, (GFunc) control_update_range, NULL);
  }
}

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Value", evt_value_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, props);
}

PUBLIC void init_plugin_evttimer(void) {
  setup_class();
}
