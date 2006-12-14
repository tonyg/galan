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

/* yep GPL apllies */

/* modified to be a scope plugin... requires changes to galan... but only
 * because of me being lazy #include "sample-display.c" should do the job..
 *
 * but because sample display is so powerful i plan to use it for sample input
 * to... (well.. should be in rart.c for loops)
 *
 * missing features:
 *  - resizing
 *  - you know what a hardware scope can do :-)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#include "libgnomecanvas/gnome-canvas.h"
#include "libgnomecanvas/gnome-canvas-rect-ellipse.h"

#define GENERATOR_CLASS_NAME	"canvascontrol"
#define GENERATOR_CLASS_PATH	"Control/CanvasControl"
#define GENERATOR_CLASS_PIXMAP	"template.xpm"

#define SIG_INPUT		1
#define SIG_OUTPUT		0

#define EVT_PLAY		0
#define EVT_STEP		1
#define EVT_EDIT		2
#define EVT_TRANSPOSE		3
#define EVT_NOTELEN		4
#define NUM_EVENT_INPUTS	5

#define EVT_NOTE_ON		0
#define EVT_NOTE_OFF		1
#define NUM_EVENT_OUTPUTS	2

#define X_SIZE 32
#define Y_SIZE 20
#define NUM 16

typedef struct Data {

  gdouble x,y;

  gint32 *seq;
  gint32 *matrix;

  gint32 w,h,num;

  gint32 transpose, notelen;

  gint32 playing, editing;
} Data;

typedef struct GroupData {

    GList *children;
} GroupData;

typedef struct ItemData {

    gdouble press_x, press_y;
    gdouble x,y;
    gdouble len;
    gboolean dragging;
    gboolean length_drag;
    

} ItemData;


PRIVATE gboolean init_instance(Generator *g) {

    int i;
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->y = 2.0;
  data->x = 2.0;

  data->w = X_SIZE;
  data->h = Y_SIZE;
  data->num = NUM;

  data->playing = 0;
  data->editing = 0;

  data->notelen = SAMPLE_RATE / 10;
  data->transpose = 32;

  data->matrix = safe_malloc( sizeof( gint32 ) * data->w * data->h * data->num );
  for( i=0; i<data->w * data->h * data->num; i++ )  data->matrix[i] = 0;

  data->seq = &(data->matrix[0]);

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;

    free( data->matrix );
    free(data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
	
    int i;
  gint32 binarylength;
  gint32 *file_buffer;
  Data *data = safe_malloc(sizeof(Data));

  g->data = data;

  data->w = objectstore_item_get_integer(item, "canvas_width", X_SIZE);
  data->h = objectstore_item_get_integer(item, "canvas_height", Y_SIZE);
  data->num = objectstore_item_get_integer(item, "canvas_num", NUM);

  data->notelen = objectstore_item_get_integer( item, "canvas_notelen", SAMPLE_RATE / 10 );
  data->transpose = objectstore_item_get_integer( item, "canvas_transpose", 32 );

  data->matrix = safe_malloc( sizeof( gint32 ) * data->w * data->h * data->num );

  binarylength = objectstore_item_get_binary(item, "matrix", (void **) (&file_buffer));
  if( binarylength == sizeof( gint32 ) * data->w * data->h * data->num )
      for( i=0; i<data->w * data->h * data->num; i++ ) {
	  data->matrix[i] = file_buffer[i];
      }
  else
      for( i=0; i<data->w * data->h * data->num; i++ ) {
	  data->matrix[i] = 0;
      }

  data->seq = &(data->matrix[data->editing * data->w * data->h]);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = g->data;
  objectstore_item_set_integer(item, "canvas_width", data->w);
  objectstore_item_set_integer(item, "canvas_height", data->h);
  objectstore_item_set_integer(item, "canvas_num", data->num);
  objectstore_item_set_integer(item, "canvas_notelen", data->notelen);
  objectstore_item_set_integer(item, "canvas_transpose", data->transpose);
  objectstore_item_set(item, "matrix",
	  objectstore_datum_new_binary(data->w * data->h * data->num * sizeof(gint32), (void *) data->matrix));
}

PRIVATE gint rect_event( GnomeCanvasItem *item, GdkEvent *event, gpointer user_data ) {

    ItemData *idata = g_object_get_data( G_OBJECT(item), "idata" );
    Control *c = g_object_get_data( G_OBJECT(item), "control" );
    Data *data = c->g->data;




    switch( event->type ) {
	case GDK_ENTER_NOTIFY:
	    gnome_canvas_item_set( item, "outline_color", "red", NULL );
	    break;

	case GDK_LEAVE_NOTIFY:
	    gnome_canvas_item_set( item, "outline_color", "black", NULL );
	    break;

	case GDK_BUTTON_PRESS:
	    {
		GdkEventButton *bev = event;

		switch( bev->button ) {
		    case 1:
			idata->dragging = TRUE;
			gdk_event_get_coords( event, &(idata->press_x), &(idata->press_y) );
			idata->press_x -= idata->x;
			idata->press_y -= idata->y;
			gnome_canvas_item_grab( item, GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK| GDK_BUTTON_RELEASE_MASK, NULL, GDK_CURRENT_TIME );
			break;
		    case 3:
			idata->length_drag = TRUE;
			gdk_event_get_coords( event, &(idata->press_x), &(idata->press_y) );
			idata->press_x -= idata->x;
			idata->press_y -= idata->y;
			gnome_canvas_item_grab( item, GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK| GDK_BUTTON_RELEASE_MASK, NULL, GDK_CURRENT_TIME );

			break;
		    default:
			break;

		}
		return TRUE;
		break;
	    }

	case GDK_BUTTON_RELEASE:
	    idata->dragging = FALSE;
	    idata->length_drag = FALSE;
	    gnome_canvas_item_ungrab( item, GDK_CURRENT_TIME );
	    break;

	case GDK_MOTION_NOTIFY:
	    if(idata->dragging) {
		gdouble x,y;
		gdk_event_get_coords( event, &(x), &(y) );

		x -= idata->press_x;
		y -= idata->press_y;

		x = rint(x/10)*10;
		y = rint(y/10)*10;

		if( (x != idata->x) || (y !=idata->y) ) {

		    gnome_canvas_item_move( item, x - idata->x, y - idata->y );
		    if( (idata->x>=0) && (idata->x<(data->w) * 10) && (idata->y>=0) && (idata->y<data->h * 10) ) 
			data->seq[ lrint(idata->x/10) * data->h + lrint(idata->y/10) ] = 0;

		    if( (x>=0) && (x<(data->w) * 10) && (y>=0) && (y<(data->h) * 10) ) 
			data->seq[ lrint(x/10) * data->h + lrint(y/10) ] = idata->len;

		    idata->x = x;
		    idata->y =  y;
		    gen_update_controls( c->g, -1 );
		}
	    } else if( idata->length_drag ) {
		gdouble x,y;
		gdk_event_get_coords( event, &(x), &(y) );

		x -= idata->x;

		x = rint(x/10)*10;

		if( (x > 0) && x != idata->len ) {

		    idata->len = x;
		    gnome_canvas_item_set( item, "x2", idata->len, NULL );
		    if( (x>=0) && (x<data->w*10) && (y>=0) && (y<data->h*10) ) 
			data->seq[ (int)(idata->x/10) * data->h + (int)(idata->y/10) ] = idata->len;

		    gen_update_controls( c->g, -1 );
		}
	    }

	default:
	    break;
    }
    return FALSE;
}


PRIVATE void createItem( GnomeCanvasItem *parent, int x, int y, int w ) {

    Control *c = g_object_get_data( G_OBJECT(parent), "control" );
    GroupData *gdata = g_object_get_data( G_OBJECT(parent), "gdata" );

    GnomeCanvasItem *it = gnome_canvas_item_new( parent, gnome_canvas_rect_get_type(), 
	    "outline_color", "black", 
	    "fill_color", "white", 
	    "x1", (gdouble)0.0, 
	    "x2", (gdouble)w, 
	    "y1", (gdouble)0.0, 
	    "y2", (gdouble)10.0, 
	    NULL );

    ItemData *idata = safe_malloc( sizeof( ItemData ) );
    idata->dragging = FALSE;
    idata->length_drag = FALSE;
    idata->x = x; 
    idata->y = y;
    idata->len = w;

    g_object_set_data( G_OBJECT( it ), "idata", idata );
    g_object_set_data( G_OBJECT( it ), "control", c);

    gdata->children = g_list_append( gdata->children, it );

    g_signal_connect( G_OBJECT( it ), "event", G_CALLBACK( rect_event ), NULL );

    gnome_canvas_item_move( it, x, y );
}

PRIVATE gint canvas_event( GnomeCanvasItem *item, GdkEvent *event, gpointer user_data ) {

    Control *c = g_object_get_data( G_OBJECT(item), "control" );
    Data *data = c->g->data;

    switch( event->type ) {
	case GDK_BUTTON_PRESS:
	    {
		GdkEventButton *bev = event;

		switch( bev->button ) {
		    case 1:
			{
			// TODO: create new item

			    gdouble x,y;
			    gdk_event_get_coords( event, &(x), &(y) );
			    x = rint(x/10-0.5)*10;
			    y = rint(y/10-0.5)*10;

			    createItem( item, x, y, 10 );

			    if( (x>=0) && (x<data->w * 10) && (y>=0) && (y<data->h * 10) ) 
				data->seq[ (int)(x/10) * data->h + (int)(y/10) ] = 10;


			gen_update_controls( c->g, -1 );

			break;
			}
		    case 3:

			break;
		    default:
			break;

		}
                break;
	    }
	default:
	    break;
    }
}



PRIVATE void init_canvas( Control *control ) {

	GtkWidget *canvas = gnome_canvas_new();
	
	Data *data = control->g->data;
	GnomeCanvasGroup *root_group = gnome_canvas_root( GNOME_CANVAS(canvas) );

	GroupData *gdata = safe_malloc( sizeof( GroupData ) );
	
	gdata->children = NULL;

	g_object_set_data( G_OBJECT( root_group ), "gdata", gdata );
		
	g_object_set_data( G_OBJECT( root_group ), "control", control );

	
	// the back ground
	gnome_canvas_item_new( root_group, gnome_canvas_rect_get_type(), 
		"fill_color", "grey", 
		"x1", (gdouble)0.0, 
		"x2", (gdouble)data->w * 10, 
		"y1", (gdouble)0.0, 
		"y2", (gdouble)data->h * 10, 
		NULL );

	
	// set event callback
	g_signal_connect( G_OBJECT( root_group ), "event", G_CALLBACK( canvas_event ), NULL );

	//XXX: add scrolled window...
	gnome_canvas_set_scroll_region( GNOME_CANVAS( canvas ), 0, 0, data->w * 10, data->h*10 );
	
	gtk_widget_set_usize( canvas, data->w * 10 , data->h * 10 );

	control->widget = canvas;
	//control->data = it;
}

PRIVATE void done_canvas(Control *control) {

    //GObject *item = G_OBJECT( control->data );
    
    ItemData *gdata = g_object_get_data( G_OBJECT( gnome_canvas_root( GNOME_CANVAS(control->widget) ) ), "gdata" );

    //free the g_list, which should already be empty.
    free( gdata );
}

PRIVATE gboolean isItemInSeq( GnomeCanvasItem *it, gint32 *seq ) {

}

PRIVATE GnomeCanvasItem *findItemAt( int tx, int ty, GroupData *gdata ) {
    GList *childX;

    for( childX = gdata->children; childX != NULL; childX = g_list_next( childX ) ) {

	GnomeCanvasItem *it = childX->data;
	gdouble x,y;
	int ix, iy;

	gnome_canvas_item_get_bounds( it, &x, &y, NULL, NULL );
	ix = (int)(x+1) / 10;
	iy = (int)(y+1) / 10;

	if( ix == tx && iy == ty )
	    return it;
    }

    return NULL;
}

PRIVATE void refresh_canvas(Control *control) {
    Data *data = control->g->data;
    
    GtkWidget *canvas = control->widget;
    GroupData *gdata = g_object_get_data( G_OBJECT( gnome_canvas_root( control->widget ) ), "gdata" );

    GList *childX, *toDelete = NULL;

    int rx, ry;
    
    for( childX = gdata->children; childX != NULL; childX = g_list_next( childX ) ) {

	GnomeCanvasItem *it = childX->data;
	ItemData *idata = g_object_get_data( G_OBJECT(it), "idata" );
	gdouble x,y, x2;
	int ix, iy, iw;

	gnome_canvas_item_get_bounds( it, &x, &y, &x2, NULL );
	ix = (int)(x+1) / 10;
	iy = (int)(y+1) / 10;
	iw = (int)(x2) / 10 - ix;
	
	if( data->seq[ix * data->h + iy] == 0 )
	    toDelete = g_list_append( toDelete, it ); 
	else if( data->seq[ix *data->h + iy ] != iw * 10) {
	    idata->len = data->seq[ix * data->h + iy ];
	    gnome_canvas_item_set( it, "x2", (gdouble) data->seq[ix * data->h + iy ], NULL );
	}
	    
	
    }

    for( childX = toDelete; childX != NULL; childX = g_list_next( childX ) ) {
	GnomeCanvasItem *it = childX->data;
	gdata->children = g_list_remove( gdata->children, it );
	gtk_object_destroy( it );
   }

    g_list_free( toDelete );


    for( rx=0; rx<data->w; rx++ )
	for( ry=0; ry<data->h; ry++ ) {
	    if( data->seq[ rx*data->h + ry ] != 0 ) {
		if( findItemAt( rx, ry, gdata ) == NULL ) {

		    createItem( gnome_canvas_root( canvas ), rx*10, ry*10, data->seq[ rx*data->h + ry ] );
		}
	    }
	}
}


PRIVATE void evt_play_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  data->playing = (int) event->d.number;
}

PRIVATE void evt_edit_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  data->editing = (int) event->d.number;
  data->seq = &(data->matrix[data->editing * data->w * data->h] );
  gen_update_controls( g, -1 );
}

PRIVATE void evt_step_handler(Generator *g, AEvent *event) {

    int i;
    Data *data = g->data;

    SAMPLETIME time = event->time;
    //data->y = event->d.number;

    int pos = lrint(event->d.number);
    gint32 *ply = &(data->matrix[ data->playing * data->w * data->h ]);

    for( i=0; i<data->h; i++ ) {
	if( ply[pos*data->h+i] > 0 ) {

	    event->time = time;
	    event->d.number = i + data->transpose;
	    gen_send_events(g, EVT_NOTE_ON, -1, event);

	    event->time = time + ply[pos*data->h+i] / 10 * data->notelen;
	    event->d.number = i + data->transpose;
	    gen_send_events(g, EVT_NOTE_OFF, -1, event);
	    
	}
    }
    //gen_update_controls( g, -1 );
}

PRIVATE void evt_transpose_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  data->transpose = (int) event->d.number;
  //XXX: if canvas displays keyboard, update is necessary.
  //gen_update_controls( g, -1 );
}

PRIVATE void evt_notelen_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  data->notelen = (int) (event->d.number * SAMPLE_RATE);
  //XXX: if canvas displays keyboard, update is necessary.
  //gen_update_controls( g, -1 );
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_USERDEF, "canvas", 0,0,0,0, 0,FALSE, 0,0, init_canvas, done_canvas, refresh_canvas },
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

  gen_configure_event_input(k, EVT_PLAY, "Play", evt_play_handler);
  gen_configure_event_input(k, EVT_STEP, "Step", evt_step_handler);
  gen_configure_event_input(k, EVT_EDIT, "Edit", evt_edit_handler);
  gen_configure_event_input(k, EVT_TRANSPOSE, "Transpose", evt_transpose_handler);
  gen_configure_event_input(k, EVT_NOTELEN, "NoteLen", evt_notelen_handler);
  gen_configure_event_output(k, EVT_NOTE_ON, "NoteOn");
  gen_configure_event_output(k, EVT_NOTE_OFF, "NoteOff");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
				  NULL);
}

PUBLIC void init_plugin_canvascontrol(void) {
  setup_class();
}
