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

#include "gtkslider.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#define SCROLL_DELAY_LENGTH	300
#define SLIDER_WIDTH		32
#define DEFAULT_SLIDER_SIZE	100	/* travel distance, in pixels */
#define SLIDER_GAP		12

#define STATE_IDLE		0
#define STATE_PRESSED		1
#define STATE_DRAGGING		2

/* XPM */
static char * slider_xpm[] = {
"24 8 94 2",
"  	c None",
". 	c #EBEBEB",
"+ 	c #EAEAEA",
"@ 	c #E5E5E5",
"# 	c #E4E4E4",
"$ 	c #E2E2E2",
"% 	c #E3E3E3",
"& 	c #E6E6E6",
"* 	c #E9E9E9",
"= 	c #E7E7E7",
"- 	c #E8E8E8",
"; 	c #E1E1E1",
"> 	c #E0E0E0",
", 	c #DBDBDB",
"' 	c #D4D4D4",
") 	c #C7C7C7",
"! 	c #D9D9D9",
"~ 	c #D5D5D5",
"{ 	c #D7D7D7",
"] 	c #D8D8D8",
"^ 	c #DDDDDD",
"/ 	c #DADADA",
"( 	c #D6D6D6",
"_ 	c #D0D0D0",
": 	c #CECECE",
"< 	c #D1D1D1",
"[ 	c #CBCBCB",
"} 	c #BDBDBD",
"| 	c #AEAEAE",
"1 	c #EEEEEE",
"2 	c #C4C4C4",
"3 	c #BBBBBB",
"4 	c #C0C0C0",
"5 	c #BFBFBF",
"6 	c #C2C2C2",
"7 	c #C1C1C1",
"8 	c #BEBEBE",
"9 	c #BCBCBC",
"0 	c #B4B4B4",
"a 	c #999999",
"b 	c #868686",
"c 	c #AFAFAF",
"d 	c #A3A3A3",
"e 	c #858585",
"f 	c #7F7F7F",
"g 	c #7D7D7D",
"h 	c #6F6F6F",
"i 	c #616161",
"j 	c #DCDCDC",
"k 	c #CCCCCC",
"l 	c #C9C9C9",
"m 	c #ADADAD",
"n 	c #929292",
"o 	c #B6B6B6",
"p 	c #A6A6A6",
"q 	c #A4A4A4",
"r 	c #A5A5A5",
"s 	c #A8A8A8",
"t 	c #A7A7A7",
"u 	c #A9A9A9",
"v 	c #9E9E9E",
"w 	c #9F9F9F",
"x 	c #A1A1A1",
"y 	c #959595",
"z 	c #555555",
"A 	c #A2A2A2",
"B 	c #919191",
"C 	c #8B8B8B",
"D 	c #898989",
"E 	c #878787",
"F 	c #888888",
"G 	c #8D8D8D",
"H 	c #8C8C8C",
"I 	c #8A8A8A",
"J 	c #848484",
"K 	c #797979",
"L 	c #696969",
"M 	c #5C5C5C",
"N 	c #B1B1B1",
"O 	c #939393",
"P 	c #818181",
"Q 	c #7B7B7B",
"R 	c #787878",
"S 	c #747474",
"T 	c #6C6C6C",
"U 	c #6A6A6A",
"V 	c #727272",
"W 	c #717171",
"X 	c #737373",
"Y 	c #707070",
"Z 	c #6E6E6E",
"` 	c #646464",
" .	c #686868",
"..	c #666666",
". + @ @ # # $ $ % & * * & & = - @ ; > ; > , ' ) ",
". @ ! ~ { { ] ! ] , ^ ^ / ] ! ! ( _ : _ < [ } | ",
"1 % 2 3 } } 4 5 4 4 6 7 8 3 8 8 9 9 9 } } 0 a b ",
"c d e f f f f f f f f f f f f f f f f f f g h i ",
"j ] : k k k k k k k k k k k k k k k k k k l m n ",
"+ { o p q d d r r s t u t t r d v v w x v y h z ",
"j ) A B C D D E E F G H C H C I E J J e f K L M ",
"l N O P Q R S T U h V V W W X X Y Z W U `  ...` "};

/* Forward declarations */

static void gtk_slider_class_init(GtkSliderClass *klass);
static void gtk_slider_init(GtkSlider *slider);
static void gtk_slider_destroy(GtkObject *object);
static void gtk_slider_realize(GtkWidget *widget);
static void gtk_slider_size_request(GtkWidget *widget, GtkRequisition *requisition);
static void gtk_slider_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_slider_expose(GtkWidget *widget, GdkEventExpose *event);
static gint gtk_slider_button_press(GtkWidget *widget, GdkEventButton *event);
static gint gtk_slider_button_release(GtkWidget *widget, GdkEventButton *event);
static gint gtk_slider_motion_notify(GtkWidget *widget, GdkEventMotion *event);
static gint gtk_slider_timer(GtkSlider *slider);

static void gtk_slider_update_mouse_update(GtkSlider *slider);
static void gtk_slider_update_mouse(GtkSlider *slider, gint x, gint y);
static void gtk_slider_update_mouse_abs(GtkSlider *slider, gint x, gint y);
static void gtk_slider_update(GtkSlider *slider);
static void gtk_slider_adjustment_changed(GtkAdjustment *adjustment, gpointer data);
static void gtk_slider_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

guint gtk_slider_get_type(void) {
  static guint slider_type = 0;

  if (!slider_type) {
    GtkTypeInfo slider_info = {
      "GtkSlider",
      sizeof (GtkSlider),
      sizeof (GtkSliderClass),
      (GtkClassInitFunc) gtk_slider_class_init,
      (GtkObjectInitFunc) gtk_slider_init,
      NULL,
      NULL,
    };

    slider_type = gtk_type_unique(gtk_widget_get_type(), &slider_info);
  }

  return slider_type;
}

static void gtk_slider_class_init (GtkSliderClass *class) {
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class(gtk_widget_get_type());

  object_class->destroy = gtk_slider_destroy;

  widget_class->realize = gtk_slider_realize;
  widget_class->expose_event = gtk_slider_expose;
  widget_class->size_request = gtk_slider_size_request;
  widget_class->size_allocate = gtk_slider_size_allocate;
  widget_class->button_press_event = gtk_slider_button_press;
  widget_class->button_release_event = gtk_slider_button_release;
  widget_class->motion_notify_event = gtk_slider_motion_notify;
}

static void gtk_slider_init (GtkSlider *slider) {
  slider->policy = GTK_UPDATE_CONTINUOUS;
  slider->state = STATE_IDLE;
  slider->saved_x = slider->saved_y = 0;
  slider->timer = 0;
  slider->pixmap = NULL;
  slider->size = DEFAULT_SLIDER_SIZE;
  slider->old_value = 0.0;
  slider->old_lower = 0.0;
  slider->old_upper = 0.0;
  slider->adjustment = NULL;
}

GtkWidget *gtk_slider_new(GtkAdjustment *adjustment, gint size) {
  GtkSlider *slider;

  slider = gtk_type_new(gtk_slider_get_type());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);


  if (size == 0)
    size = DEFAULT_SLIDER_SIZE;

  slider->size = size;

  gtk_slider_set_adjustment(slider, adjustment);

  return GTK_WIDGET(slider);
}

static void gtk_slider_destroy(GtkObject *object) {
  GtkSlider *slider;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_SLIDER(object));

  slider = GTK_SLIDER(object);

  if (slider->adjustment) {
    gtk_object_unref(GTK_OBJECT(slider->adjustment));
    slider->adjustment = NULL;
  }

  if (slider->pixmap) {
    gdk_pixmap_unref(slider->pixmap);
    slider->pixmap = NULL;
  }

  if (GTK_OBJECT_CLASS(parent_class)->destroy)
    (*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

GtkAdjustment* gtk_slider_get_adjustment(GtkSlider *slider) {
  g_return_val_if_fail(slider != NULL, NULL);
  g_return_val_if_fail(GTK_IS_SLIDER(slider), NULL);

  return slider->adjustment;
}

void gtk_slider_set_update_policy(GtkSlider *slider, GtkUpdateType policy) {
  g_return_if_fail (slider != NULL);
  g_return_if_fail (GTK_IS_SLIDER (slider));

  slider->policy = policy;
}

void gtk_slider_set_adjustment(GtkSlider *slider, GtkAdjustment *adjustment) {
  g_return_if_fail (slider != NULL);
  g_return_if_fail (GTK_IS_SLIDER (slider));

  if (slider->adjustment) {
    gtk_signal_disconnect_by_data(GTK_OBJECT(slider->adjustment), (gpointer)slider);
    gtk_object_unref(GTK_OBJECT(slider->adjustment));
  }

  slider->adjustment = adjustment;
  gtk_object_ref(GTK_OBJECT(slider->adjustment));
  gtk_object_sink(GTK_OBJECT( slider-> adjustment ) );

  gtk_signal_connect(GTK_OBJECT(adjustment), "changed",
		     GTK_SIGNAL_FUNC(gtk_slider_adjustment_changed), (gpointer) slider);
  gtk_signal_connect(GTK_OBJECT(adjustment), "value_changed",
		     GTK_SIGNAL_FUNC(gtk_slider_adjustment_value_changed), (gpointer) slider);

  slider->old_value = adjustment->value;
  slider->old_lower = adjustment->lower;
  slider->old_upper = adjustment->upper;

  gtk_slider_update(slider);
}

static void gtk_slider_realize(GtkWidget *widget) {
  GtkSlider *slider;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkBitmap *mask;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_SLIDER(widget));

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  slider = GTK_SLIDER(widget);

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

  widget->style = gtk_style_attach(widget->parent->style, widget->window);

  gdk_window_set_user_data(widget->window, widget);

  slider->pixmap = gdk_pixmap_colormap_create_from_xpm_d(widget->window, gdk_colormap_get_system(), &mask,
					      &widget->style->bg[GTK_STATE_NORMAL],
					      slider_xpm);

  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);
}

static void gtk_slider_size_request (GtkWidget *widget, GtkRequisition *requisition) {
  requisition->width = SLIDER_WIDTH;
  requisition->height = GTK_SLIDER(widget)->size + SLIDER_GAP * 2;
}

static void gtk_slider_size_allocate (GtkWidget *widget, GtkAllocation *allocation) {
  GtkSlider *slider;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_SLIDER(widget));
  g_return_if_fail(allocation != NULL);

  widget->allocation = *allocation;
  slider = GTK_SLIDER(widget);

  if (GTK_WIDGET_REALIZED(widget)) {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
  }
}

static gint gtk_slider_expose(GtkWidget *widget, GdkEventExpose *event) {
  GtkSlider *slider;
  gfloat dy;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_SLIDER(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
        
  slider = GTK_SLIDER(widget);

  gdk_window_clear_area(widget->window, 0, 0, widget->allocation.width, widget->allocation.height);

//  gdk_draw_rectangle(widget->window, widget->style->bg_gc[widget->state], TRUE,
//		     0, 0, widget->allocation.width, widget->allocation.height);

  gdk_draw_line(widget->window, widget->style->black_gc,
		SLIDER_WIDTH>>1, SLIDER_GAP, SLIDER_WIDTH>>1, SLIDER_GAP + slider->size);

  dy = slider->adjustment->upper - slider->adjustment->lower;

  if (dy != 0) {
    dy = (slider->adjustment->value - slider->adjustment->lower) / dy;
    dy = MIN(MAX(dy, 0), 1);
    dy = (1 - dy) * slider->size + SLIDER_GAP;

    gdk_draw_pixmap(widget->window, widget->style->bg_gc[widget->state], slider->pixmap,
		    0, 0, (SLIDER_WIDTH>>1)-12, dy-4, 24, 8);
  }

  return FALSE;
}

static gint gtk_slider_button_press(GtkWidget *widget, GdkEventButton *event) {
  GtkSlider *slider;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_SLIDER(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  slider = GTK_SLIDER(widget);

  switch (slider->state) {
    case STATE_IDLE:
      switch (event->button) {
#if 0
	case 2:
	  gtk_slider_update_mouse_abs(slider, event->x, event->y);
	  /* FALL THROUGH */
#endif

	case 1:
	case 3:
	  gtk_grab_add(widget);
	  slider->state = STATE_PRESSED;
	  slider->saved_x = event->x;
	  slider->saved_y = event->y;
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

static gint gtk_slider_button_release(GtkWidget *widget, GdkEventButton *event) {
  GtkSlider *slider;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_SLIDER(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  slider = GTK_SLIDER(widget);

  switch (slider->state) {
    case STATE_PRESSED:
      gtk_grab_remove(widget);
      slider->state = STATE_IDLE;

      switch (event->button) {
	case 1:
	  slider->adjustment->value -= slider->adjustment->page_increment;
	  gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");
	  break;

	case 3:
	  slider->adjustment->value += slider->adjustment->page_increment;
	  gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");
	  break;

	default:
	  break;
      }
      break;

    case STATE_DRAGGING:
      gtk_grab_remove(widget);
      slider->state = STATE_IDLE;

      if (slider->policy != GTK_UPDATE_CONTINUOUS && slider->old_value != slider->adjustment->value)
	gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");

      break;

    default:
      break;
  }

  return FALSE;
}

static gint gtk_slider_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
  GtkSlider *slider;
  GdkModifierType mods;
  gint x, y;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_SLIDER(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  slider = GTK_SLIDER(widget);

  x = event->x;
  y = event->y;

  if (event->is_hint || (event->window != widget->window))
    gdk_window_get_pointer(widget->window, &x, &y, &mods);

  switch (slider->state) {
    case STATE_PRESSED:
      slider->state = STATE_DRAGGING;
      /* fall through */

    case STATE_DRAGGING:
      if (mods & GDK_BUTTON1_MASK) {
	gtk_slider_update_mouse(slider, x, y);
	return TRUE;
      } else if (mods & GDK_BUTTON3_MASK) {
	gtk_slider_update_mouse_abs(slider, x, y);
	return TRUE;
      }
      break;

    default:
      break;
  }

  return FALSE;
}

static gint gtk_slider_timer(GtkSlider *slider) {
  g_return_val_if_fail(slider != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_SLIDER(slider), FALSE);

  if (slider->policy == GTK_UPDATE_DELAYED)
    gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");

  return FALSE;	/* don't keep running this timer */
}

static void gtk_slider_update_mouse_update(GtkSlider *slider) {
  if (slider->policy == GTK_UPDATE_CONTINUOUS)
    gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");
  else {
    gtk_widget_draw(GTK_WIDGET(slider), NULL);

    if (slider->policy == GTK_UPDATE_DELAYED) {
      if (slider->timer)
	gtk_timeout_remove(slider->timer);

      slider->timer = gtk_timeout_add (SCROLL_DELAY_LENGTH, (GtkFunction) gtk_slider_timer,
				       (gpointer) slider);
    }
  }
}

static void gtk_slider_update_mouse(GtkSlider *slider, gint x, gint y) {
  gfloat old_value;
  gfloat dv;

  g_return_if_fail(slider != NULL);
  g_return_if_fail(GTK_IS_SLIDER(slider));

  old_value = slider->adjustment->value;

  dv = (slider->saved_y - y) * slider->adjustment->step_increment;
  slider->saved_x = x;
  slider->saved_y = y;

  slider->adjustment->value += dv;

  if (slider->adjustment->value != old_value)
    gtk_slider_update_mouse_update(slider);
}

static void gtk_slider_update_mouse_abs(GtkSlider *slider, gint x, gint y) {
  gfloat old_value;
  gfloat dy;

  g_return_if_fail(slider != NULL);
  g_return_if_fail(GTK_IS_SLIDER(slider));

  old_value = slider->adjustment->value;

  y -= SLIDER_GAP;
  y = slider->size - y;

  dy = y / ((gfloat) slider->size);
  dy *= slider->adjustment->upper - slider->adjustment->lower;
  dy += slider->adjustment->lower;

  slider->adjustment->value = dy;

  if (slider->adjustment->value != old_value)
    gtk_slider_update_mouse_update(slider);
}

static void gtk_slider_update(GtkSlider *slider) {
  gfloat new_value;
        
  g_return_if_fail(slider != NULL);
  g_return_if_fail(GTK_IS_SLIDER (slider));

  new_value = slider->adjustment->value;
        
  if (new_value < slider->adjustment->lower)
    new_value = slider->adjustment->lower;

  if (new_value > slider->adjustment->upper)
    new_value = slider->adjustment->upper;

  if (new_value != slider->adjustment->value) {
    slider->adjustment->value = new_value;
    gtk_signal_emit_by_name(GTK_OBJECT(slider->adjustment), "value_changed");
  }

  gtk_widget_draw(GTK_WIDGET(slider), NULL);
}

static void gtk_slider_adjustment_changed(GtkAdjustment *adjustment, gpointer data) {
  GtkSlider *slider;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);

  slider = GTK_SLIDER(data);

  if ((slider->old_value != adjustment->value) ||
      (slider->old_lower != adjustment->lower) ||
      (slider->old_upper != adjustment->upper)) {
    gtk_slider_update (slider);

    slider->old_value = adjustment->value;
    slider->old_lower = adjustment->lower;
    slider->old_upper = adjustment->upper;
  }
}

static void gtk_slider_adjustment_value_changed (GtkAdjustment *adjustment, gpointer data) {
  GtkSlider *slider;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);

  slider = GTK_SLIDER(data);

  if (slider->old_value != adjustment->value) {
    gtk_slider_update (slider);

    slider->old_value = adjustment->value;
  }
}
