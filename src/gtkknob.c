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

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gtkknob.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#define SCROLL_DELAY_LENGTH	300
#define KNOB_SIZE		32

#define STATE_IDLE		0
#define STATE_PRESSED		1
#define STATE_DRAGGING		2


/* Forward declarations */

static void gtk_knob_class_init(GtkKnobClass *klass);
static void gtk_knob_init(GtkKnob *knob);
static void gtk_knob_destroy(GtkObject *object);
static void gtk_knob_realize(GtkWidget *widget);
static void gtk_knob_size_request(GtkWidget *widget, GtkRequisition *requisition);
static void gtk_knob_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_knob_expose(GtkWidget *widget, GdkEventExpose *event);
static gint gtk_knob_button_press(GtkWidget *widget, GdkEventButton *event);
static gint gtk_knob_button_release(GtkWidget *widget, GdkEventButton *event);
static gint gtk_knob_motion_notify(GtkWidget *widget, GdkEventMotion *event);
static gint gtk_knob_timer(GtkKnob *knob);

static void gtk_knob_update_mouse_update(GtkKnob *knob);
static void gtk_knob_update_mouse(GtkKnob *knob, gint x, gint y);
static void gtk_knob_update_mouse_abs(GtkKnob *knob, gint x, gint y);
static void gtk_knob_update(GtkKnob *knob);
static void gtk_knob_adjustment_changed(GtkAdjustment *adjustment, gpointer data);
static void gtk_knob_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;
static GList *animation = NULL;

GdkPixbuf *get_empty_frame( GdkPixbufAnimation *anim ) {

    return gdk_pixbuf_new( GDK_COLORSPACE_RGB, TRUE, 8,
	    gdk_pixbuf_animation_get_width( anim ),
	    gdk_pixbuf_animation_get_height( anim )      );
}

GList *get_anim_list( char *name ) {

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

void free_anim_list( GList *anim_list )
{
    // g_list_foreach( anim_list, gdk_pixbuf_unref );
}


guint gtk_knob_get_type(void) {
  static guint knob_type = 0;

  if (!knob_type) {
    GtkTypeInfo knob_info = {
      "GtkKnob",
      sizeof (GtkKnob),
      sizeof (GtkKnobClass),
      (GtkClassInitFunc) gtk_knob_class_init,
      (GtkObjectInitFunc) gtk_knob_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };

    knob_type = gtk_type_unique(gtk_widget_get_type(), &knob_info);
  }

  return knob_type;
}

static void gtk_knob_class_init (GtkKnobClass *class) {
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  animation = get_anim_list( PIXMAPDIRIFY("knob.gif") );
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class(gtk_widget_get_type());

  object_class->destroy = gtk_knob_destroy;

  widget_class->realize = gtk_knob_realize;
  widget_class->expose_event = gtk_knob_expose;
  widget_class->size_request = gtk_knob_size_request;
  widget_class->size_allocate = gtk_knob_size_allocate;
  widget_class->button_press_event = gtk_knob_button_press;
  widget_class->button_release_event = gtk_knob_button_release;
  widget_class->motion_notify_event = gtk_knob_motion_notify;
}

static void gtk_knob_init (GtkKnob *knob) {
  knob->policy = GTK_UPDATE_CONTINUOUS;
  knob->state = STATE_IDLE;
  knob->saved_x = knob->saved_y = 0;
  knob->timer = 0;
  knob->pixbuf = NULL;
  knob->anim_list = animation;
  knob->old_value = 0.0;
  knob->old_lower = 0.0;
  knob->old_upper = 0.0;
  knob->adjustment = NULL;
}





GtkWidget *gtk_knob_new(GtkAdjustment *adjustment) {
  GtkKnob *knob;

  knob = gtk_type_new(gtk_knob_get_type());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_knob_set_adjustment(knob, adjustment);

  return GTK_WIDGET(knob);
}

static void gtk_knob_destroy(GtkObject *object) {
  GtkKnob *knob;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_KNOB(object));

  knob = GTK_KNOB(object);

  if (knob->adjustment)
    gtk_object_unref(GTK_OBJECT(knob->adjustment));

  if (knob->pixbuf)
    gdk_pixbuf_unref(knob->pixbuf);
  if (knob->anim_list)
    free_anim_list(knob->anim_list);

  if (GTK_OBJECT_CLASS(parent_class)->destroy)
    (*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

GtkAdjustment* gtk_knob_get_adjustment(GtkKnob *knob) {
  g_return_val_if_fail(knob != NULL, NULL);
  g_return_val_if_fail(GTK_IS_KNOB(knob), NULL);

  return knob->adjustment;
}

void gtk_knob_set_update_policy(GtkKnob *knob, GtkUpdateType policy) {
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GTK_IS_KNOB (knob));

  knob->policy = policy;
}

void gtk_knob_set_adjustment(GtkKnob *knob, GtkAdjustment *adjustment) {
  g_return_if_fail (knob != NULL);
  g_return_if_fail (GTK_IS_KNOB (knob));

  if (knob->adjustment) {
    gtk_signal_disconnect_by_data(GTK_OBJECT(knob->adjustment), (gpointer)knob);
    gtk_object_unref(GTK_OBJECT(knob->adjustment));
  }

  knob->adjustment = adjustment;
  gtk_object_ref(GTK_OBJECT(knob->adjustment));

  gtk_signal_connect(GTK_OBJECT(adjustment), "changed",
		     GTK_SIGNAL_FUNC(gtk_knob_adjustment_changed), (gpointer) knob);
  gtk_signal_connect(GTK_OBJECT(adjustment), "value_changed",
		     GTK_SIGNAL_FUNC(gtk_knob_adjustment_value_changed), (gpointer) knob);

  knob->old_value = adjustment->value;
  knob->old_lower = adjustment->lower;
  knob->old_upper = adjustment->upper;

  gtk_knob_update(knob);
}

static void gtk_knob_realize(GtkWidget *widget) {
  GtkKnob *knob;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_KNOB(widget));

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  knob = GTK_KNOB(widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask =
    gtk_widget_get_events (widget) | 
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

  widget->style = gtk_style_attach(widget->style, widget->window);

  gdk_window_set_user_data(widget->window, widget);


  gtk_style_set_background(widget->style, widget->window, GTK_STATE_ACTIVE);
}

static void gtk_knob_size_request (GtkWidget *widget, GtkRequisition *requisition) {
  requisition->width = KNOB_SIZE;
  requisition->height = KNOB_SIZE;
}

static void gtk_knob_size_allocate (GtkWidget *widget, GtkAllocation *allocation) {
  GtkKnob *knob;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_KNOB(widget));
  g_return_if_fail(allocation != NULL);

  widget->allocation = *allocation;
  knob = GTK_KNOB(widget);

  if (GTK_WIDGET_REALIZED(widget)) {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
  }
}

static gint gtk_knob_expose(GtkWidget *widget, GdkEventExpose *event) {
  GtkKnob *knob;
  gfloat dx, dy;
  GList *framelist;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
        
  knob = GTK_KNOB(widget);

  gdk_window_clear_area(widget->window, 0, 0, widget->allocation.width, widget->allocation.height);

  //gdk_draw_pixmap(widget->window, widget->style->bg_gc[widget->state], knob->pixmap,
  //		  0, 0, 0, 0, KNOB_SIZE, KNOB_SIZE);
  //
  dx = knob->adjustment->value - knob->adjustment->lower;
  dy = knob->adjustment->upper - knob->adjustment->lower;
  framelist = knob->anim_list;

  if (dy != 0) {
    GdkPixbuf *pixbuf;

    dx = MIN(MAX(dx / dy, 0), 1);
    dx = (1-dx) * (g_list_length( framelist )-0.5);

    pixbuf = g_list_nth_data( framelist, (int) dx);

    //tmpbuf = gdk_pixbuf_scale_simple( pixbuf, KNOB_SIZE, KNOB_SIZE, GDK_INTERP_TILES );

    //gdk_pixbuf_render_to_drawable( pixbuf, widget->window, widget->style->bg_gc[widget->state], 
    //    0, 0, 0, 0, gdk_pixbuf_get_width( pixbuf ), gdk_pixbuf_get_height( pixbuf ), 0,0,0 );
   //     0, 0, 0, 0, KNOB_SIZE-1, KNOB_SIZE-1, 0,0,0 );
   //
    gdk_pixbuf_render_to_drawable_alpha( pixbuf, widget->window,
	    0, 0, 0, 0, gdk_pixbuf_get_width( pixbuf ), gdk_pixbuf_get_height( pixbuf ), GDK_PIXBUF_ALPHA_BILEVEL, 100, 0,0,0 );

    //gdk_draw_line(widget->window, widget->style->white_gc, 16, 16, dx, dy);
  }
  



  return FALSE;
}

static gint gtk_knob_button_press(GtkWidget *widget, GdkEventButton *event) {
  GtkKnob *knob;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  knob = GTK_KNOB(widget);

  switch (knob->state) {
    case STATE_IDLE:
      switch (event->button) {
#if 0
	case 3:
	  gtk_knob_update_mouse_abs(knob, event->x, event->y);
	  /* FALL THROUGH */
#endif

	case 1:
	case 3:
	  gtk_grab_add(widget);
	  knob->state = STATE_PRESSED;
	  knob->saved_x = event->x;
	  knob->saved_y = event->y;
	  break;

	default:
	  break;
      }
      break;

    default:
      break;
  }

  return FALSE;
}

static gint gtk_knob_button_release(GtkWidget *widget, GdkEventButton *event) {
  GtkKnob *knob;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  knob = GTK_KNOB(widget);

  switch (knob->state) {
    case STATE_PRESSED:
      gtk_grab_remove(widget);
      knob->state = STATE_IDLE;

      switch (event->button) {
	case 1:
	  knob->adjustment->value -= knob->adjustment->page_increment;
	  gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
	  break;

	case 3:
	  knob->adjustment->value += knob->adjustment->page_increment;
	  gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
	  break;

	default:
	  break;
      }
      break;

    case STATE_DRAGGING:
      gtk_grab_remove(widget);
      knob->state = STATE_IDLE;

      if (knob->policy != GTK_UPDATE_CONTINUOUS && knob->old_value != knob->adjustment->value)
	gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");

      break;

    default:
      break;
  }

  return FALSE;
}

static gint gtk_knob_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
  GtkKnob *knob;
  GdkModifierType mods;
  gint x, y;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  knob = GTK_KNOB(widget);

  x = event->x;
  y = event->y;

  if (event->is_hint || (event->window != widget->window))
    gdk_window_get_pointer(widget->window, &x, &y, &mods);

  switch (knob->state) {
    case STATE_PRESSED:
      knob->state = STATE_DRAGGING;
      /* fall through */

    case STATE_DRAGGING:
      if (mods & GDK_BUTTON1_MASK) {
	gtk_knob_update_mouse(knob, x, y);
	return TRUE;
      } else if (mods & GDK_BUTTON3_MASK) {
	gtk_knob_update_mouse_abs(knob, x, y);
	return TRUE;
      }
      break;

    default:
      break;
  }

  return FALSE;
}

static gint gtk_knob_timer(GtkKnob *knob) {
  g_return_val_if_fail(knob != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(knob), FALSE);

  if (knob->policy == GTK_UPDATE_DELAYED)
    gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");

  return FALSE;	/* don't keep running this timer */
}

static void gtk_knob_update_mouse_update(GtkKnob *knob) {
  if (knob->policy == GTK_UPDATE_CONTINUOUS)
    gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
  else {
    gtk_widget_draw(GTK_WIDGET(knob), NULL);

    if (knob->policy == GTK_UPDATE_DELAYED) {
      if (knob->timer)
	gtk_timeout_remove(knob->timer);

      knob->timer = gtk_timeout_add (SCROLL_DELAY_LENGTH, (GtkFunction) gtk_knob_timer,
				     (gpointer) knob);
    }
  }
}

static void gtk_knob_update_mouse(GtkKnob *knob, gint x, gint y) {
  gfloat old_value;
  gfloat dv;

  g_return_if_fail(knob != NULL);
  g_return_if_fail(GTK_IS_KNOB(knob));

  old_value = knob->adjustment->value;

  dv = (knob->saved_y - y) * knob->adjustment->step_increment;
  knob->saved_x = x;
  knob->saved_y = y;

  knob->adjustment->value += dv;

  if (knob->adjustment->value != old_value)
    gtk_knob_update_mouse_update(knob);
}

static void gtk_knob_update_mouse_abs(GtkKnob *knob, gint x, gint y) {
  gfloat old_value;
  gdouble angle;

  g_return_if_fail(knob != NULL);
  g_return_if_fail(GTK_IS_KNOB(knob));

  old_value = knob->adjustment->value;

  x -= KNOB_SIZE>>1;
  y -= KNOB_SIZE>>1;
  y = -y;	/* inverted cartesian graphics coordinate system */

  angle = atan2(y, x) / M_PI;
  if (angle < -0.5)
    angle += 2;

  angle = -(2.0/3.0) * (angle - 1.25);	/* map [1.25pi, -0.25pi] onto [0, 1] */
  angle *= knob->adjustment->upper - knob->adjustment->lower;
  angle += knob->adjustment->lower;

  knob->adjustment->value = angle;

  if (knob->adjustment->value != old_value)
    gtk_knob_update_mouse_update(knob);
}

static void gtk_knob_update(GtkKnob *knob) {
  gfloat new_value;
        
  g_return_if_fail(knob != NULL);
  g_return_if_fail(GTK_IS_KNOB (knob));

  new_value = knob->adjustment->value;
        
  if (new_value < knob->adjustment->lower)
    new_value = knob->adjustment->lower;

  if (new_value > knob->adjustment->upper)
    new_value = knob->adjustment->upper;

  if (new_value != knob->adjustment->value) {
    knob->adjustment->value = new_value;
    gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
  }

  gtk_widget_draw(GTK_WIDGET(knob), NULL);
}

static void gtk_knob_adjustment_changed(GtkAdjustment *adjustment, gpointer data) {
  GtkKnob *knob;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);

  knob = GTK_KNOB(data);

  if ((knob->old_value != adjustment->value) ||
      (knob->old_lower != adjustment->lower) ||
      (knob->old_upper != adjustment->upper)) {
    gtk_knob_update (knob);

    knob->old_value = adjustment->value;
    knob->old_lower = adjustment->lower;
    knob->old_upper = adjustment->upper;
  }
}

static void gtk_knob_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data) {
  GtkKnob *knob;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);

  knob = GTK_KNOB(data);

  if (knob->old_value != adjustment->value) {
    gtk_knob_update (knob);

    knob->old_value = adjustment->value;
  }
}
