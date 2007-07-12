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

#include "global.h"
#include "generator.h"
#include "gui.h"
#include "comp.h"
#include "sheet.h"
#include "control.h"
#include "gencomp.h"
#include "iscomp.h"
#include "shcomp.h"
#include "msgbox.h"

#define GEN_AREA_LENGTH		2048

enum SheetModes {
  SHEET_MODE_NORMAL = 0,	    /**< nothing happening */
  SHEET_MODE_SCROLLING,		    /**< middle button was pressed and we follow mouse movement with the scrollwin */
  SHEET_MODE_PRESSING,		    /**< left button was pressed on a connector or component. Lets see if the user
				     *   moves the mouse. */
  SHEET_MODE_DRAGGING_COMP,	    /**< we were on a component and the user moved the mouse. */
  SHEET_MODE_DRAGGING_LINK,	    /**< we were on a connector and the user moved the mouse. */
  SHEET_MODE_DRAGGING_SEL_COMPS,    /**< we are moving the selected components from sheet->selected_comps */
  SHEET_MODE_PRESSING_NOWHERE,	    /**< The user pressed left mouse button on an empty area. If user now releases the button
				     *	 we will unselect the selection, if the user moves the mouse we will draw a selection box */
  SHEET_MODE_DRAGGING_SEL_BOX,	    /**< the user is dragging the selection box. */
  SHEET_MAX_MODE
};

GeneratorClass *is_evtin_klass; 
GeneratorClass *is_evtout_klass; 
GeneratorClass *is_sigin_klass; 
GeneratorClass *is_sigout_klass; 

PRIVATE int next_sheet_number = 1;  /**< This is a counter for sheet numbers.
				     *   \note It should be adjusted when we load a new sheet. But normally sheets should be
				     *         named correctly after instantiantion so this will be done later.
				     */


PRIVATE gboolean load_hidden = FALSE;

PUBLIC void sheet_set_load_hidden( gboolean v ) {
    load_hidden = v;
}


/**
 * /brief Colors for normal Component drawing
 */

PRIVATE GdkColor comp_colors[COMP_NUM_COLORS] = {
  { 0, 0, 0, 0 },
  { 0, 65535, 65535, 65535 },
  { 0, 65535, 0, 0 },
  { 0, 0, 65535, 0 },
  { 0, 0, 0, 65535 },
  { 0, 65535, 65535, 0 },
  { 0, 65535, 0, 65535 },
  { 0, 0, 65535, 65535 }
};

/**
 * /brief Colors for selected  Component drawing
 */

PRIVATE GdkColor comp_sel_colors[COMP_NUM_COLORS] = {
  { 0, 0, 0, 0 },
  { 0, 32767, 32767, 32767 },
  { 0, 65535, 0, 0 },
  { 0, 0, 65535, 0 },
  { 0, 0, 0, 65535 },
  { 0, 65535, 65535, 0 },
  { 0, 65535, 0, 65535 },
  { 0, 0, 65535, 65535 }
};


/**
 * \brief process an expose event
 *
 * This traverses The list of components.
 *  calls comp_paint() for all components if necessary.
 *  also selects the colormap based on the list of selected components
 *
 * \param widget the sheets drawing area.
 * \param ee The Expose Event containing the box which should be redrawn.
 */

PRIVATE gboolean do_sheet_repaint(GtkWidget *widget, GdkEventExpose *ee) {
  GdkDrawable *drawable = widget->window;
  GtkStyle *style = gtk_widget_get_style(widget);
  Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT(widget) );
  GList *l;
  int i;
  GdkRectangle area = ee->area;

  area.width++;
  area.height++;

  for (i = 0; i < 5; i++) {
    gdk_gc_set_clip_rectangle(style->fg_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->bg_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->light_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->dark_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->mid_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->text_gc[i], &area);
    gdk_gc_set_clip_rectangle(style->base_gc[i], &area);
  }
  gdk_gc_set_clip_rectangle(style->black_gc, &area);
  gdk_gc_set_clip_rectangle(style->white_gc, &area);

  gdk_draw_rectangle(drawable, style->black_gc, TRUE,
		     area.x, area.y, area.width, area.height);

  for (l = g_list_last(sheet->components); l != NULL; l = g_list_previous(l)) {
    Component *c = l->data;
    GdkRectangle r_gen, r;

    comp_paint_connections(c, &area, drawable, style, comp_colors);

    r_gen.x = c->x;
    r_gen.y = c->y;
    r_gen.width = c->width;
    r_gen.height = c->height;

    if (gdk_rectangle_intersect(&r_gen, &area, &r)) {
	if( g_list_find( sheet->selected_comps, c ) != NULL )
	    comp_paint(c, &area, drawable, style, comp_sel_colors);
	else
	    comp_paint(c, &area, drawable, style, comp_colors);
    }
  }

  if( sheet->sel_valid )
      gdk_draw_rectangle(drawable, style->white_gc, FALSE,
	      sheet->sel_rect.x, sheet->sel_rect.y, 
	      sheet->sel_rect.width, sheet->sel_rect.height);

  return TRUE;
}

/**
 * \brief process Motion event for sheet scrolling on middle mouse button
 */

PRIVATE void scroll_follow_mouse(Sheet *sheet, GdkEventMotion *me) {
  GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sheet->scrollwin));
  GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sheet->scrollwin));

  gtk_adjustment_set_value(ha, sheet->saved_x - me->x_root);
  gtk_adjustment_set_value(va, sheet->saved_y - me->y_root);
  sheet->saved_x = ha->value + me->x_root;
  sheet->saved_y = va->value + me->y_root;
}

/**
 * \brief return first component containing \a x , \a y
 *
 * Traverse components list and call comp_contains_point()
 */

PRIVATE Component *find_component_at(Sheet *sheet, gint x, gint y) {
  GList *lst = sheet->components;

  while (lst != NULL) {
    Component *c = lst->data;

    if (comp_contains_point(c, x, y))
      return c;

    lst = g_list_next(lst);
  }

  return NULL;
}

/**
 * \brief return all Component s containing \a x , \a y
 */

PRIVATE GList *find_components_at(Sheet *sheet, gint x, gint y) {
  GList *lst = sheet->components;
  GList *accum = NULL;

  while (lst != NULL) {
    Component *c = lst->data;

    if (comp_contains_point(c, x, y))
      accum = g_list_append(accum, c);

    lst = g_list_next(lst);
  }

  return accum;
}

/**
 * \brief Process GdkButtonEvent
 */

PRIVATE void process_click(GtkWidget *w, GdkEventButton *be) {
  Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT(w) );
  Component *c = find_component_at(sheet, be->x, be->y);

  if (c != NULL) {
    int found_conn;
    sheet->saved_ref.kind = COMP_NO_CONNECTOR;

    found_conn = comp_find_connector(c, be->x, be->y, &sheet->saved_ref);
    

    if (!found_conn) {
	GList *selcomp;
	sheet->components = g_list_remove(sheet->components, c);
	sheet->components = g_list_prepend(sheet->components, c);

	for( selcomp = sheet->selected_comps; selcomp != NULL; selcomp = g_list_next( selcomp ) ) {
	    Component *selcompX = selcomp->data;
	    selcompX->saved_x = selcompX->x - be->x_root;
	    selcompX->saved_y = selcompX->y - be->y_root;
	}

	sheet->saved_ref.c = c;
    }
    else
    {
	sheet->highlight_ref = sheet->saved_ref;
    }
    gtk_widget_queue_draw_area(sheet->drawingwidget, c->x, c->y, c->width, c->height);

    sheet->saved_x = c->x - be->x_root;
    sheet->saved_y = c->y - be->y_root;
    sheet->sheetmode = SHEET_MODE_PRESSING;
  } else {
      sheet->saved_x = be->x;
      sheet->saved_y = be->y;

      sheet->sel_rect.x = be->x;
      sheet->sel_rect.y = be->y;
      sheet->sel_rect.width = sheet->sel_rect.height = 0;

      sheet->sheetmode = SHEET_MODE_PRESSING_NOWHERE;
  }
}

PRIVATE void disconnect_handler(GtkWidget *menuitem, ConnectorReference *ref) {

  Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT( menuitem ) );
  comp_unlink(&sheet->saved_ref, ref);
  gtk_widget_queue_draw(sheet->drawingwidget);
}

PRIVATE void disconnect_all_handler(GtkWidget *menuitem) {
    
  Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT( menuitem ) );
  Connector *con = comp_get_connector(&sheet->saved_ref);
  GList *lst = con->refs;

  while (lst != NULL) {
    ConnectorReference *ref = lst->data;
    lst = g_list_next(lst);

    comp_unlink(&con->ref, ref);
  }

  gtk_widget_queue_draw(sheet->drawingwidget);
}


#define DISCONNECT_STRING "Disconnect "

PRIVATE void append_disconnect_menu(Sheet *sheet, Connector *con, GtkWidget *menu) {
  GtkWidget *submenu;
  GtkWidget *item;
  GList *lst;
  char *name;
  char *label;

  sheet->saved_ref = con->ref;

  submenu = gtk_menu_new();

  for (lst = con->refs; lst != NULL; lst = g_list_next(lst)) {
    ConnectorReference *ref = lst->data;

    name = comp_get_connector_name(ref);
    item = gtk_menu_item_new_with_label(name);
    gtk_object_set_user_data( GTK_OBJECT( item ), (gpointer) sheet );
    free(name);

    gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(disconnect_handler), ref);
    gtk_menu_append(GTK_MENU(submenu), item);
    gtk_widget_show(item);
  }

  item = gtk_menu_item_new_with_label("ALL");
  gtk_object_set_user_data( GTK_OBJECT( item ), (gpointer) sheet );
  gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(disconnect_all_handler), NULL);
  gtk_menu_append(GTK_MENU(submenu), item);
  gtk_widget_show(item);

  name = comp_get_connector_name(&con->ref);
  label = malloc(strlen(DISCONNECT_STRING) + strlen(name) + 1);
  if (label == NULL)
    item = gtk_menu_item_new_with_label(DISCONNECT_STRING);
  else {
    strcpy(label, DISCONNECT_STRING);
    strcat(label, name);
    item = gtk_menu_item_new_with_label(label);
    free(label);
  }
  free(name);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
}

PRIVATE void do_popup_menu(Sheet *sheet, GdkEventButton *be) {
  static GtkWidget *old_popup_menu = NULL;
  GtkWidget *menu;
  GList *comps;

  sheet->saved_x = be->x;
  sheet->saved_y = be->y;

  if (old_popup_menu != NULL) {
    gtk_widget_unref(old_popup_menu);
    old_popup_menu = NULL;
  }


  comps = find_components_at(sheet, be->x, be->y);
  if (comps != NULL) {
    ConnectorReference ref;

    if (comp_find_connector(comps->data, be->x, be->y, &ref)) {
	menu = gtk_menu_new();
	append_disconnect_menu(sheet, comp_get_connector(&ref), menu);
    }
    else
	while (comps != NULL) {
	    GList *tmp = g_list_next(comps);

	    //gtk_widget_unref(menu);
	    menu=comp_get_popup(comps->data);

	    g_list_free_1(comps);
	    comps = tmp;
	}

  } else {

      //gtk_widget_unref(menu);
      menu = comp_get_newmenu(sheet);
  }



  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 be->button, be->time);

  g_object_ref( G_OBJECT( menu ) );
  //g_object_sink( G_OBJECT( menu ) );
  old_popup_menu = menu;
}

/*! handle mouse motion events to display port names */
PRIVATE gint motion_notify_event(GtkWidget *widget, GdkEventMotion *event,
		gpointer func_data)
{
  int x, y;
  GdkModifierType state;
  Component *c;
  Sheet *sheet = (Sheet*)func_data;

  if(event->is_hint)
    gdk_window_get_pointer(event->window, &x, &y, &state);
  else {
    x = event->x;
    y = event->y;
    state = event->state;
  }
  
  if((c = find_component_at(sheet, x, y)) != NULL)
  {
     char *name;
     ConnectorReference ref;
     
     if(!comp_find_connector(c, x, y, &ref)) {
	     gui_statusbar_push( "" );
	     return FALSE;
     }
 
     name = comp_get_connector_name(&ref);
     g_assert(name != NULL);

     //g_print("tooltip %s\n", name);
     gui_statusbar_push( name );
     free(name);
   } else {
       gui_statusbar_push( "" );
   }

   return FALSE;
}

PRIVATE gboolean do_sheet_event(GtkWidget *w, GdkEvent *e) {

    Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT(w) );

    switch (e->type)
    {
	case GDK_BUTTON_PRESS:
	    {
		GdkEventButton *be = (GdkEventButton *) e;

		switch (be->button) {
		    case 1:
			process_click(w, be);
			return TRUE;

		    case 2:
			if (sheet->sheetmode == SHEET_MODE_NORMAL) {
			    GtkAdjustment *ha =
				gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sheet->scrollwin));
			    GtkAdjustment *va =
				gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sheet->scrollwin));
			    sheet->sheetmode = SHEET_MODE_SCROLLING;
			    sheet->saved_x = be->x_root + ha->value;
			    sheet->saved_y = be->y_root + va->value;
			    return TRUE;
			}
			break;

		    case 3:
			do_popup_menu(sheet, be);
			return TRUE;

		    default:
			break;
		}
		break;
	    }

	case GDK_BUTTON_RELEASE:
	    {
		GdkEventButton *be = (GdkEventButton *) e;

		switch (sheet->sheetmode)
		{
		    case SHEET_MODE_SCROLLING:
			break;

		    case SHEET_MODE_DRAGGING_COMP:
		    case SHEET_MODE_DRAGGING_SEL_COMPS:
			gtk_widget_queue_draw(sheet->drawingwidget);
			break;

		    case SHEET_MODE_DRAGGING_LINK:
			{
			    Component *c = find_component_at(sheet, be->x, be->y);

			    if (c != NULL) {
				ConnectorReference ref;

				if (comp_find_connector(c, be->x, be->y, &ref)) {
				    comp_link(&sheet->saved_ref, &ref);
				}
			    }
			    sheet->highlight_ref.kind = COMP_NO_CONNECTOR;
			    gtk_widget_queue_draw(sheet->drawingwidget);

			    break;
			}

		    case SHEET_MODE_PRESSING:
			{
			    Component *c = find_component_at( sheet, be->x, be->y );
			    if( c != NULL ) {
				if( g_list_find( sheet->selected_comps, c ) != NULL )
				    sheet->selected_comps = g_list_remove( sheet->selected_comps, c );
				else
				    sheet->selected_comps = g_list_append( sheet->selected_comps, c );

				sheet_queue_redraw_component( sheet, c );
			    }

			    break;
			}

		    case SHEET_MODE_PRESSING_NOWHERE:
			{
			    if( sheet->selected_comps != NULL ) {

				g_list_free( sheet->selected_comps );
				sheet->selected_comps = NULL;
				gtk_widget_queue_draw( sheet->scrollwin );
			    }

			    break;
			}

		    case SHEET_MODE_DRAGGING_SEL_BOX:
			{
			    GList *l;

			    for (l = g_list_last(sheet->components); l != NULL; l = g_list_previous(l)) {
				Component *c = l->data;
				GdkRectangle r_gen, r;

				r_gen.x = c->x;
				r_gen.y = c->y;
				r_gen.width = c->width;
				r_gen.height = c->height;

				if (gdk_rectangle_intersect(&r_gen, &sheet->sel_rect, &r)) {
				    if( g_list_find( sheet->selected_comps, c ) == NULL )
					sheet->selected_comps = g_list_append( sheet->selected_comps, c );
				}
			    }
			    gtk_widget_queue_draw( sheet->drawingwidget );
			    sheet->sel_valid = FALSE;
			}

		    default:
			break;
		}

		sheet->sheetmode = SHEET_MODE_NORMAL;
		return TRUE;
	    }

	case GDK_MOTION_NOTIFY:
	    {
		GdkEventMotion *me = (GdkEventMotion *) e;

		switch (sheet->sheetmode) {
		    case SHEET_MODE_SCROLLING:
			scroll_follow_mouse(sheet, me);
			return TRUE;

		    case SHEET_MODE_PRESSING:
			if (sheet->saved_ref.kind != COMP_NO_CONNECTOR)
			    sheet->sheetmode = SHEET_MODE_DRAGGING_LINK;
			else
			    if( g_list_find( sheet->selected_comps, sheet->saved_ref.c ) == NULL )
				sheet->sheetmode = SHEET_MODE_DRAGGING_COMP;
			    else
				sheet->sheetmode = SHEET_MODE_DRAGGING_SEL_COMPS;


			break;

		    case SHEET_MODE_DRAGGING_LINK:
			motion_notify_event( w, me, sheet );
			break;

		    case SHEET_MODE_DRAGGING_SEL_COMPS:
			{
			    GList *selcomp;

			    for( selcomp = sheet->selected_comps; selcomp != NULL; selcomp = g_list_next( selcomp ) ) {
				Component *selcompX = selcomp->data;
				int x = selcompX->x;
				int y = selcompX->y;

				//		  gtk_widget_queue_draw_area(sheet->drawingwidget,
				//			  selcompX->x, selcompX->y,
				//			  selcompX->width, selcompX->height);

				selcompX->x = MIN(MAX(selcompX->saved_x + me->x_root, 0),
					GEN_AREA_LENGTH - selcompX->width);

				selcompX->y = MIN(MAX(selcompX->saved_y + me->y_root, 0),
					GEN_AREA_LENGTH - selcompX->height);

				gtk_widget_queue_draw_area(sheet->drawingwidget,
					x, y,
					sheet->saved_ref.c->width, sheet->saved_ref.c->height);
				gtk_widget_queue_draw_area(sheet->drawingwidget,
					selcompX->x, selcompX->y,
					selcompX->width, selcompX->height);
			    }
			    break;
			}

		    case SHEET_MODE_DRAGGING_COMP:
			{
			    int x = sheet->saved_ref.c->x;
			    int y = sheet->saved_ref.c->y;

			    //	      gtk_widget_queue_draw_area(sheet->drawingwidget,
			    //		      sheet->saved_ref.c->x, sheet->saved_ref.c->y,
			    //		      sheet->saved_ref.c->width, sheet->saved_ref.c->height);

			    sheet->saved_ref.c->x = MIN(MAX(sheet->saved_x + me->x_root, 0),
				    GEN_AREA_LENGTH - sheet->saved_ref.c->width);

			    sheet->saved_ref.c->y = MIN(MAX(sheet->saved_y + me->y_root, 0),
				    GEN_AREA_LENGTH - sheet->saved_ref.c->height);

			    gtk_widget_queue_draw_area(sheet->drawingwidget,
				    x, y,
				    sheet->saved_ref.c->width, sheet->saved_ref.c->height);

			    gtk_widget_queue_draw_area(sheet->drawingwidget,
				    sheet->saved_ref.c->x, sheet->saved_ref.c->y,
				    sheet->saved_ref.c->width, sheet->saved_ref.c->height);

			    break;
			}

		    case SHEET_MODE_PRESSING_NOWHERE:
			{
			    sheet->sheetmode = SHEET_MODE_DRAGGING_SEL_BOX;

			    // fallthrough.
			}
		    case SHEET_MODE_DRAGGING_SEL_BOX:
			{
			    GdkRectangle newsel_rect, union_rect;

			    newsel_rect.x = MIN( sheet->saved_x, me->x );
			    newsel_rect.y = MIN( sheet->saved_y, me->y );
			    newsel_rect.width = MAX( sheet->saved_x, me->x ) - newsel_rect.x; 
			    newsel_rect.height = MAX( sheet->saved_y, me->y ) - newsel_rect.y; 

			    gdk_rectangle_union( &newsel_rect, &sheet->sel_rect, &union_rect );

			    sheet->sel_rect = newsel_rect;
			    sheet->sel_valid = TRUE;

			    gtk_widget_queue_draw_area( sheet->drawingwidget,
				    union_rect.x, union_rect.y, union_rect.width+1, union_rect.height+1 );

			    break;
			}

		    default:
			break;
		}
		break;
	    }

	default:
	    break;
    }

    return FALSE;
}


PRIVATE void sheet_set_dirty( Sheet *s, gboolean d ) {
    s->dirty = d;
    update_sheet_name( s );
}

/**
 * \brief Register a component as a reference to this sheet
 *
 * \param sheet The Sheet Referenced by \a comp
 * \param comp The compenent refering to \a sheet
 */

PUBLIC void sheet_register_ref( Sheet *s, Component *comp ) {

    s->referring_sheets = g_list_append( s->referring_sheets, comp );
}

/**
 * \brief Unregister a component as a reference to this sheet
 *
 * \param sheet The Sheet Referenced by \a comp
 * \param comp The compenent refering to \a sheet
 */


PUBLIC void sheet_unregister_ref( Sheet *s, Component *comp ) {
    s->referring_sheets = g_list_remove( s->referring_sheets, comp );
}

/**
 * \brief Is the sheet referneced by a Component ?
 *
 * \param s Sheet to be queried
 */

PUBLIC gboolean sheet_has_refs( Sheet *s ) {
    return ( s->referring_sheets != NULL );
}

/**
 * \brief kill all components which registered with this sheet
 *
 * \param s This sheet
 */

PUBLIC void sheet_kill_refs( Sheet *s ) {
    GList *compX = s->referring_sheets;

    while( compX != NULL ) {
	GList *temp = g_list_next( compX );
	Component *c = compX->data;

	sheet_delete_component( c->sheet, c );
	compX = temp;
    }
    s->referring_sheets = NULL;
}

/**
 * \brief create an empty sheet
 *
 * \return A pointer to a Sheet struct 
 */

PUBLIC Sheet *create_sheet( void ) {
  GtkWidget *eb;
  GdkColormap *colormap;
  int i;
  Sheet *sheet = safe_malloc( sizeof( Sheet ) );
  
  sheet->components = NULL;
  sheet->selected_comps = NULL;
  sheet->sel_valid = FALSE;
  sheet->sheetmode  = SHEET_MODE_NORMAL;
  sheet->sheetklass = NULL;
  sheet->panel_control_active = FALSE;
  sheet->panel_control = NULL;
  sheet->referring_sheets = NULL;
  sheet->visible = TRUE;
  sheet->highlight_ref.kind = COMP_NO_CONNECTOR;

  sheet->name = safe_malloc( sizeof( "sheet" ) + 20 );
  sprintf( sheet->name, "sheet%d", next_sheet_number++ );

  sheet->scrollwin = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(sheet->scrollwin);
  gtk_widget_ref(sheet->scrollwin);

  eb = gtk_event_box_new();
  gtk_widget_show(eb);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sheet->scrollwin), eb);

  sheet->drawingwidget = gtk_drawing_area_new();
  // connect mouse motion event handler
  gtk_signal_connect( GTK_OBJECT(sheet->drawingwidget),
	"motion_notify_event", (GtkSignalFunc)motion_notify_event, sheet);

  // unmask motion events so handler gets called
  gtk_widget_set_events(sheet->drawingwidget, GDK_EXPOSURE_MASK
	| GDK_LEAVE_NOTIFY_MASK
	| GDK_POINTER_MOTION_MASK
	| GDK_POINTER_MOTION_HINT_MASK);

  gtk_widget_show(sheet->drawingwidget);
  gtk_drawing_area_size(GTK_DRAWING_AREA(sheet->drawingwidget), GEN_AREA_LENGTH, GEN_AREA_LENGTH);
  gtk_container_add(GTK_CONTAINER(eb), sheet->drawingwidget);

  gtk_signal_connect(GTK_OBJECT(eb), "event", GTK_SIGNAL_FUNC(do_sheet_event), NULL);
  gtk_signal_connect(GTK_OBJECT(sheet->drawingwidget), "expose_event", GTK_SIGNAL_FUNC(do_sheet_repaint), NULL);

  gtk_object_set_user_data( GTK_OBJECT(sheet->drawingwidget), (gpointer) sheet );
  gtk_object_set_user_data( GTK_OBJECT(sheet->scrollwin), (gpointer) sheet );
  gtk_object_set_user_data( GTK_OBJECT(eb), (gpointer) sheet );

  colormap = gtk_widget_get_colormap(sheet->drawingwidget);

  for (i = 0; i < COMP_NUM_COLORS; i++) {
    gdk_colormap_alloc_color(colormap, &comp_colors[i], FALSE, TRUE);
    gdk_colormap_alloc_color(colormap, &comp_sel_colors[i], FALSE, TRUE);
  }
  
  //gui_register_sheet( sheet );


  //sheet->control_panel = control_panel_new( sheet->name );
  sheet_set_dirty( sheet, FALSE );
  update_sheet_name( sheet );

  return sheet;
}

/**
 * \brief Create a new component and add that to the sheet
 *
 * \param sheet The sheet to add to
 * \param k The Class of the Component
 * \param init_data This is a pointer to a structure from which the comp
 *                  constructor can initialise itself. 
 *
 *  See comp_new_component() for details
 */

PUBLIC Component *sheet_build_new_component(Sheet *sheet, ComponentClass *k, gpointer init_data) {
  Component *c = comp_new_component(k, init_data, sheet, sheet->saved_x, sheet->saved_y);

  if (c != NULL) {
    sheet->components = g_list_prepend(sheet->components, c);
    gtk_widget_queue_draw(sheet->drawingwidget);
    sheet_set_dirty( sheet, TRUE );
  }

  return c;
}

PUBLIC void sheet_add_component( Sheet *sheet, Component *c ) {
    if( c != NULL ) {
	sheet->components = g_list_prepend(sheet->components, c);
	gtk_widget_queue_draw(sheet->drawingwidget);
	sheet_set_dirty( sheet, TRUE );
    }
}

/**
 * \brief delete a component from the sheet
 *
 * \param sheet Sheet to delete from
 * \param c which component
 */

PUBLIC void sheet_delete_component(Sheet *sheet, Component *c) {

  if( comp_kill_component(c) ) {
      sheet->components = g_list_remove(sheet->components, c);
      if( g_list_find( sheet->selected_comps, c ) )
	  sheet->selected_comps = g_list_remove( sheet->selected_comps, c );
      sheet_set_dirty( sheet, TRUE );
  }

  gtk_widget_queue_draw(sheet->drawingwidget);
}

/**
 * \brief Component changed and needs to be redrawn
 *
 * \param sheet The sheet on which the component is.
 * \param c The component to be redrawn.
 *
 * This emits an expose event for the area of the component.
 * Is there a possibility to only call comp_paint here ?
 */

PUBLIC void sheet_queue_redraw_component(Sheet *sheet, Component *c) {
  gtk_widget_queue_draw_area(sheet->drawingwidget, c->x, c->y, c->width, c->height);
  sheet_set_dirty( sheet, TRUE );
}

/**
 * \brief Get GdkWindow of sheet
 *
 * \param sheet The Sheet
 */

PUBLIC GdkWindow *sheet_get_window(Sheet *sheet) {
  return sheet->drawingwidget->window;
}

/**
 * \brief Get the background color
 */

PUBLIC GdkColor *sheet_get_transparent_color(Sheet *sheet) {
  return &gtk_widget_get_style(sheet->drawingwidget)->bg[GTK_STATE_NORMAL];
}

/**
 * \brief Get the width of string \a text
 *
 * \param sheet The Sheet
 * \param text the string
 *
 * \return width of string in pixels
 */

PUBLIC int sheet_get_textwidth(Sheet *sheet, char *text) {
  //GtkStyle *style = gtk_widget_get_style(sheet->drawingwidget);
  PangoLayout *layout = gtk_widget_create_pango_layout( sheet->drawingwidget, text );
  int retval;

  pango_layout_get_pixel_size(layout, &retval, NULL);

  g_object_unref( G_OBJECT( layout ) );

  return retval;
}

PUBLIC int sheet_get_textheight(Sheet *sheet, char *text) {
  //GtkStyle *style = gtk_widget_get_style(sheet->drawingwidget);
  PangoLayout *layout = gtk_widget_create_pango_layout( sheet->drawingwidget, text );
  int retval;

  pango_layout_get_pixel_size(layout, NULL, &retval );

  g_object_unref( G_OBJECT( layout ) );

  return retval;
}

PUBLIC gboolean sheet_dont_like_be_destroyed( Sheet *sheet ) {

    if( sheet->dirty ) {
	GtkWidget *dirtydialog = gtk_message_dialog_new( 
		GTK_WINDOW( gtk_widget_get_toplevel( sheet->scrollwin ) ),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_NONE,
		"Sheet is not saved !!!\nGo ahead ?" );

	gtk_dialog_add_button( GTK_DIALOG( dirtydialog ), GTK_STOCK_SAVE, 0 );
	gtk_dialog_add_button( GTK_DIALOG( dirtydialog ), GTK_STOCK_REMOVE, GTK_RESPONSE_REJECT );
	gtk_dialog_add_button( GTK_DIALOG( dirtydialog ), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL );

	gint dialog_result = gtk_dialog_run( GTK_DIALOG( dirtydialog ) );
	gtk_widget_destroy( GTK_WIDGET( dirtydialog ) );

	switch( dialog_result ) {
	    case 0:
		if( save_sheet( sheet, NULL ) )
		    return 0;
		else
		    return 1;
	    case GTK_RESPONSE_REJECT:
		return 0;
	    case GTK_RESPONSE_CANCEL:
		return 1;
	}
    }

    return 0;
}

/**
 * \brief kill all components on sheet
 *
 * \param sheet Sheet to clear
 */

PUBLIC void sheet_clear(Sheet *sheet) {


    if( sheet_dont_like_be_destroyed( sheet ) )
	return;
	    
  sheet_kill_refs( sheet );
  while (sheet->components != NULL) {
    GList *temp = g_list_next(sheet->components);

    if( !comp_kill_component(sheet->components->data) )
	return;

    g_list_free_1(sheet->components);
    sheet->components = temp;
  }

  gtk_widget_queue_draw(sheet->drawingwidget);
  reset_control_panel();

  sheet_set_dirty( sheet, FALSE );
}

/**
 * \brief free all memory allocated by sheet
 * 
 * \param sheet The Sheet
 */

PUBLIC void sheet_remove( Sheet *sheet ) {

    if( sheet_dont_like_be_destroyed( sheet ) )
	return;

    sheet_clear( sheet );
    gui_unregister_sheet( sheet );

    if( sheet->control_panel ) {
	control_panel_unregister_panel( sheet->control_panel );
	//safe_free( sheet->control_panel );
    }
    gtk_widget_unref( sheet->scrollwin );
    if( sheet->name )
	safe_free( sheet->name );
    safe_free( sheet );
    
}

PRIVATE GtkWidget *rename_text_widget=NULL;

PRIVATE void rename_handler(MsgBoxResponse action_taken, Sheet *s) {
    if (action_taken == MSGBOX_OK) {

	GList *compX;

	free(s->name);
	if( s->control_panel != NULL ) {
	    if( s->control_panel->name != NULL )
		free(s->control_panel->name);
	    s->control_panel->name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(rename_text_widget)));
	    update_panel_name( s->control_panel );
	}
		    
	s->name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(rename_text_widget)));
	update_sheet_name( s );

	for( compX = s->referring_sheets; compX != NULL; compX=g_list_next( compX ) ) {
	    Component *c = compX->data;

	    sheet_queue_redraw_component(c->sheet, c);	/* to 'erase' the old size */
	    shcomp_resize(c);
	    sheet_queue_redraw_component(c->sheet, c);	/* to 'fill in' the new size */
	}
    }
}

/**
 * \brief popup Rename msgbox and rename sheet
 *
 * \param sheet The Sheet
 */

PUBLIC void sheet_rename(Sheet *sheet ) {
  GtkWidget *hb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Rename Sheet:");
  GtkWidget *text = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_entry_set_text(GTK_ENTRY(text), sheet->name);

  rename_text_widget = text;
  popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	       (MsgBoxResponseHandler) rename_handler, sheet);
}

/**
 * \brief unpickle a Sheet from ObjectStoreItem
 *
 * \param item The ObjectStoreItem representing the Sheet
 *
 * \return A Sheet as it was saved
 */

PUBLIC Sheet *sheet_unpickle( ObjectStoreItem *item ) {

    // First i give it the Sheet Struct which is already there
    // but later it will be created here and given back by sheet_loadfrom

    Sheet *s = objectstore_get_object( item );

    if( s == NULL ) {

	ObjectStoreDatum *sheetlist = objectstore_item_get( item, "sheets" );
	
	s=create_sheet( );
	s->name = safe_string_dup( objectstore_item_get_string( item, "name", "strango" ) );

	if( load_hidden )
	    s->visible = FALSE;
	else
	    s->visible = objectstore_item_get_integer( item, "visible", TRUE );
	
	objectstore_set_object( item, s );
	s->control_panel = control_panel_unpickle( objectstore_item_get_object( item, "control_panel" ) );
	gui_register_sheet( s );
	
	s->panel_control_active = objectstore_item_get_integer(item, "panel_control_active", FALSE );
	if( s->panel_control_active )
	    s->panel_control = control_unpickle( objectstore_item_get_object(item, "panel_control" ) );
	else
	    s->panel_control = NULL;

	s->components = objectstore_extract_list_of_items(objectstore_item_get(item, "components"),
							 item->db, (objectstore_unpickler_t) comp_unpickle);

	if( sheetlist ) {
	    objectstore_extract_list_of_items(sheetlist, item->db, 
		    (objectstore_unpickler_t) sheet_unpickle);
	}
    }
    return s;
}


/**
 * \brief pickle a Sheet into ObjectStore
 *
 * \param sheet the Sheet to be pickled
 * \param db ObjectStore into which the sheet should be pickled.
 *
 * \return The ObjectStoreItem representing this sheet (new one or the old one if it has already been created)
 */

PUBLIC ObjectStoreItem *sheet_pickle( Sheet *sheet, ObjectStore *db ) {

    ObjectStoreItem *item = objectstore_get_item( db, sheet );

    if( item == NULL ) {

	item = objectstore_new_item( db, "Sheet", (gpointer) sheet );
	objectstore_item_set_string( item, "name", sheet->name );

	if( sheet->control_panel )
	    objectstore_item_set_object( item, "control_panel", control_panel_pickle( sheet->control_panel, db ) );
	objectstore_item_set_integer(item, "panel_control_active", sheet->panel_control_active );
	objectstore_item_set_integer(item, "visible", sheet->visible );
	if( sheet->panel_control_active )
	    objectstore_item_set_object(item, "panel_control", control_pickle( sheet->panel_control, db ) );

	objectstore_item_set( item, "components",
		objectstore_create_list_of_items( sheet->components, db,
		    (objectstore_pickler_t) comp_pickle ));

	sheet_set_dirty( sheet, FALSE );
    }	
    return item;
}


PUBLIC Sheet *sheet_clone( Sheet *sheet ) {

    Sheet *clone = create_sheet();
    ControlPanel *cp;

    free( clone->name );
    clone->name = safe_string_dup( sheet->name );
    update_sheet_name( sheet );

    cp = clone->control_panel = control_panel_new( clone->name, TRUE, clone );
    clone->visible = FALSE;
    gtk_layout_move( GTK_LAYOUT( cp->fixedwidget ), cp->sizer_ebox, sheet->control_panel->sizer_x+16, sheet->control_panel->sizer_y+16 );

    if( sheet->control_panel->bg_image_name ) {
	cp->bg_image_name = safe_string_dup( sheet->control_panel->bg_image_name );
	if( clone->panel_control_active )
	    control_update_bg( clone->panel_control );
    }

    comp_clone_list( sheet->components, clone );
    clone->dirty = FALSE;
    return clone;
}

/**
 * \brief load a sheet from a file
 *
 * \param sheet This was for the migration process (needs to be removed and is ignored)
 * \param f The FILE * where the sheet is in
 *
 * \return The Sheet from the object store
 *
 * I Think this will be changed to have an ObjectStore as parameter.
 */

PUBLIC Sheet *sheet_loadfrom(Sheet *sheet, FILE *f) {
  ObjectStore *db = objectstore_new_objectstore();
  ObjectStoreItem *root;

  if (!objectstore_read(f, db)) {
    objectstore_kill_objectstore(db);
    return NULL;
  }

  root = objectstore_get_root(db);
  sheet = sheet_unpickle( root );
  sheet_set_dirty( sheet, FALSE );

  objectstore_kill_objectstore(db);
  reset_control_panel();
  return sheet;
}

/**
 * \brief save sheet to FILE
 *
 * \param sheet The Sheet to save
 * \param f The FILE * to save to
 * \param sheet_only FALSE if all other sheets should be saved to.
 */

PUBLIC void sheet_saveto(Sheet *sheet, FILE *f, gboolean sheet_only ) {
  ObjectStore *db;
  ObjectStoreItem *root;

  
  db = objectstore_new_objectstore();
  root = sheet_pickle( sheet, db );
  //ObjectStoreItem *root = objectstore_new_item(db, "Sheet", (gpointer) sheet);

  objectstore_set_root(db, root);

  if( !sheet_only )
      objectstore_item_set(root, "sheets", objectstore_create_list_of_items(get_sheet_list(),
		  db, (objectstore_pickler_t) sheet_pickle));

  objectstore_write(f, db);
  objectstore_kill_objectstore(db);
  sheet_set_dirty( sheet, FALSE );
}


