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

#define GEN_AREA_LENGTH		2048

enum SheetModes {
  SHEET_MODE_NORMAL = 0,
  SHEET_MODE_SCROLLING,
  SHEET_MODE_PRESSING,
  SHEET_MODE_DRAGGING_COMP,
  SHEET_MODE_DRAGGING_LINK,

  SHEET_MAX_MODE
};

PRIVATE int sheetmode = SHEET_MODE_NORMAL;
PRIVATE gdouble saved_x, saved_y;
PRIVATE ConnectorReference saved_ref;

PRIVATE GtkWidget *scrollwin = NULL;
PRIVATE GtkWidget *drawingwidget = NULL;
PRIVATE GdkColor comp_colors[COMP_NUM_COLORS] = {
  { 0, 0, 0, 0 },
  { 0, 65535, 65535, 65535 },
  { 0, 65535, 0, 0 },
  { 0, 0, 65535, 0 },
  { 0, 0, 0, 65535 },
  { 0, 65535, 65535, 0 }
};

PRIVATE GList *components = NULL;

PRIVATE gboolean do_sheet_repaint(GtkWidget *widget, GdkEventExpose *ee) {
  GdkDrawable *drawable = widget->window;
  GtkStyle *style = gtk_widget_get_style(widget);
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

  for (l = g_list_last(components); l != NULL; l = g_list_previous(l)) {
    Component *c = l->data;
    GdkRectangle r_gen, r;

    comp_paint_connections(c, &area, drawable, style, comp_colors);

    r_gen.x = c->x;
    r_gen.y = c->y;
    r_gen.width = c->width;
    r_gen.height = c->height;

    if (gdk_rectangle_intersect(&r_gen, &area, &r))
      comp_paint(c, &area, drawable, style, comp_colors);
  }

  return TRUE;
}

PRIVATE void scroll_follow_mouse(GdkEventMotion *me) {
  GtkAdjustment *ha = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrollwin));
  GtkAdjustment *va = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrollwin));

  gtk_adjustment_set_value(ha, saved_x - me->x_root);
  gtk_adjustment_set_value(va, saved_y - me->y_root);
  saved_x = ha->value + me->x_root;
  saved_y = va->value + me->y_root;
}

PRIVATE Component *find_component_at(gint x, gint y) {
  GList *lst = components;

  while (lst != NULL) {
    Component *c = lst->data;

    if (comp_contains_point(c, x, y))
      return c;

    lst = g_list_next(lst);
  }

  return NULL;
}

PRIVATE GList *find_components_at(gint x, gint y) {
  GList *lst = components;
  GList *accum = NULL;

  while (lst != NULL) {
    Component *c = lst->data;

    if (comp_contains_point(c, x, y))
      accum = g_list_append(accum, c);

    lst = g_list_next(lst);
  }

  return accum;
}

PRIVATE void process_click(GtkWidget *w, GdkEventButton *be) {
  Component *c = find_component_at(be->x, be->y);

  if (c != NULL) {
    int found_conn;
    saved_ref.kind = COMP_NO_CONNECTOR;

    found_conn = comp_find_connector(c, be->x, be->y, &saved_ref);

    if (!found_conn) {
      components = g_list_remove(components, c);
      components = g_list_prepend(components, c);
      saved_ref.c = c;
      gtk_widget_queue_draw_area(drawingwidget, c->x, c->y, c->width, c->height);
    }

    saved_x = c->x - be->x_root;
    saved_y = c->y - be->y_root;
    sheetmode = SHEET_MODE_PRESSING;
  }
}

PRIVATE void disconnect_handler(GtkWidget *menuitem, ConnectorReference *ref) {
  comp_unlink(&saved_ref, ref);
  gtk_widget_queue_draw(drawingwidget);
}

PRIVATE void disconnect_all_handler(void) {
  Connector *con = comp_get_connector(&saved_ref);
  GList *lst = con->refs;

  while (lst != NULL) {
    ConnectorReference *ref = lst->data;
    lst = g_list_next(lst);

    comp_unlink(&con->ref, ref);
  }

  gtk_widget_queue_draw(drawingwidget);
}

#define DISCONNECT_STRING "Disconnect "
PRIVATE void append_disconnect_menu(Connector *con, GtkWidget *menu) {
  GtkWidget *submenu;
  GtkWidget *item;
  GList *lst;
  char *name;
  char *label;

  saved_ref = con->ref;

  submenu = gtk_menu_new();

  for (lst = con->refs; lst != NULL; lst = g_list_next(lst)) {
    ConnectorReference *ref = lst->data;

    name = comp_get_connector_name(ref);
    item = gtk_menu_item_new_with_label(name);
    free(name);

    gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(disconnect_handler), ref);
    gtk_menu_append(GTK_MENU(submenu), item);
    gtk_widget_show(item);
  }

  item = gtk_menu_item_new_with_label("ALL");
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

PRIVATE void do_popup_menu(GdkEventButton *be) {
  static GtkWidget *old_popup_menu = NULL;
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *newmenu;
  GList *comps;

  saved_x = be->x;
  saved_y = be->y;

  if (old_popup_menu != NULL) {
    gtk_widget_unref(old_popup_menu);
    old_popup_menu = NULL;
  }

  menu = gtk_menu_new();

  item = gtk_menu_item_new_with_label("New");
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);

  newmenu = comp_get_newmenu();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), newmenu);

  comps = find_components_at(be->x, be->y);

  if (comps != NULL) {
    ConnectorReference ref;

    if (comp_find_connector(comps->data, be->x, be->y, &ref))
      append_disconnect_menu(comp_get_connector(&ref), menu);
  }

  while (comps != NULL) {
    GList *tmp = g_list_next(comps);

    comp_append_popup(comps->data, menu);

    g_list_free_1(comps);
    comps = tmp;
  }

  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 be->button, be->time);

  old_popup_menu = menu;
}

PRIVATE gboolean do_sheet_event(GtkWidget *w, GdkEvent *e) {
  switch (e->type) {
    case GDK_BUTTON_PRESS: {
      GdkEventButton *be = (GdkEventButton *) e;

      switch (be->button) {
	case 1:
	  process_click(w, be);
	  return TRUE;

	case 2:
	  if (sheetmode == SHEET_MODE_NORMAL) {
	    GtkAdjustment *ha =
	      gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrollwin));
	    GtkAdjustment *va =
	      gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrollwin));
	    sheetmode = SHEET_MODE_SCROLLING;
	    saved_x = be->x_root + ha->value;
	    saved_y = be->y_root + va->value;
	    return TRUE;
	  }
	  break;

	case 3:
	  do_popup_menu(be);
	  return TRUE;

	default:
	  break;
      }
      break;
    }

    case GDK_BUTTON_RELEASE: {
      GdkEventButton *be = (GdkEventButton *) e;

      switch (sheetmode) {
	case SHEET_MODE_SCROLLING:
	  break;

	case SHEET_MODE_DRAGGING_COMP:
	  gtk_widget_queue_draw(drawingwidget);
	  break;

	case SHEET_MODE_DRAGGING_LINK: {
	  Component *c = find_component_at(be->x, be->y);

	  if (c != NULL) {
	    ConnectorReference ref;

	    if (comp_find_connector(c, be->x, be->y, &ref)) {
	      comp_link(&saved_ref, &ref);
	      gtk_widget_queue_draw(drawingwidget);
	    }
	  }

	  break;
	}

	default:
	  break;
      }

      sheetmode = SHEET_MODE_NORMAL;
      return TRUE;
    }

    case GDK_MOTION_NOTIFY: {
      GdkEventMotion *me = (GdkEventMotion *) e;

      switch (sheetmode) {
	case SHEET_MODE_SCROLLING:
	  scroll_follow_mouse(me);
	  return TRUE;

	case SHEET_MODE_PRESSING:
	  if (saved_ref.kind != COMP_NO_CONNECTOR) {
	    sheetmode = SHEET_MODE_DRAGGING_LINK;
	    break;
	  } else {
	    sheetmode = SHEET_MODE_DRAGGING_COMP;
	  }

	  /* FALL THROUGH */

	case SHEET_MODE_DRAGGING_COMP:
	  gtk_widget_queue_draw_area(drawingwidget,
				     saved_ref.c->x, saved_ref.c->y,
				     saved_ref.c->width, saved_ref.c->height);
	  saved_ref.c->x = MIN(MAX(saved_x + me->x_root, 0),GEN_AREA_LENGTH - saved_ref.c->width);
	  saved_ref.c->y = MIN(MAX(saved_y + me->y_root, 0),GEN_AREA_LENGTH - saved_ref.c->height);
	  gtk_widget_queue_draw_area(drawingwidget,
				     saved_ref.c->x, saved_ref.c->y,
				     saved_ref.c->width, saved_ref.c->height);
	  break;

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

PUBLIC GtkWidget *create_sheet(GtkBox *parent) {
  GtkWidget *eb;
  GdkColormap *colormap;
  int i;

  scrollwin = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_show(scrollwin);
  gtk_box_pack_start(parent, scrollwin, TRUE, TRUE, 0);

  eb = gtk_event_box_new();
  gtk_widget_show(eb);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollwin), eb);

  drawingwidget = gtk_drawing_area_new();
  gtk_widget_show(drawingwidget);
  gtk_drawing_area_size(GTK_DRAWING_AREA(drawingwidget), GEN_AREA_LENGTH, GEN_AREA_LENGTH);
  gtk_container_add(GTK_CONTAINER(eb), drawingwidget);

  gtk_signal_connect(GTK_OBJECT(eb), "event", GTK_SIGNAL_FUNC(do_sheet_event), NULL);
  gtk_signal_connect(GTK_OBJECT(drawingwidget), "expose_event", GTK_SIGNAL_FUNC(do_sheet_repaint), NULL);

  if (!GTK_WIDGET_REALIZED(drawingwidget))
    gtk_widget_realize(drawingwidget);

  colormap = gtk_widget_get_colormap(drawingwidget);

  for (i = 0; i < COMP_NUM_COLORS; i++)
    gdk_colormap_alloc_color(colormap, &comp_colors[i], FALSE, TRUE);

  return scrollwin;
}

PUBLIC void sheet_build_new_component(ComponentClass *k, gpointer init_data) {
  Component *c = comp_new_component(k, init_data, saved_x, saved_y);

  if (c != NULL) {
    components = g_list_prepend(components, c);
    gtk_widget_queue_draw(drawingwidget);
  }
}

PUBLIC void sheet_delete_component(Component *c) {
  components = g_list_remove(components, c);
  comp_kill_component(c);
  gtk_widget_queue_draw(drawingwidget);
}

PUBLIC void sheet_queue_redraw_component(Component *c) {
  gtk_widget_queue_draw_area(drawingwidget, c->x, c->y, c->width, c->height);
}

PUBLIC GdkWindow *sheet_get_window(void) {
  return drawingwidget->window;
}

PUBLIC GdkColor *sheet_get_transparent_color(void) {
  return &gtk_widget_get_style(drawingwidget)->bg[GTK_STATE_NORMAL];
}

PUBLIC int sheet_get_textwidth(char *text) {
  GtkStyle *style = gtk_widget_get_style(drawingwidget);

  return gdk_text_width(style->font, text, strlen(text));
}

PUBLIC void sheet_clear(void) {
  while (components != NULL) {
    GList *temp = g_list_next(components);

    comp_kill_component(components->data);

    g_list_free_1(components);
    components = temp;
  }

  gtk_widget_queue_draw(drawingwidget);
  reset_control_panel();
}

PUBLIC gboolean sheet_loadfrom(FILE *f) {
  ObjectStore *db = objectstore_new_objectstore();
  ObjectStoreItem *root;

  if (!objectstore_read(f, db)) {
    objectstore_kill_objectstore(db);
    return FALSE;
  }

  sheet_clear();	/* empties GList *components. */

  root = objectstore_get_root(db);
  components = objectstore_extract_list_of_items(objectstore_item_get(root, "components"),
						 db, (objectstore_unpickler_t) comp_unpickle);

  objectstore_kill_objectstore(db);
  reset_control_panel();
  return TRUE;
}

PUBLIC void sheet_saveto(FILE *f) {
  ObjectStore *db = objectstore_new_objectstore();
  ObjectStoreItem *root = objectstore_new_item(db, "Sheet", (gpointer) 0xdeadbeef);

  objectstore_set_root(db, root);
  objectstore_item_set(root, "components",
		       objectstore_create_list_of_items(components, db,
							(objectstore_pickler_t) comp_pickle));
  objectstore_write(f, db);
  objectstore_kill_objectstore(db);
}
