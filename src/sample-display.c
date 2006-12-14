
/*
 * The Real SoundTracker - GTK+ sample display widget
 *
 * Copyright (C) 1998-2000 Michael Krause
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>

#include "sample-display.h"

#include <string.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#define XPOS_TO_OFFSET(x) (s->win_start + ((guint64)(x)) * s->win_length / s->width)
#define OFFSET_RANGE(l, x) (x < 0 ? 0 : (x >= l ? l - 1 : x))

static const int default_colors[] = {
    10, 20, 30,
    230, 230, 230,
    230, 200, 0,
    230, 0, 0,
    60, 60, 140,
};

enum {
    SELECTING_NOTHING = 0,
    SELECTING_SELECTION_START,
    SELECTING_SELECTION_END,
    SELECTING_LOOP_START,
    SELECTING_LOOP_END,
    SELECTING_PAN_WINDOW,
};

enum {
    SIG_SELECTION_CHANGED,
    SIG_LOOP_CHANGED,
    SIG_WINDOW_CHANGED,
    LAST_SIGNAL
};

#define IS_INITIALIZED(s) (s->datalen != 0)

static guint sample_display_signals[LAST_SIGNAL] = { 0 };

static int  sample_display_startoffset_to_xpos (SampleDisplay *s,
						int offset);
static gint sample_display_idle_draw_function  (SampleDisplay *s);

static void
sample_display_idle_draw (SampleDisplay *s)
{
    if(!s->idle_handler) {
	s->idle_handler = gtk_idle_add((GtkFunction)sample_display_idle_draw_function,
				       (gpointer)s);
	g_assert(s->idle_handler != 0);
    }
}

void
sample_display_enable_zero_line (SampleDisplay *s,
				 gboolean enable)
{
    s->display_zero_line = enable;

    if(s->datalen) {
	gtk_widget_queue_draw(GTK_WIDGET(s));
    }
}

static void
sample_display_set_data (SampleDisplay *s,
			 void *data,
			 int type,
			 int len,
			 gboolean copy)
{
    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));

    gboolean len_changed = ( s->datalen != len );
    if(!data || !len) {
	s->datalen = 0;
    } else {
	if(copy) {
	    if(s->datacopy) {
		if(s->datacopylen < len * type / 8) {
		    g_free(s->data);
		    s->data = g_new(gint8, len * type / 8);
		    s->datacopylen = len * type / 8;
		}
	    } else {
		s->data = g_new(gint8, len * type / 8);
		s->datacopylen = len * type / 8;
	    }
	    g_assert(s->data != NULL);
	    memcpy(s->data, data, len * type / 8);
	    s->datalen = len;
	} else {
	    if(s->datacopy) {
		g_free(s->data);
	    }
	    s->data = data;
	    s->datalen = len;
	}
	s->datacopy = copy;
	s->datatype = type;
    }

    s->old_mixerpos = -1;
    s->mixerpos = -1;
	
    if( len_changed ) {
	s->win_start = 0;
	s->win_length = len;
	gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], s->win_start, s->win_start + s->win_length);

	s->sel_start = -1;
	s->old_ss = s->old_se = -1;
	s->selecting = 0;

	s->loop_start = -1;
    }

    gtk_widget_queue_draw(GTK_WIDGET(s));    
}

void
sample_display_set_data_16 (SampleDisplay *s,
			    gint16 *data,
			    int len,
			    gboolean copy)
{
    sample_display_set_data(s, data, 16, len, copy);
}

void
sample_display_set_data_8 (SampleDisplay *s,
			   gint8 *data,
			   int len,
			   gboolean copy)
{
    sample_display_set_data(s, data, 8, len, copy);
}

void 
sample_display_set_loop (SampleDisplay *s, 
			 int start,
			 int end)
{
    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));

    if(!s->edit || !IS_INITIALIZED(s))
	return;

    g_return_if_fail(start >= -1 && start < s->datalen);
    g_return_if_fail(end > 0 && end <= s->datalen);
    g_return_if_fail(end > start);

    s->loop_start = start;
    s->loop_end = end;

    gtk_widget_queue_draw(GTK_WIDGET(s));
    gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_LOOP_CHANGED], start, end);
}

void 
sample_display_set_selection (SampleDisplay *s, 
			      int start,
			      int end)
{
    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));

    if(!s->edit || !IS_INITIALIZED(s))
	return;

    g_return_if_fail(start >= -1 && start < s->datalen);
    g_return_if_fail(end >= 1 && end <= s->datalen);
    g_return_if_fail(end > start);

    s->sel_start = start;
    s->sel_end = end;

    sample_display_idle_draw(s);
    gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_SELECTION_CHANGED], start, end);
}

void
sample_display_set_mixer_position (SampleDisplay *s,
				   int offset)
{
    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));

    if(!s->edit || !IS_INITIALIZED(s))
	return;

    if(offset != s->mixerpos) {
	s->mixerpos = offset;
	sample_display_idle_draw(s);
    }
}

void
sample_display_set_window (SampleDisplay *s,
			   int start,
			   int end)
{
    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    g_return_if_fail(start >= 0 && start < s->datalen);
    g_return_if_fail(end > 0 && end <= s->datalen);
    g_return_if_fail(end > start);

    s->win_start = start;
    s->win_length = end - start;
    gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], start, end);

    gtk_widget_queue_draw(GTK_WIDGET(s));
}

static void
sample_display_init_display (SampleDisplay *s,
			     int w,
			     int h)
{
    s->width = w;
    s->height = h;
}

static void
sample_display_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
    SampleDisplay *s;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget)) {
	s = SAMPLE_DISPLAY (widget);

	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

	sample_display_init_display(s, allocation->width, allocation->height);
    }
}

static void
sample_display_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    SampleDisplay *s;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (IS_SAMPLE_DISPLAY (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    s = SAMPLE_DISPLAY(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
	| GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	| GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach (widget->style, widget->window);

    s->bg_gc = gdk_gc_new(widget->window);
    s->fg_gc = gdk_gc_new(widget->window);
    s->zeroline_gc = gdk_gc_new(widget->window);
    gdk_gc_set_foreground(s->bg_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_BG]);
    gdk_gc_set_foreground(s->fg_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_FG]);
    gdk_gc_set_foreground(s->zeroline_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_ZERO]);
    if(s->edit) {
	s->loop_gc = gdk_gc_new(widget->window);
	s->mixerpos_gc = gdk_gc_new(widget->window);
	gdk_gc_set_foreground(s->loop_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_LOOP]);
	gdk_gc_set_foreground(s->mixerpos_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_MIXERPOS]);
    }

    sample_display_init_display(s, attributes.width, attributes.height);

    gdk_window_set_user_data (widget->window, widget);
}

static void
sample_display_size_request (GtkWidget *widget,
			     GtkRequisition *requisition)
{
    requisition->width = 10;
    requisition->height = 32;
}

static void
sample_display_draw_data (GdkDrawable *win,
			  const SampleDisplay *s,
			  int color,
			  int x,
			  int width)
{
    gint16 c, d;
    GdkGC *gc;
    const int sh = s->height;

    if(width == 0)
	return;

    g_return_if_fail(x >= 0);
    g_return_if_fail(x + width <= s->width);

    gdk_draw_rectangle(win, color ? s->fg_gc : s->bg_gc,
		       TRUE, x, 0, width, s->height);

    if(s->display_zero_line) {
	gdk_draw_line(win, s->zeroline_gc, x, s->height / 2, x + width - 1, s->height / 2);
    }

    gc = color ? s->bg_gc : s->fg_gc;

    if(s->datatype == 16) {
	c = ((gint16*)s->data)[OFFSET_RANGE(s->datalen, XPOS_TO_OFFSET(x - 1))];
	
	while(width >= 0) {
	    d = ((gint16*)s->data)[OFFSET_RANGE(s->datalen, XPOS_TO_OFFSET(x))];
	    gdk_draw_line(win, gc,
			  x - 1, ((32768 - c) * sh) >> 16,
			  x,     ((32768 - d) * sh) >> 16);
	    c = d;
	    x++;
	    width--;
	}
    } else {
	c = ((gint8*)s->data)[OFFSET_RANGE(s->datalen, XPOS_TO_OFFSET(x - 1))];
	
	while(width >= 0) {
	    d = ((gint8*)s->data)[OFFSET_RANGE(s->datalen, XPOS_TO_OFFSET(x))];
	    gdk_draw_line(win, gc,
			  x - 1, ((128 - c) * sh) >> 8,
			  x,     ((128 - d) * sh) >> 8);
	    c = d;
	    x++;
	    width--;
	}
    }
}

static int
sample_display_startoffset_to_xpos (SampleDisplay *s,
				    int offset)
{
    gint64 d = offset - s->win_start;

    if(d < 0)
	return 0;
    if(d >= s->win_length)
	return s->width;

    return d * s->width / s->win_length;
}

static int
sample_display_endoffset_to_xpos (SampleDisplay *s,
				  int offset)
{
    if(s->win_length < s->width) {
	return sample_display_startoffset_to_xpos(s, offset);
    } else {
	gint64 d = offset - s->win_start;
	int l = (1 - s->win_length) / s->width;

	/* you get these tests by setting the complete formula below equal to 0 or s->width, respectively,
	   and then resolving towards d. */
	if(d < l)
	    return 0;
	if(d > s->win_length + l)
	    return s->width;

	return (d * s->width + s->win_length - 1) / s->win_length;
    }
}

static void
sample_display_do_marker_line (GdkDrawable *win,
			       SampleDisplay *s,
			       int endoffset,
			       int offset,
			       int x_min,
			       int x_max,
			       GdkGC *gc)
{
    int x;

    if(offset >= s->win_start && offset <= s->win_start + s->win_length) {
	if(!endoffset)
	    x = sample_display_startoffset_to_xpos(s, offset);
	else
	    x = sample_display_endoffset_to_xpos(s, offset);
	if(x+3 >= x_min && x-3 < x_max) {
	    gdk_draw_line(win, gc,
			  x, 0, 
			  x, s->height);
	    gdk_draw_rectangle(win, gc, TRUE,
			       x - 3, 0, 7, 10);
	    gdk_draw_rectangle(win, gc, TRUE,
			       x - 3, s->height - 10, 7, 10);
	}
    }
}

static void
sample_display_draw_main (GtkWidget *widget,
			  GdkRectangle *area)
{
    SampleDisplay *s = SAMPLE_DISPLAY(widget);
    int x, x2;

    g_return_if_fail(area->x >= 0);

    if(area->width == 0)
	return;

    if(area->x + area->width > s->width)
	return;

    if(!IS_INITIALIZED(s)) {
	gdk_draw_rectangle(widget->window,
			   s->bg_gc,
			   TRUE, area->x, area->y, area->width, area->height);
	gdk_draw_line(widget->window,
		      s->fg_gc,
		      area->x, s->height / 2,
		      area->x + area->width - 1, s->height / 2);
    } else {
	const int x_min = area->x;
	const int x_max = area->x + area->width;

	if(s->sel_start != -1) {
	    /* draw the part to the left of the selection */
	    x = sample_display_startoffset_to_xpos(s, s->sel_start);
	    x = MIN(x_max, MAX(x_min, x));
	    sample_display_draw_data(widget->window, s, 0, x_min, x - x_min);

	    /* draw the selection */
	    x2 = sample_display_endoffset_to_xpos(s, s->sel_end);
	    x2 = MIN(x_max, MAX(x_min, x2));
	    sample_display_draw_data(widget->window, s, 1, x, x2 - x);
	} else {
	    /* we don't have a selection */
	    x2 = x_min;
	}

	/* draw the part to the right of the selection */
	sample_display_draw_data(widget->window, s, 0, x2, x_max - x2);

	if(s->loop_start != -1) {
	    sample_display_do_marker_line(widget->window, s, 0, s->loop_start, x_min, x_max, s->loop_gc);
	    sample_display_do_marker_line(widget->window, s, 1, s->loop_end, x_min, x_max, s->loop_gc);
	}

	if(s->mixerpos != -1) {
	    sample_display_do_marker_line(widget->window, s, 0, s->mixerpos, x_min, x_max, s->mixerpos_gc);
	    s->old_mixerpos = s->mixerpos;
	}
    }
}

static void
sample_display_draw_update (GtkWidget *widget,
			    GdkRectangle *area)
{
    SampleDisplay *s = SAMPLE_DISPLAY(widget);
    GdkRectangle area2 = { 0, 0, 0, s->height };
    int x, i;
    const int x_min = area->x;
    const int x_max = area->x + area->width;
    gboolean special_draw = FALSE;

    if(s->mixerpos != s->old_mixerpos) {
	/* Redraw area of old position, redraw area of new position. */
	for(i = 0; i < 2; i++) {
	    if(s->old_mixerpos >= s->win_start && s->old_mixerpos < s->win_start + s->win_length) {
		x = sample_display_startoffset_to_xpos(s, s->old_mixerpos);
		area2.x = MIN(x_max - 1, MAX(x_min, x - 3));
		area2.width = 7;
		if(area2.x + area2.width > x_max) {
		    area2.width = x_max - area2.x;
		}
		sample_display_draw_main(widget, &area2);
	    }
	    s->old_mixerpos = s->mixerpos;
	}
	special_draw = TRUE;
    }

    if(s->sel_start != s->old_ss || s->sel_end != s->old_se) {
	if(s->sel_start == -1 || s->old_ss == -1) {
	    sample_display_draw_main(widget, area);
	} else {
	    if(s->sel_start < s->old_ss) {
		/* repaint left additional side */
		x = sample_display_startoffset_to_xpos(s, s->sel_start);
		area2.x = MIN(x_max, MAX(x_min, x));
		x = sample_display_startoffset_to_xpos(s, s->old_ss);
		area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
	    } else {
		/* repaint left removed side */
		x = sample_display_startoffset_to_xpos(s, s->old_ss);
		area2.x = MIN(x_max, MAX(x_min, x));
		x = sample_display_startoffset_to_xpos(s, s->sel_start);
		area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
	    }
	    sample_display_draw_main(widget, &area2);

	    if(s->sel_end < s->old_se) {
		/* repaint right removed side */
		x = sample_display_endoffset_to_xpos(s, s->sel_end);
		area2.x = MIN(x_max, MAX(x_min, x));
		x = sample_display_endoffset_to_xpos(s, s->old_se);
		area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
	    } else {
		/* repaint right additional side */
		x = sample_display_endoffset_to_xpos(s, s->old_se);
		area2.x = MIN(x_max, MAX(x_min, x));
		x = sample_display_endoffset_to_xpos(s, s->sel_end);
		area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
	    }
	    sample_display_draw_main(widget, &area2);
	}

	s->old_ss = s->sel_start;
	s->old_se = s->sel_end;
	special_draw = TRUE;
    }

    if(!special_draw) {
	sample_display_draw_main(widget, area);
    }
}

static gint
sample_display_expose (GtkWidget *widget,
		       GdkEventExpose *event)
{
    sample_display_draw_main(widget, &event->area);
    return FALSE;
}

static gint
sample_display_idle_draw_function (SampleDisplay *s)
{
    GdkRectangle area = { 0, 0, s->width, s->height };

    if(GTK_WIDGET_MAPPED(GTK_WIDGET(s))) {
	sample_display_draw_update(GTK_WIDGET(s), &area);
    }

    gtk_idle_remove(s->idle_handler);
    s->idle_handler = 0;
    return TRUE;
}

static void
sample_display_handle_motion (SampleDisplay *s,
			      int x,
			      int y,
			      int just_clicked)
{
    int ol, or;
    int ss = s->sel_start, se = s->sel_end;
    int ls = s->loop_start, le = s->loop_end;

    if(!s->selecting)
	return;

    if(x < 0)
	x = 0;
    else if(x >= s->width)
	x = s->width - 1;

    ol = XPOS_TO_OFFSET(x);
    if(s->win_length < s->width) {
	or = XPOS_TO_OFFSET(x) + 1;
    } else {
	or = XPOS_TO_OFFSET(x + 1);
    }

    g_return_if_fail(ol >= 0 && ol < s->datalen);
    g_return_if_fail(or > 0 && or <= s->datalen);
    g_return_if_fail(ol < or);

    switch(s->selecting) {
    case SELECTING_SELECTION_START:
	if(just_clicked) {
	    if(ss != -1 && ol < se) {
		ss = ol;
	    } else {
		ss = ol;
		se = ol + 1;
	    }
	} else {
	    if(ol < se) {
		ss = ol;
	    } else {
		ss = se - 1;
		se = or;
		s->selecting = SELECTING_SELECTION_END;
	    }
	}
	break;
    case SELECTING_SELECTION_END:
	if(just_clicked) {
	    if(ss != -1 && or > ss) {
		se = or;
	    } else {
		ss = or - 1;
		se = or;
	    }
	} else {
	    if(or > ss) {
		se = or;
	    } else {
		se = ss + 1;
		ss = ol;
		s->selecting = SELECTING_SELECTION_START;
	    }
	}
	break;
    case SELECTING_LOOP_START:
	if(ol < s->loop_end)
	    ls = ol;
	else
	    ls = le - 1;
	break;
    case SELECTING_LOOP_END:
	if(or > s->loop_start)
	    le = or;
	else
	    le = ls + 1;
	break;
    default:
	g_assert_not_reached();
	break;
    }

    if(s->sel_start != ss || s->sel_end != se) {
	s->sel_start = ss;
	s->sel_end = se;
	sample_display_idle_draw(s);
	gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_SELECTION_CHANGED], ss, se);
    }

    if(s->loop_start != ls || s->loop_end != le) {
	s->loop_start = ls;
	s->loop_end = le;
	sample_display_idle_draw(s);
	gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_LOOP_CHANGED], ls, le);
    }
}

// Handle middle mousebutton display window panning
static void
sample_display_handle_motion_2 (SampleDisplay *s,
				int x,
				int y)
{
    int new_win_start = s->selecting_wins0 + (s->selecting_x0 - x) * s->win_length / s->width;

    new_win_start = CLAMP(new_win_start, 0, s->datalen - s->win_length);

    if(new_win_start != s->win_start) {
	sample_display_set_window (s,
				   new_win_start,
				   new_win_start + s->win_length);
    }
}

static gint
sample_display_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
    SampleDisplay *s;
    int x, y;
    GdkModifierType state;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    s = SAMPLE_DISPLAY(widget);

    if(!s->edit)
	return FALSE;

    if(!IS_INITIALIZED(s))
	return TRUE;

    if(s->selecting && event->button != s->button) {
	s->selecting = 0;
    } else {
	s->button = event->button;
	gdk_window_get_pointer (event->window, &x, &y, &state);
	if(!(state & GDK_SHIFT_MASK)) {
	    if(s->button == 1) {
		s->selecting = SELECTING_SELECTION_START;
	    } else if(s->button == 2) {
		s->selecting = SELECTING_PAN_WINDOW;
		gdk_window_get_pointer (event->window, &s->selecting_x0, NULL, NULL);
		s->selecting_wins0 = s->win_start;
	    } else if(s->button == 3) {
		s->selecting = SELECTING_SELECTION_END;
	    }
	} else {
	    if(s->loop_start != -1) {
		if(s->button == 1) {
		    s->selecting = SELECTING_LOOP_START;
		} else if(s->button == 3) {
		    s->selecting = SELECTING_LOOP_END;
		}
	    }
	}
	if(!s->selecting)
	    return TRUE;
	if(s->selecting != SELECTING_PAN_WINDOW)
	    sample_display_handle_motion(s, x, y, 1);
    }
	    
    return TRUE;
}

static gint
sample_display_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
    SampleDisplay *s;

    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    
    s = SAMPLE_DISPLAY(widget);

    if(!s->edit)
	return FALSE;

    if(s->selecting && event->button == s->button) {
	s->selecting = 0;
    }
    
    return TRUE;
}

static gint
sample_display_motion_notify (GtkWidget *widget,
			      GdkEventMotion *event)
{
    SampleDisplay *s;
    int x, y;
    GdkModifierType state;

    s = SAMPLE_DISPLAY(widget);

    if(!s->edit)
	return FALSE;

    if(!IS_INITIALIZED(s) || !s->selecting)
	return TRUE;

    if (event->is_hint) {
	gdk_window_get_pointer (event->window, &x, &y, &state);
    } else {
	x = event->x;
	y = event->y;
	state = event->state;
    }

    if(((state & GDK_BUTTON1_MASK) && s->button == 1) || ((state & GDK_BUTTON3_MASK) && s->button == 3)) {
	sample_display_handle_motion(SAMPLE_DISPLAY(widget), x, y, 0);
    } else if((state & GDK_BUTTON2_MASK) && s->button == 2) {
	sample_display_handle_motion_2(SAMPLE_DISPLAY(widget), x, y);
    } else {
	s->selecting = 0;
    }
    
    return TRUE;
}

static void
sample_display_class_init (SampleDisplayClass *class)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;
    int n;
    const int *p;
    GdkColor *c;

    object_class = (GtkObjectClass*) class;
    widget_class = (GtkWidgetClass*) class;

    widget_class->realize = sample_display_realize;
    widget_class->size_allocate = sample_display_size_allocate;
    widget_class->expose_event = sample_display_expose;
    //widget_class->draw = sample_display_draw;
    widget_class->size_request = sample_display_size_request;
    widget_class->button_press_event = sample_display_button_press;
    widget_class->button_release_event = sample_display_button_release;
    widget_class->motion_notify_event = sample_display_motion_notify;

    sample_display_signals[SIG_SELECTION_CHANGED] = gtk_signal_new ("selection_changed",
							     GTK_RUN_FIRST,
							     GTK_CLASS_TYPE(object_class),
							     GTK_SIGNAL_OFFSET(SampleDisplayClass, selection_changed),
							     gtk_marshal_NONE__INT_INT,
							     GTK_TYPE_NONE, 2,
							     GTK_TYPE_INT,
							     GTK_TYPE_INT);
    sample_display_signals[SIG_LOOP_CHANGED] = gtk_signal_new ("loop_changed",
							GTK_RUN_FIRST,
							GTK_CLASS_TYPE(object_class),
							GTK_SIGNAL_OFFSET(SampleDisplayClass, loop_changed),
							gtk_marshal_NONE__INT_INT,
							GTK_TYPE_NONE, 2,
							GTK_TYPE_INT,
							GTK_TYPE_INT);
    sample_display_signals[SIG_WINDOW_CHANGED] = gtk_signal_new ("window_changed",
								 GTK_RUN_FIRST,
								 GTK_CLASS_TYPE(object_class),
								 GTK_SIGNAL_OFFSET(SampleDisplayClass, window_changed),
								 gtk_marshal_NONE__INT_INT,
								 GTK_TYPE_NONE, 2,
								 GTK_TYPE_INT,
								 GTK_TYPE_INT);

    //gtk_object_class_add_signals(object_class, sample_display_signals, LAST_SIGNAL);
    
    class->selection_changed = NULL;
    class->loop_changed = NULL;
    class->window_changed = NULL;

    for(n = 0, p = default_colors, c = class->colors; n < SAMPLE_DISPLAYCOL_LAST; n++, c++) {
	c->red = *p++ * 65535 / 255;
	c->green = *p++ * 65535 / 255;
	c->blue = *p++ * 65535 / 255;
        c->pixel = (gulong)((c->red & 0xff00)*256 + (c->green & 0xff00) + (c->blue & 0xff00)/256);
        gdk_color_alloc(gdk_colormap_get_system(), c);
    }
}

static void
sample_display_init (SampleDisplay *s)
{
    s->idle_handler = 0;
}

guint
sample_display_get_type (void)
{
    static guint sample_display_type = 0;

    if (!sample_display_type) {
	GtkTypeInfo sample_display_info =
	{
	    "SampleDisplay",
	    sizeof(SampleDisplay),
	    sizeof(SampleDisplayClass),
	    (GtkClassInitFunc) sample_display_class_init,
	    (GtkObjectInitFunc) sample_display_init,
	    NULL,
	    NULL,
	};

	sample_display_type = gtk_type_unique (gtk_widget_get_type (), &sample_display_info);
    }

    return sample_display_type;
}

GtkWidget*
sample_display_new (gboolean edit)
{
    SampleDisplay *s = SAMPLE_DISPLAY(gtk_type_new(sample_display_get_type()));

    s->edit = edit;
    s->datacopy = 0;
    s->display_zero_line = 0;
    return GTK_WIDGET(s);
}
