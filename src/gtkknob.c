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

#include "gtkknob.h"

#ifndef M_PI
# define M_PI		3.14159265358979323846	/* pi */
#endif

#define SCROLL_DELAY_LENGTH	300
#define KNOB_SIZE		32

#define STATE_IDLE		0
#define STATE_PRESSED		1
#define STATE_DRAGGING		2

/* XPM */
static char * knob_xpm[] = {
"32 32 133 2",
"  	c None",
". 	c #7F7F7F",
"+ 	c #4C4C4C",
"@ 	c #838383",
"# 	c #8A8A8A",
"$ 	c #9F9F9F",
"% 	c #BABABA",
"& 	c #CBCBCB",
"* 	c #CACACA",
"= 	c #B6B6B6",
"- 	c #9A9A9A",
"; 	c #888888",
"> 	c #989898",
", 	c #DDDDDD",
"' 	c #DCDCDC",
") 	c #D9D9D9",
"! 	c #D4D4D4",
"~ 	c #D1D1D1",
"{ 	c #CECECE",
"] 	c #CDCDCD",
"^ 	c #C6C6C6",
"/ 	c #8B8B8B",
"( 	c #868686",
"_ 	c #D7D7D7",
": 	c #E0E0E0",
"< 	c #DFDFDF",
"[ 	c #DBDBDB",
"} 	c #C7C7C7",
"| 	c #C4C4C4",
"1 	c #C3C3C3",
"2 	c #BFBFBF",
"3 	c #AEAEAE",
"4 	c #818181",
"5 	c #E1E1E1",
"6 	c #DADADA",
"7 	c #D2D2D2",
"8 	c #C8C8C8",
"9 	c #C2C2C2",
"0 	c #BEBEBE",
"a 	c #BDBDBD",
"b 	c #B9B9B9",
"c 	c #B5B5B5",
"d 	c #D0D0D0",
"e 	c #BBBBBB",
"f 	c #B4B4B4",
"g 	c #B3B3B3",
"h 	c #ADADAD",
"i 	c #A2A2A2",
"j 	c #8F8F8F",
"k 	c #808080",
"l 	c #D3D3D3",
"m 	c #CFCFCF",
"n 	c #B8B8B8",
"o 	c #B2B2B2",
"p 	c #AFAFAF",
"q 	c #A6A6A6",
"r 	c #969696",
"s 	c #949494",
"t 	c #B7B7B7",
"u 	c #B1B1B1",
"v 	c #AAAAAA",
"w 	c #9C9C9C",
"x 	c #898989",
"y 	c #7C7C7C",
"z 	c #D8D8D8",
"A 	c #A0A0A0",
"B 	c #8E8E8E",
"C 	c #929292",
"D 	c #9D9D9D",
"E 	c #919191",
"F 	c #7D7D7D",
"G 	c #787878",
"H 	c #B0B0B0",
"I 	c #ABABAB",
"J 	c #A1A1A1",
"K 	c #797979",
"L 	c #747474",
"M 	c #D5D5D5",
"N 	c #A8A8A8",
"O 	c #858585",
"P 	c #717171",
"Q 	c #6A6A6A",
"R 	c #A4A4A4",
"S 	c #7B7B7B",
"T 	c #686868",
"U 	c #636363",
"V 	c #909090",
"W 	c #616161",
"X 	c #C1C1C1",
"Y 	c #C0C0C0",
"Z 	c #6C6C6C",
"` 	c #595959",
" .	c #6E6E6E",
"..	c #656565",
"+.	c #535353",
"@.	c #979797",
"#.	c #575757",
"$.	c #4B4B4B",
"%.	c #A9A9A9",
"&.	c #A7A7A7",
"*.	c #4A4A4A",
"=.	c #707070",
"-.	c #A3A3A3",
";.	c #9B9B9B",
">.	c #ACACAC",
",.	c #848484",
"'.	c #6F6F6F",
").	c #494949",
"!.	c #434343",
"~.	c #828282",
"{.	c #7E7E7E",
"].	c #5A5A5A",
"^.	c #474747",
"/.	c #404040",
"(.	c #8C8C8C",
"_.	c #8D8D8D",
":.	c #939393",
"<.	c #727272",
"[.	c #555555",
"}.	c #484848",
"|.	c #3F3F3F",
"1.	c #7A7A7A",
"2.	c #757575",
"3.	c #767676",
"4.	c #5E5E5E",
"5.	c #545454",
"6.	c #5B5B5B",
"7.	c #5C5C5C",
"8.	c #5D5D5D",
"9.	c #4F4F4F",
"0.	c #464646",
"a.	c #3D3D3D",
"b.	c #393939",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . + + . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . + + . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ",
". . . . . . . . . . . @ # $ % & * = - ; @ . . . . . . . . . . . ",
". . . . . . . . . @ > , ' ) ! ~ { ] & * ^ / @ . . . . . . . . . ",
". . . . . . . . ( _ : < [ ! { * } ^ | 1 2 % 3 4 . . . . . . . . ",
". . . . . . . # , 5 : 6 7 8 9 0 a % b b b c 3 $ . . . . . . . . ",
". . . . . . ( [ 5 5 6 d 1 e = f f g g g g g h i j k . . . . . . ",
". . . . . @ l : : [ m 9 n g o o o o o o o o p q r @ @ . . . . . ",
". . . . . s [ , 6 m 1 t g o o o o o o o o o u v w x y . . . . . ",
". . . . @ _ [ z m 1 t o o o o o o o o o o o o h A B k @ . . . . ",
". . . . x ) 6 7 ^ b o o o o o o o o o o o o o p q C 4 . . . . . ",
". . . . D z _ ] 2 f o o o o o o o o o o o o o 3 q E F G . . . . ",
". . . . e _ ! 8 e g o o o o o o o o o o o o H I J B K L . . . . ",
". . . . ] M 7 ^ b g o o o o o o o o o o o u 3 N - O P Q . . . . ",
". . . . & l { 1 t o o o o o o o o o o o o H h R s S T U . . . . ",
". . . . g ] 8 2 f o o o o o o o o o o o o 3 v i V L W U . . . . ",
". . . . s X Y b o o o o o o o o o o o o H h N w # Z `  .. . . . ",
". . . . O c c o u u o o o o o o o o o o p v i r k ..+.y . . . . ",
". . . . @ u 3 I I p H o o o o u H H H 3 v i @.O Z #.$.@ . . . . ",
". . . . . x %.R R %.h H o o u p h I %.&.R r O  .` *.=.. . . . . ",
". . . . . @ -.D ;.A &.>.p H 3 I &.-.A w C ,.'.#.).!.@ . . . . . ",
". . . . . . ~.> C r - A q &.q R A @.E x {.Z ].^./.{.. . . . . . ",
". . . . . . . k (._./ B j C :.E (.( F <.U [.}.|.1.. . . . . . . ",
". . . . . + + . k 4 1.L <.P 2.3.=.Q 4.5.)./.|.{.. + + . . . . . ",
". . . . . + + . . @ K U 6.7.8.7.[.9.0.a.b.Z @ . . + + . . . . . ",
". . . . . . . . . . . @ y  .4.+.*.+...1.@ . . . . . . . . . . . ",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ",
". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . "};

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
  knob->pixmap = NULL;
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

  if (knob->pixmap)
    gdk_pixmap_unref(knob->pixmap);

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
  GdkBitmap *mask;

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

  knob->pixmap = gdk_pixmap_create_from_xpm_d(widget->window, &mask,
					      &widget->style->bg[GTK_STATE_ACTIVE],
					      knob_xpm);

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

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;
        
  knob = GTK_KNOB(widget);

  gdk_window_clear_area(widget->window, 0, 0, widget->allocation.width, widget->allocation.height);

  gdk_draw_pixmap(widget->window, widget->style->bg_gc[widget->state], knob->pixmap,
		  0, 0, 0, 0, KNOB_SIZE, KNOB_SIZE);

  dx = knob->adjustment->value - knob->adjustment->lower;
  dy = knob->adjustment->upper - knob->adjustment->lower;

  if (dy != 0) {
    dx = MIN(MAX(dx / dy, 0), 1);
    dx = (-1.5 * dx + 1.25) * M_PI;
    dy = -sin(dx) * 12 + 16;
    dx = cos(dx) * 12 + 16;

    gdk_draw_line(widget->window, widget->style->white_gc, 16, 16, dx, dy);
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
