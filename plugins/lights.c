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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define SEQUENCE_LENGTH	32
#define NUM_PATTERNS	16

#define GENERATOR_CLASS_NAME	"lights"
#define GENERATOR_CLASS_PATH	"Pattern/Lights"

#define EVT_SEL_PLAY	0	/* input */

/* Forward refs. */
PRIVATE void init_lights(Control *control);
PRIVATE void done_lights(Control *control);
PRIVATE void refresh_lights(Control *control);

#define TRIGSEQ_CONTROL_PATTERN		0
PRIVATE ControlDescriptor trigseq_controls[] = {
  { CONTROL_KIND_USERDEF, "lights", 0,0,0,0, 0,FALSE, 0,0, init_lights, done_lights, refresh_lights },
  { CONTROL_KIND_NONE, }
};

typedef struct Data {
  int play;
} Data;

PRIVATE GdkPixmap *on_pixmap;
PRIVATE GdkBitmap *on_mask;
PRIVATE GdkPixmap *off_pixmap;
PRIVATE GdkBitmap *off_mask;

PRIVATE GList *lget_anim_list( char *name ) {

   GdkPixbufAnimation *animation = gdk_pixbuf_animation_new_from_file( name );

   GList *framelistX, *framelist = gdk_pixbuf_animation_get_frames( animation );
   GdkPixbufFrame *firstframe = framelist->data;
   GdkPixbuf *firstpixbuf = gdk_pixbuf_frame_get_pixbuf( firstframe );
   GdkPixbuf *actpixbuf  = gdk_pixbuf_copy( firstpixbuf );
   GList *retval = NULL;

   retval= g_list_append( retval, gdk_pixbuf_copy( firstpixbuf ) );

   for( framelistX = framelist; framelistX != NULL; framelistX = g_list_next( framelistX ) ) {
       GdkPixbufFrame *frame = framelistX->data;
       GdkPixbuf *framebuf = gdk_pixbuf_frame_get_pixbuf( frame );
       int xo=gdk_pixbuf_frame_get_x_offset( frame ); 
       int yo=gdk_pixbuf_frame_get_y_offset( frame ); 
       int w=gdk_pixbuf_get_width( framebuf ); 
       int h=gdk_pixbuf_get_height( framebuf ); 
       GdkPixbuf *tmpbuf = gdk_pixbuf_copy( actpixbuf );

       gdk_pixbuf_composite( framebuf, tmpbuf, xo,yo, w, h, xo, yo, 1,1, GDK_INTERP_NEAREST, 255 );  
       
       retval= g_list_append( retval, tmpbuf );

       
       switch( gdk_pixbuf_frame_get_action( frame ) ) {
	   case GDK_PIXBUF_FRAME_RETAIN:
	       gdk_pixbuf_unref( actpixbuf );
	       actpixbuf = gdk_pixbuf_copy( tmpbuf );
	       break;
	   case GDK_PIXBUF_FRAME_DISPOSE:
	       break;
	   case GDK_PIXBUF_FRAME_REVERT:
	       gdk_pixbuf_unref( actpixbuf );
	       actpixbuf = gdk_pixbuf_copy( firstpixbuf );
	       break;
       }
       
   }

    
   gdk_pixbuf_unref( actpixbuf );
   gdk_pixbuf_unref( firstpixbuf );

   return retval;
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;

  data->play = 0;
  //data->diode = get_anim_list( PIXMAPDIRIFY( "diode.gif" ) );

  //g_print( "list_legth = %d\n" , g_list_length( data->diode ) );

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;

  data->play = objectstore_item_get_integer(item, "lights_play", 0);
  //data->diode = get_anim_list( PIXMAPDIRIFY( "diode.gif" ) );

}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;

  objectstore_item_set_integer(item, "lights_play", data->play);
}


PRIVATE void evt_play_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->play = GEN_DOUBLE_TO_INT(event->d.number) % SEQUENCE_LENGTH;
  gen_update_controls( g, -1 );
}


PRIVATE void init_lights(Control *control) {
  GtkWidget *hb;
  int i;
  Data *data = control->g->data;
  GtkWidget **widgets = safe_malloc(sizeof(GtkWidget *) * SEQUENCE_LENGTH);

  hb = gtk_hbox_new(TRUE, 1);

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
      GtkWidget *img;
      if( data->play == i )
	  img = gtk_pixmap_new(on_pixmap, on_mask );
      else
	  img = gtk_pixmap_new(off_pixmap, off_mask );

      gtk_box_pack_start(GTK_BOX(hb), img, FALSE, FALSE, 0);
      gtk_widget_show(img);
      widgets[i] = img;
  }

  control->widget = hb;
  control->data = widgets;
}

PRIVATE void done_lights(Control *control) {
  free(control->data);
}

PRIVATE void refresh_lights(Control *control) {
  Data *data = control->g->data;
  GtkWidget **widgets = control->data;
  int i;

  for (i = 0; i < SEQUENCE_LENGTH; i++) {
      if( data->play == i )
	  gtk_pixmap_set(GTK_PIXMAP(widgets[i]), on_pixmap, on_mask );
      else
	  gtk_pixmap_set(GTK_PIXMAP(widgets[i]), off_pixmap, off_mask );
  }
}

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 0,
					     NULL, NULL, trigseq_controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_SEL_PLAY, "Step", evt_play_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_lights(void) {

  GList *diode = lget_anim_list( PIXMAPDIRIFY( "diode.gif" ) );
  GdkPixbuf *on_pixbuf = g_list_nth_data( diode, 2 );
  GdkPixbuf *off_pixbuf = g_list_nth_data( diode, 0 );

  gdk_pixbuf_render_pixmap_and_mask( on_pixbuf, &on_pixmap, &on_mask, 255 );
  gdk_pixbuf_render_pixmap_and_mask( off_pixbuf, &off_pixmap, &off_mask, 255 );

  setup_class();
}
