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
#include "sheet.h"
#include "gencomp.h"
#include "gui.h"

typedef struct Data {

    GdkColor	plane_color;
    guint16	plane_alpha;

} Data;


PRIVATE int init_instance(Generator *g) {

  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  gdk_color_white( gdk_colormap_get_system(), &(data->plane_color) );
  data->plane_alpha = 0;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {

  Data *data = g->data;
  free(data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

    Data *data = safe_malloc(sizeof(Data));
    g->data = data;

    data->plane_color.red   = objectstore_item_get_integer( item, "plane_red", 65535 );
    data->plane_color.green = objectstore_item_get_integer( item, "plane_green", 65535 );
    data->plane_color.blue  = objectstore_item_get_integer( item, "plane_blue", 65535 );
    data->plane_alpha = objectstore_item_get_integer( item, "plane_alpha", 0 );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer( item, "plane_red", data->plane_color.red );
  objectstore_item_set_integer( item, "plane_green", data->plane_color.green );
  objectstore_item_set_integer( item, "plane_blue", data->plane_color.blue );
}



PRIVATE gboolean render_function(Generator *g ) {

  Data *data = g->data;

    glColor4f( data->plane_color.red / 65535.0,data->plane_color.green / 65535.0, data->plane_color.blue / 65535.0, data->plane_alpha / 65535.0 );
  //glColor3f(1,1,0);
  glNormal3f( 0,0,1 );
  glBegin(GL_QUADS);
  glVertex3f(-1,-1,0);
  glVertex3f(-1,1,0);
  glVertex3f(1,1,0);
  glVertex3f(1,-1,0);
  glEnd();

  return TRUE;
}



PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "plane", SIG_FLAG_OPENGL, { NULL, { NULL, NULL }, render_function } },
  { NULL, }
};


PRIVATE void propgen(Component *c, Generator *g) {
  Data *data = g->data;

    GtkWidget *dialog, *vbox2, *color1, *color2; //*type;

    dialog = gtk_dialog_new_with_buttons( "GL Plane Properties",  gui_get_mesh_window(), GTK_DIALOG_DESTROY_WITH_PARENT,
	    "Oki", GTK_RESPONSE_OK, "Not Now", GTK_RESPONSE_CANCEL, NULL );

    vbox2 = gtk_vbox_new( FALSE, 10 );
    color1 = g_object_new( GTK_TYPE_COLOR_BUTTON, "color", &(data->plane_color), "title", "plane_color", "use-alpha", TRUE, "alpha", data->plane_alpha, NULL );


    gtk_container_add( GTK_CONTAINER( vbox2 ), color1 );
    
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog )->vbox ), vbox2 );
    gtk_widget_show_all( dialog );

    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );
    if( response != GTK_RESPONSE_CANCEL ) {
	gtk_color_button_get_color( GTK_COLOR_BUTTON(color1), &(data->plane_color) );
	data->plane_alpha = gtk_color_button_get_alpha( GTK_COLOR_BUTTON(color1) );
    }

    
    gtk_widget_destroy( dialog );


#if 0
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
#endif
}

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass("glplane", FALSE, 0, 0,
					     NULL, output_sigs, NULL,
					     init_instance, destroy_instance, unpickle_instance, pickle_instance);

  gencomp_register_generatorclass(k, FALSE, "GL/plane", NULL, propgen);
}


PUBLIC void init_plugin_plane(void) {
  setup_class();
}

