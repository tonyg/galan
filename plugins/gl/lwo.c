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
#include <GL/gl.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"
#include "lw.h"


typedef struct Data {
    char *filename;
    lwObject *obj;
} Data;

PRIVATE gboolean render_function(Generator *g ) {

    Data *data = g->data;

    if( data->obj != NULL )
	lw_object_show( data->obj );

    return TRUE;
}

PRIVATE gboolean try_load(Generator *g, const char *filename, gboolean verbose) {

  Data *data = g->data;
  lwObject *obj = lw_object_read( filename );

  if (obj == NULL) {
    if (verbose)
      popup_msgbox("Load Error", MSGBOX_OK, 0, MSGBOX_OK,
		   "Could not open LWO file %s", filename);
    return FALSE;
  }

  if (data->obj != NULL)
    lw_object_free(data->obj);

  data->obj = obj;

  return TRUE;
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->filename = NULL;
  data->obj = NULL;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if( data->obj != NULL )
      lw_object_free( data->obj );
  if( data->filename != NULL )
      free( data->filename );

  free(data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
    Data *data = safe_malloc(sizeof(Data));
    g->data = data;

    data->filename = safe_string_dup( objectstore_item_get_string(item, "filename", NULL) );
    data->obj = NULL;
    if( data->filename != NULL )
	try_load(g, data->filename, FALSE);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  if( data->filename != NULL )
      objectstore_item_set_string(item, "filename", data->filename);
}

PRIVATE void load_new_sample(GtkWidget *widget, GtkWidget *fs) {
  Generator *g = gtk_object_get_data(GTK_OBJECT(fs), "Generator");
  GtkWidget *label = gtk_object_get_data(GTK_OBJECT(fs), "FilenameLabel");
  Data *data = g->data;
  const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  if (try_load(g, filename, TRUE)) {
    if (data->filename != NULL)
      free(data->filename);

    data->filename = safe_string_dup(filename);
    gtk_label_set_text(GTK_LABEL(label), data->filename);

    gtk_widget_destroy(fs);	/* %%% should this be gtk_widget_hide? uber-paranoia */
  }
}

PRIVATE void choose_clicked(GtkWidget *choose_button, Generator *g) {
  GtkWidget *fs = gtk_file_selection_new("Open LWO File");
  Data *data = g->data;

  gtk_object_set_data(GTK_OBJECT(fs), "Generator", g);
  gtk_object_set_data(GTK_OBJECT(fs), "FilenameLabel",
		      gtk_object_get_data(GTK_OBJECT(choose_button), "FilenameLabel"));

  if (data->filename)
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), data->filename);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		     GTK_SIGNAL_FUNC(load_new_sample), fs);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

  gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
  gtk_widget_show(fs);
}

PRIVATE void reload_clicked(GtkWidget *choose_button, Generator *g) {
  Data *data = g->data;

  if (data->filename != NULL)
    try_load(g, data->filename, TRUE);
}

PRIVATE void propgen(Component *c, Generator *g) {
  Data *data = g->data;

  GtkWidget *vb = gtk_vbox_new(FALSE, 5);
  GtkWidget *hb = gtk_hbox_new(TRUE, 5);
  GtkWidget *frame = gtk_frame_new("LWO File");
  GtkWidget *label = gtk_label_new(data->filename ? data->filename : "<none>");
  GtkWidget *reload_button = gtk_button_new_with_label("Reload");
  GtkWidget *choose_button = gtk_button_new_with_label("Choose File...");

  gtk_container_add(GTK_CONTAINER(frame), vb);
  gtk_widget_show(vb);

  gtk_box_pack_start(GTK_BOX(vb), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  gtk_box_pack_start(GTK_BOX(vb), hb, TRUE, TRUE, 0);
  gtk_widget_show(hb);



  gtk_box_pack_start(GTK_BOX(hb), reload_button, TRUE, TRUE, 0);
  gtk_widget_show(reload_button);
  gtk_box_pack_start(GTK_BOX(hb), choose_button, TRUE, TRUE, 0);
  gtk_widget_show(choose_button);

  gtk_signal_connect(GTK_OBJECT(reload_button), "clicked",
		     GTK_SIGNAL_FUNC(reload_clicked), g);
  gtk_signal_connect(GTK_OBJECT(choose_button), "clicked",
		     GTK_SIGNAL_FUNC(choose_clicked), g);
  gtk_object_set_data(GTK_OBJECT(choose_button), "FilenameLabel", label);


  popup_dialog("Properties", MSGBOX_DISMISS, 0, MSGBOX_DISMISS, frame, NULL, 0);
}
PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "triangle", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("lwo", FALSE, 0, 0,
					     NULL, output_sigs, NULL,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gencomp_register_generatorclass(k, FALSE, "GL/LWO Render", NULL, propgen);
}

PUBLIC void init_plugin_lwo(void) {
  setup_class();
}

