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
#include "msgbox.h"

/* If you change these, don't forget to change the radiobutton in props() below */
#define DEFAULT_LINEAR	0.003
#define DEFAULT_EXP	0.0005

typedef struct Data {
  gdouble linear_time;
  gdouble exp_halftime;

  SAMPLETIME location;
  SAMPLETIME linear_sampletime;
  gdouble linear_step;
  gdouble exp_factor;
  SAMPLE current;
} Data;

PRIVATE void precalc_data(Data *data) {
  data->linear_sampletime = data->linear_time * SAMPLE_RATE;
  data->linear_step = 1.0 / data->linear_sampletime;
  data->exp_factor = data->exp_halftime > 0
    ? pow(0.5, 1.0 / (data->exp_halftime * SAMPLE_RATE))
    : 0;
  data->location = data->linear_sampletime;
}

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));

  data->linear_time = 0;
  data->exp_halftime = 0;

  data->current = 0;
  precalc_data(data);

  g->data = data;
  return TRUE;
}

PRIVATE void done_instance(Generator *g) {
  free(g->data);
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  SAMPLETIME loc = data->location;
  gboolean result;

  data->location += buflen;
  result = gen_read_realtime_input(g, 0, -1, buf, buflen);

  if (result && (data->linear_sampletime > 0)) {
    if (loc < data->linear_sampletime) {
      int i;
      int remaining = data->linear_sampletime - loc;
      gdouble linear_current_factor = (gdouble) remaining / data->linear_sampletime;
      gdouble linear_buf_factor = 1.0 - linear_current_factor;
      gdouble current = data->current;

      for (i = 0; i < buflen && i < remaining; i++) {
	current = buf[i] - (buf[i] - current) * data->exp_factor;
	buf[i] = linear_buf_factor * buf[i] + linear_current_factor * current;
	linear_buf_factor += data->linear_step;
	linear_current_factor -= data->linear_step;
      }

      data->current = current;
    } else {
      data->current = buf[buflen - 1];
    }
  } else {
    data->current = 0;
  }

  return result;
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->linear_time = objectstore_item_get_double(item, "smooth_linear_ramp_time", 0);
  data->exp_halftime = objectstore_item_get_double(item, "smooth_exp_merge_halftime", 0);
  data->current = 0;
  precalc_data(data);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "smooth_linear_ramp_time", data->linear_time);
  objectstore_item_set_double(item, "smooth_exp_merge_halftime", data->exp_halftime);
}

PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  Data *data = g->data;
  data->location = 0;
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
  GtkWidget *linentry, *expentry;
  GtkWidget *none_button, *default_button, *custom_button;
  GtkWidget *frame;
  GtkWidget *vbox;

  frame = gtk_frame_new("Trigger Smoothing");

  vbox = gtk_vbox_new(FALSE, 2);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  gtk_widget_show(vbox);

  none_button =
    gtk_radio_button_new_with_label(NULL,
				    "No smoothing");
  default_button =
    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(none_button)),
				    "Default smoothing (3ms linear, 0.5ms exp)");
  custom_button =
    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(default_button)),
				    "Custom smoothing:");

  gtk_box_pack_start(GTK_BOX(vbox), none_button, TRUE, TRUE, 0);
  gtk_widget_show(none_button);
  gtk_box_pack_start(GTK_BOX(vbox), default_button, TRUE, TRUE, 0);
  gtk_widget_show(default_button);
  gtk_box_pack_start(GTK_BOX(vbox), custom_button, TRUE, TRUE, 0);
  gtk_widget_show(custom_button);

  {
    GtkWidget *toactivate = NULL;

    if (data->linear_time == 0 || data->exp_halftime == 0)
      toactivate = none_button;
    else if (data->linear_time == DEFAULT_LINEAR && data->exp_halftime == DEFAULT_EXP)
      toactivate = default_button;
    else
      toactivate = custom_button;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toactivate), TRUE);
  }

  linentry = build_entry(vbox, "linear ramp time (seconds):", data->linear_time);
  expentry = build_entry(vbox, "exponential merge halftime (seconds):", data->exp_halftime);

  if (popup_dialog(g->name, MSGBOX_OK | MSGBOX_CANCEL,
		   0, MSGBOX_OK, frame, NULL, 0) == MSGBOX_OK) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(none_button))) {
      data->linear_time = 0;
      data->exp_halftime = 0;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(default_button))) {
      data->linear_time = DEFAULT_LINEAR;
      data->exp_halftime = DEFAULT_EXP;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(custom_button))) {
      data->linear_time = atof(gtk_entry_get_text(GTK_ENTRY(linentry)));
      data->exp_halftime = atof(gtk_entry_get_text(GTK_ENTRY(expentry)));
    } else
      g_warning("Peculiar: none of the three radiobuttons were toggled on! (g->name is %s)",
		g->name);

    precalc_data(data);
  }
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("smooth", FALSE, 1, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, done_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, 0, "Trigger", evt_trigger_handler);

  gencomp_register_generatorclass(k, FALSE, "Levels/Triggered Cross-fade",
				  NULL, props);
}

PUBLIC void init_plugin_smooth(void) {
  setup_class();
}
