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

#define GENERATOR_CLASS_NAME	"sigctrl"
#define GENERATOR_CLASS_PATH	"Controls/Signal Control"

#define EVT_VALUE 0
#define SIG_OUTPUT 0

typedef struct Data {
  gdouble min, max, step, page;
  gdouble value;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->min = data->value = 0;
  data->max = 100;
  data->step = data->page = 1;

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->min = objectstore_item_get_double(item, "sigctrl_min", 0);
  data->max = objectstore_item_get_double(item, "sigctrl_max", 0);
  data->step = objectstore_item_get_double(item, "sigctrl_step", 0);
  data->page = objectstore_item_get_double(item, "sigctrl_page", 0);
  data->value = objectstore_item_get_double(item, "sigctrl_value", 0);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "sigctrl_min", data->min);
  objectstore_item_set_double(item, "sigctrl_max", data->max);
  objectstore_item_set_double(item, "sigctrl_step", data->step);
  objectstore_item_set_double(item, "sigctrl_page", data->page);
  objectstore_item_set_double(item, "sigctrl_value", data->value);
}

PRIVATE void ctrl_setrange(Control *c) {
  Data *data= c->g->data;

  c->min = data->min;
  c->max = data->max;
  c->step = data->step;
  c->page = data->page;
}

PRIVATE void evt_value_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->value = event->d.number;
  gen_update_controls(g, -1);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if (data->value == 0)
    return FALSE;

  for (i = 0; i < buflen; i++)
    buf[i] = data->value;
  return TRUE;
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_KNOB, "knob", 0,0,0,0,0,TRUE, 1,EVT_VALUE,
    ctrl_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_SLIDER, "slider", 0,0,0,0,0,TRUE, 1,EVT_VALUE,
    ctrl_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
  { CONTROL_KIND_TOGGLE, "toggle", 0,0,0,0,0,FALSE, 1,EVT_VALUE,
    ctrl_setrange,NULL, control_double_updater, (gpointer) offsetof(Data, value) },
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

    g_list_foreach(g->controls, (GFunc) ctrl_setrange, NULL);
    g_list_foreach(g->controls, (GFunc) control_update_range, NULL);
  }
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 0,
					     NULL, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Value", evt_value_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, props);
}

PUBLIC void init_plugin_sigctrl(void) {
  setup_class();
}
