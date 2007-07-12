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

/* Template gAlan plugin file. Please distribute your plugins according to
   the GNU General Public License! (You don't have to, but I'd prefer it.) */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#include <gtkgl/gtkglarea.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"


#define GENERATOR_CLASS_NAME	"gldisplay"
#define GENERATOR_CLASS_PATH	"GL/gldisplay"

#define SIG_INPUT		1

#define EVT_TRIGGER		0

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	0
#define NUM_EVENT_INPUTS	1


typedef struct Data {
	/* state, to be pickled, unpickled */
} Data;

PRIVATE void setup_tables(void) {
  /* Put any plugin-wide initialisation here. */
}



PRIVATE gboolean init_instance(Generator *g) {
	/* TODO: need to allocate the intbuf of size yscale */
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
	
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  //Data *data = g->data;
}

PRIVATE gint init(GtkWidget *widget)
{
  /* OpenGL functions can be called only if make_current returns true */
  if (gtk_gl_area_make_current(GTK_GL_AREA(widget))) {

      int i;

      glViewport(0,0, widget->allocation.width, widget->allocation.height);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      //glOrtho(0,100, 100,0, -1,1);
      //glFrustum( -1,1, 1,-1, 2, 40 );

      gluPerspective( 45,1,1,100 );
      
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      //glDepthFunc( GL_LESS );
      glEnable(GL_DEPTH_TEST);
      /* remove back faces */
      glDisable(GL_CULL_FACE);
      //glCullFace(GL_FRONT);

      /* speedups */
      glEnable(GL_DITHER);
      glShadeModel(GL_SMOOTH);
      //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
      //glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);

      /* light */
      //glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
      //glLightfv(GL_LIGHT0, GL_DIFFUSE,  light0_color);  
      //glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
      //glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_color);
      //glEnable(GL_LIGHT0);
      //glEnable(GL_LIGHT1);
      glEnable(GL_LIGHTING);

      for( i=0; i<8; i++ )
	  glDisable( GL_LIGHT0 + i );

      glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
      glEnable(GL_COLOR_MATERIAL);  
  }

  return TRUE;
}


/* When widget is exposed it's contents are redrawn. */
PRIVATE gint draw(GtkWidget *widget, GdkEventExpose *event)
{
  /* Draw only last expose. */

  if (event->count > 0)
    return TRUE;

  /* OpenGL functions can be called only if make_current returns true */
  if (gtk_gl_area_make_current(GTK_GL_AREA(widget))) {

      Control *control = g_object_get_data( G_OBJECT(widget), "Control" );
      
      glClearColor(0,0,0,1);
      glClearDepth( 1 );
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

      /* call the render inputs */
      gen_render_gl( control->g, 0, -1 );

      /* Swap backbuffer to front */
      glFlush();
      gtk_gl_area_swap_buffers(GTK_GL_AREA(widget));

  }
  else
      printf( "gldisplay.c: gtk_gl_area_make_current == false \n" );

  return TRUE;
}

/* When glarea widget size changes, viewport size is set to match the new size */
PRIVATE gint reshape(GtkWidget *widget, GdkEventConfigure *event)
{
  /* OpenGL functions can be called only if make_current returns true */
  if (gtk_gl_area_make_current(GTK_GL_AREA(widget)))
    {
      glViewport(0,0, widget->allocation.width, widget->allocation.height);
    }
  return TRUE;
}

PRIVATE int attrlist[] = {
    GDK_GL_RGBA,
    GDK_GL_RED_SIZE,1,
    GDK_GL_GREEN_SIZE,1,
    GDK_GL_BLUE_SIZE,1,
    GDK_GL_DEPTH_SIZE,1,
    GDK_GL_DOUBLEBUFFER,
    GDK_GL_NONE
};


PRIVATE void init_scope( Control *control, int w, int h ) {

	GtkWidget *glarea;

  if (gdk_gl_query() == FALSE) {
    g_print("OpenGL not supported\n");
    control->widget = NULL;
    return;
  }

  /* Create new OpenGL widget. */
  glarea = GTK_WIDGET(gtk_gl_area_new(attrlist));
  /* Events for widget must be set before X Window is created */
  gtk_widget_set_events(GTK_WIDGET(glarea),
			GDK_EXPOSURE_MASK|
			GDK_BUTTON_PRESS_MASK);

  /* Connect signal handlers */
  /* Redraw image when exposed. */
  g_signal_connect(G_OBJECT(glarea), "expose_event",
		     GTK_SIGNAL_FUNC(draw), NULL);
  /* When window is resized viewport needs to be resized also. */
  g_signal_connect(G_OBJECT(glarea), "configure_event",
		     GTK_SIGNAL_FUNC(reshape), NULL);
  /* Do initialization when widget has been realized. */
  g_signal_connect(G_OBJECT(glarea), "realize",
		     GTK_SIGNAL_FUNC(init), NULL);

  /* set minimum size */
  gtk_widget_set_usize(GTK_WIDGET(glarea), w,h);
  g_object_set_data( G_OBJECT(glarea), "Control", control );

  control->widget = glarea;
}

PRIVATE void done_scope(Control *control) {
    GtkWidget *tmp = control->widget;
    control->widget = NULL;
    //gtk_widget_destroy( tmp );
}

PRIVATE void refresh_scope(Control *control) {
    if( control->widget )
	gtk_widget_queue_draw( control->widget );
}

PRIVATE void init_big_scope( Control *control ){
	init_scope( control, 400, 400 );
}

PRIVATE void init_small_scope( Control *control ){
	init_scope( control, 100, 100 );
}


PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_TRIGGER
   * set phase=0;
   */
  //Data *data = g->data;
  gen_update_controls( g, -1 );
}


PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_OPENGL },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_USERDEF, "glarea", 0,0,0,0, 0,FALSE, 0,0, init_small_scope, done_scope, refresh_scope },
  { CONTROL_KIND_USERDEF, "glarea-big", 0,0,0,0, 0,FALSE, 0,0, init_big_scope, done_scope, refresh_scope },
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_TRIGGER, "Trigger", evt_trigger_handler);
  //gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_gldisplay(void) {
  setup_tables();
  setup_class();
}
