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

#ifndef __GTK_SLIDER_H__
#define __GTK_SLIDER_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_SLIDER(obj)		GTK_CHECK_CAST(obj, gtk_slider_get_type(), GtkSlider)
#define GTK_SLIDER_CLASS(klass)	GTK_CHECK_CLASS_CAST(klass, gtk_slider_get_type(), GtkSliderClass)
#define GTK_IS_SLIDER(obj)	GTK_CHECK_TYPE(obj, gtk_slider_get_type())

typedef struct _GtkSlider        GtkSlider;
typedef struct _GtkSliderClass   GtkSliderClass;

struct _GtkSlider {
  GtkWidget widget;

  /* update policy (GTK_UPDATE_[CONTINUOUS/DELAYED/DISCONTINUOUS]) */
  guint policy : 2;

  /* State of widget (to do with user interaction) */
  guint8 state;
  gint saved_x, saved_y;

  /* ID of update timer, or 0 if none */
  guint32 timer;

  /* Pixmap for slider */
  GdkPixmap *pixmap;
  /* vertical size of slider - travel distance in pixels */
  gint size;

  /* Old values from adjustment stored so we know when something changes */
  gfloat old_value;
  gfloat old_lower;
  gfloat old_upper;

  /* The adjustment object that stores the data for this slider */
  GtkAdjustment *adjustment;
};

struct _GtkSliderClass
{
  GtkWidgetClass parent_class;
};

extern GtkWidget *gtk_slider_new(GtkAdjustment *adjustment, gint size);
extern guint gtk_slider_get_type(void);

extern GtkAdjustment *gtk_slider_get_adjustment(GtkSlider *slider);
extern void gtk_slider_set_update_policy(GtkSlider *slider, GtkUpdateType  policy);

extern void gtk_slider_set_adjustment(GtkSlider *slider, GtkAdjustment *adjustment);

#ifdef __cplusplus
}
#endif

#endif
