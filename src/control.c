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
#include <math.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "gui.h"
#include "comp.h"
#include "control.h"
#include "gtkknob.h"
#include "gtkslider.h"
#include "msgbox.h"
#include "sheet.h"

#define GAUGE_SIZE 32
#define GRANULARITY 8

PRIVATE GtkWidget *control_panel = NULL;
PRIVATE ControlPanel *global_panel = NULL;
PRIVATE GtkWidget *control_notebook = NULL;
//PRIVATE GtkWidget *fixed_widget = NULL;
PRIVATE GList *control_panels = NULL;

PUBLIC void control_panel_register_panel( ControlPanel *panel, char *name, gboolean add_fixed) {

    panel->scrollwin = gtk_scrolled_window_new( NULL, NULL );
    if( add_fixed )
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( panel->scrollwin ), panel->fixedwidget );

    gtk_widget_show(panel->scrollwin);

    gtk_notebook_append_page( GTK_NOTEBOOK( control_notebook ), panel->scrollwin, gtk_label_new( name ) );
    control_panels = g_list_append( control_panels, panel );
    panel->visible = TRUE;
}

PUBLIC void control_panel_unregister_panel( ControlPanel *panel ) {

    int pagenum;

    control_panels = g_list_remove( control_panels, panel );
    pagenum = gtk_notebook_page_num( GTK_NOTEBOOK( control_notebook ), panel->scrollwin );
    gtk_notebook_remove_page( GTK_NOTEBOOK( control_notebook ), pagenum );
    panel->scrollwin = NULL;
    panel->visible = FALSE;
}

PRIVATE void control_moveto(Control *c, int x, int y) {
  x = floor((x + (GRANULARITY>>1)) / ((gdouble) GRANULARITY)) * GRANULARITY;
  y = floor((y + (GRANULARITY>>1)) / ((gdouble) GRANULARITY)) * GRANULARITY;

  if (x != c->x || y != c->y) {
    x = MAX(x, 0);
    y = MAX(y, 0);
    gtk_fixed_move(GTK_FIXED(c->panel == NULL ? global_panel->fixedwidget : c->panel->fixedwidget),
		c->whole, x, y);
    c->x = x;
    c->y = y;
  }
}

PRIVATE void knob_slider_handler(GtkWidget *adj, Control *c) {
  control_emit(c, GTK_ADJUSTMENT(adj)->value);
}

PRIVATE void toggled_handler(GtkWidget *button, Control *c) {
  control_emit(c, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ? 1 : 0);
}

PRIVATE void clicked_handler(GtkWidget *button, Control *c) {
  control_emit(c, 1);
}

PRIVATE void entry_changed(GtkEntry *entry, GtkAdjustment *adj) {
  adj->value = atof(gtk_entry_get_text(entry));
  gtk_signal_handler_block_by_data(GTK_OBJECT(adj), entry);
  gtk_signal_emit_by_name(GTK_OBJECT(adj), "value_changed");
  gtk_signal_handler_unblock_by_data(GTK_OBJECT(adj), entry);
}

PRIVATE void update_entry(GtkAdjustment *adj, GtkEntry *entry) {
  char buf[128];
  sprintf(buf, "%g", adj->value);
  gtk_signal_handler_block_by_data(GTK_OBJECT(entry), adj);
  gtk_entry_set_text(entry, buf);
  gtk_signal_handler_unblock_by_data(GTK_OBJECT(entry), adj);
}

PRIVATE void delete_ctrl_handler(GtkWidget *widget, Control *c) {
  control_kill_control(c);
}

PRIVATE GtkWidget *ctrl_rename_text_widget = NULL;
PRIVATE void ctrl_rename_handler(MsgBoxResponse action_taken, Control *c) {
  if (action_taken == MSGBOX_OK) {
    char *newname = gtk_entry_get_text(GTK_ENTRY(ctrl_rename_text_widget));

    if (c->name != NULL) {
      free(c->name);
      c->name = NULL;
    }

    if (newname[0] != '\0')	/* nonempty string */
      c->name = safe_string_dup(newname);

    control_update_names(c);
  }
}

PRIVATE void rename_ctrl_handler(GtkWidget *widget, Control *c) {
  GtkWidget *hb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Rename control:");
  GtkWidget *text = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_entry_set_text(GTK_ENTRY(text), c->name ? c->name : "");

  ctrl_rename_text_widget = text;
  popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	       (MsgBoxResponseHandler) ctrl_rename_handler, c);
}

PRIVATE void fold_ctrl_handler(GtkWidget *widget, Control *c) {
    c->folded = !(c->folded);

    if( c->folded )
	gtk_widget_hide( c->widget );
    else
	gtk_widget_show( c->widget );

    gtk_widget_queue_resize( c->whole );
}

PRIVATE void popup_menu(Control *c, GdkEventButton *be) {
  static GtkWidget *old_popup_menu = NULL;
  GtkWidget *menu;
  GtkWidget *item;

  if (old_popup_menu != NULL) {
    gtk_widget_unref(old_popup_menu);
    old_popup_menu = NULL;
  }

  menu = gtk_menu_new();

  item = gtk_menu_item_new_with_label("Delete");
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(delete_ctrl_handler), c);

  item = gtk_menu_item_new_with_label("Rename...");
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(rename_ctrl_handler), c);

  item = gtk_check_menu_item_new_with_label( "Folded" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->folded );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(fold_ctrl_handler), c);


  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 be->button, be->time);

  old_popup_menu = menu;
}

PRIVATE gboolean eventbox_handler(GtkWidget *eventbox, GdkEvent *event, Control *c) {
  /* There's something fucked wrt event catching going on here. Lots of weird
     behaviour :-( it's pretty simple, how can it be broken?? */
  /* I might have fixed this by moving to an x_root/y_root solution a la sheet.c */
  /* It seems not. SIGH!!! */

  switch (event->type) {
    case GDK_BUTTON_PRESS: {
      GdkEventButton *be = (GdkEventButton *) event;

      switch (be->button) {
	case 1:
	  if (!c->moving) {
	    c->moving = 1;
	    gtk_grab_add(eventbox);
	    c->saved_x = c->x - be->x_root;
	    c->saved_y = c->y - be->y_root;
	  }
	  break;

	case 2:
	case 3:
	  popup_menu(c, be);
	  break;
      }
      return TRUE;
    }

    case GDK_BUTTON_RELEASE: {
      if (c->moving) {
	c->moving = 0;
	gtk_grab_remove(eventbox);
      }
      return TRUE;
    }

    case GDK_MOTION_NOTIFY: {
      GdkEventMotion *me = (GdkEventMotion *) event;

      if (c->moving) {
	control_moveto(c,
		       c->saved_x + me->x_root,
		       c->saved_y + me->y_root);
      }

      return TRUE;
    }

    default:
      return FALSE;
  }
}

PUBLIC Control *control_new_control(ControlDescriptor *desc, Generator *g, ControlPanel *panel) {
  Control *c = safe_malloc(sizeof(Control));
  GtkAdjustment *adj = NULL;

  c->desc = desc;
  c->name = NULL;
  c->min = desc->min;
  c->max = desc->max;
  c->step = desc->step;
  c->page = desc->page;

  c->folded = FALSE;

  if( panel == NULL && global_panel == NULL )
      global_panel = control_panel_new( "Global", TRUE, NULL );

  c->panel = panel;

  c->moving = c->saved_x = c->saved_y = 0;
  c->x = 0;
  c->y = 0;
  c->events_flow = TRUE;

  c->whole = NULL;
  c->g = g;
  c->data = NULL;

  switch (desc->kind) {
    case CONTROL_KIND_SLIDER:
      c->widget = gtk_slider_new(NULL, c->desc->size);
      adj = gtk_slider_get_adjustment(GTK_SLIDER(c->widget));
      break;

    case CONTROL_KIND_KNOB:
      c->widget = gtk_knob_new(NULL);
      adj = gtk_knob_get_adjustment(GTK_KNOB(c->widget));
      break;

    case CONTROL_KIND_TOGGLE:
      c->widget = gtk_toggle_button_new_with_label("0/1");
      gtk_signal_connect(GTK_OBJECT(c->widget), "toggled", GTK_SIGNAL_FUNC(toggled_handler), c);
      break;

    case CONTROL_KIND_BUTTON:
      c->widget = gtk_button_new();
      gtk_widget_set_usize(c->widget, 24, 8);
      gtk_signal_connect(GTK_OBJECT(c->widget), "clicked", GTK_SIGNAL_FUNC(clicked_handler), c);
      break;

    case CONTROL_KIND_USERDEF:
    case CONTROL_KIND_PANEL:
      c->widget = NULL;
      break;

    default:
      g_error("Unknown control kind %d (ControlDescriptor named '%s')",
	      desc->kind,
	      desc->name);
  }

  if (desc->initialize)
    desc->initialize(c);

  if (c->widget == NULL) {
    free(c);
    return NULL;
  }

  if (adj != NULL) {
    adj->lower = c->min;
    adj->upper = c->max;
    adj->value = c->min;
    adj->step_increment = c->step;
    adj->page_increment = c->page;

    gtk_signal_connect(GTK_OBJECT(adj), "value_changed", GTK_SIGNAL_FUNC(knob_slider_handler), c);
  }

  {
    GtkWidget *eventbox;
    GtkWidget *vbox;

    if( desc->kind == CONTROL_KIND_PANEL )
	c->this_panel = desc->refresh_data;
    else
	c->this_panel = NULL;

    c->title_frame = gtk_frame_new(g == NULL ? c->this_panel->name : g->name);
    gtk_widget_show(c->title_frame);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(c->title_frame), vbox);
    gtk_widget_show(vbox);

    eventbox = gtk_event_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), eventbox, FALSE, FALSE, 0);
    gtk_widget_show(eventbox);
    gtk_signal_connect(GTK_OBJECT(eventbox), "event", GTK_SIGNAL_FUNC(eventbox_handler), c);

    c->title_label = gtk_label_new(c->name ? c->name : desc->name);
    gtk_container_add(GTK_CONTAINER(eventbox), c->title_label);
    gtk_widget_show(c->title_label);

    if( desc->kind == CONTROL_KIND_PANEL )
	gtk_widget_reparent( c->widget, vbox );
    else
	gtk_box_pack_start(GTK_BOX(vbox), c->widget, FALSE, FALSE, 0);

    gtk_widget_show(c->widget);

    if (adj != NULL && c->desc->allow_direct_edit) {
      GtkWidget *entry = gtk_entry_new();
      gtk_widget_set_usize(entry, GAUGE_SIZE, 0);
      gtk_widget_show(entry);
      gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

      gtk_signal_connect(GTK_OBJECT(entry), "activate",
			 GTK_SIGNAL_FUNC(entry_changed), adj);
      gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
			 GTK_SIGNAL_FUNC(update_entry), entry);
    }

    c->whole = c->title_frame;
    gtk_fixed_put(GTK_FIXED(panel == NULL ? global_panel->fixedwidget : panel->fixedwidget),
	    c->whole, c->x, c->y);

    if (!GTK_WIDGET_REALIZED(eventbox))
      gtk_widget_realize(eventbox);
    if (!GTK_WIDGET_REALIZED(c->widget))
      gtk_widget_realize(c->widget);

    gdk_window_set_events(eventbox->window, 
	    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK  );
  }

  if( c->desc->kind != CONTROL_KIND_PANEL ) {
      gen_register_control(c->g, c);
      gen_update_controls( c->g, -1 );
  }


  return c;
}

PUBLIC void control_kill_control(Control *c) {
  g_return_if_fail(c != NULL);

  if( c->desc->destroy != NULL )
      c->desc->destroy( c );
  gtk_widget_hide(c->whole);
  gtk_container_remove(GTK_CONTAINER(c->panel == NULL ? global_panel->fixedwidget : c->panel->fixedwidget), c->whole);

  if (c->name != NULL)
    free(c->name);

  if (c->g != NULL)
    gen_deregister_control(c->g, c);

  free(c);
}

PRIVATE int find_control_index(Control *c) {
  int i;

  for (i = 0; i < c->g->klass->numcontrols; i++)
    if (&c->g->klass->controls[i] == c->desc)
      return i;

  g_error("Control index unfindable! c->desc->name is %p (%s)", c->desc->name, c->desc->name);
  return -1;
}

PRIVATE GtkWidget *panel_fixed = NULL;

PRIVATE void init_panel( Control *control ) {
    control->widget = panel_fixed;
}

PRIVATE void done_panel( Control *control ) {

    GtkWidget *viewport; 


    control->this_panel->sheet->panel_control_active = FALSE;
    control->this_panel->sheet->panel_control = NULL;

    control_panel_register_panel( control->this_panel, control->this_panel->name, FALSE );
    viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW(control->this_panel->scrollwin)),
	    gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(control->this_panel->scrollwin)));
    gtk_container_add (GTK_CONTAINER (control->this_panel->scrollwin), viewport);
    gtk_widget_reparent( control->this_panel->fixedwidget, viewport );
    gtk_widget_show( viewport );
}

PRIVATE  ControlDescriptor desc = 
  { CONTROL_KIND_PANEL, "panel", 0,0,0,0, 0,FALSE, TRUE,0, init_panel, done_panel, NULL, NULL };


PUBLIC Control *control_unpickle(ObjectStoreItem *item) {
  Generator *g = gen_unpickle(objectstore_item_get_object(item, "generator"));
  int control_index = objectstore_item_get_integer(item, "desc_index", 0);
  ObjectStoreItem *ccp = objectstore_item_get_object( item, "panel" );
  ControlPanel *cp = ((ccp == NULL) ? NULL : control_panel_unpickle( ccp ));
  ControlPanel *tp = control_panel_unpickle( objectstore_item_get_object( item, "this_panel" ));
  Control *c; 
  char *name;
  int x, y;

  if( g == NULL ) {
      desc.refresh_data = tp;
      panel_fixed = tp->fixedwidget;
      c = control_new_control( &desc, NULL, cp );
      control_panel_unregister_panel( tp );
  } else {
      c = control_new_control(&g->klass->controls[control_index], g, cp);
  }

  
  name = objectstore_item_get_string(item, "name", NULL);
  c->name = name ? safe_string_dup(name) : NULL;
  if (name)
    control_update_names(c);

  c->min = objectstore_item_get_double(item, "min", 0);
  c->max = objectstore_item_get_double(item, "max", 100);
  c->step = objectstore_item_get_double(item, "step", 1);
  c->page = objectstore_item_get_double(item, "page", 1);

  if( (c->folded = objectstore_item_get_integer(item, "folded", 0)) )
      gtk_widget_hide( c->widget );

  x = objectstore_item_get_integer(item, "x_coord", 0);
  y = objectstore_item_get_integer(item, "y_coord", 0);
  control_moveto(c, x, y);
  c->events_flow = TRUE;

  return c;
}

PUBLIC ObjectStoreItem *control_pickle(Control *c, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_new_item(db, "Control", c);
  if( c->g != NULL ) {
      objectstore_item_set_object(item, "generator", gen_pickle(c->g, db));
      objectstore_item_set_integer(item, "desc_index", find_control_index(c));
  }

  if( c->this_panel )
      objectstore_item_set_object(item, "this_panel", control_panel_pickle(c->this_panel, db));

  if( c->panel )
      objectstore_item_set_object(item, "panel", control_panel_pickle(c->panel, db));
  if (c->name)
    objectstore_item_set_string(item, "name", c->name);
  objectstore_item_set_double(item, "min", c->min);
  objectstore_item_set_double(item, "max", c->max);
  objectstore_item_set_double(item, "step", c->step);
  objectstore_item_set_double(item, "page", c->page);
  objectstore_item_set_integer(item, "x_coord", c->x);
  objectstore_item_set_integer(item, "y_coord", c->y);
  objectstore_item_set_integer(item, "folded", c->folded);
  /* don't save c->data in any form, Controls are MVC views, not models. */
  return item;
}

PUBLIC void control_emit(Control *c, gdouble number) {
  AEvent e;

  if (!c->events_flow)
    return;

  gen_init_aevent(&e, AE_NUMBER, NULL, 0, c->g, c->desc->queue_number, gen_get_sampletime());
  e.d.number = number;

  if (c->desc->is_dst_gen)
    gen_post_aevent(&e);	/* send to c->g as dest */
  else
    gen_send_events(c->g, c->desc->queue_number, -1, &e);	/* send *from* c->g */
}

PUBLIC void control_update_names(Control *c) {
  g_return_if_fail(c != NULL);

  if( c->g != NULL )
      gtk_frame_set_label(GTK_FRAME(c->title_frame), c->g->name);
  else
      gtk_frame_set_label(GTK_FRAME(c->title_frame), c->this_panel->name );

  gtk_label_set_text(GTK_LABEL(c->title_label), c->name ? c->name : c->desc->name);
}

PUBLIC void control_update_range(Control *c) {
  GtkAdjustment *adj = NULL;

  switch (c->desc->kind) {
    case CONTROL_KIND_SLIDER: adj = gtk_slider_get_adjustment(GTK_SLIDER(c->widget)); break;
    case CONTROL_KIND_KNOB: adj = gtk_knob_get_adjustment(GTK_KNOB(c->widget)); break;
    default:
      break;
  }

  if (adj != NULL) {
    adj->lower = c->min;
    adj->upper = c->max;
    adj->step_increment = c->step;
    adj->page_increment = c->page;

    gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
  }
}

PUBLIC void control_update_value(Control *c) {
  if (c->desc->refresh != NULL)
    c->desc->refresh(c);
}

PRIVATE gboolean control_panel_delete_handler(GtkWidget *cp, GdkEvent *event) {
  hide_control_panel();
  return TRUE;
}

PUBLIC ControlPanel *control_panel_new( char *name, gboolean visible, Sheet *sheet ) {

  ControlPanel *panel = safe_malloc( sizeof( ControlPanel ) );
  panel->scrollwin = NULL; //gtk_scrolled_window_new(NULL, NULL);
  panel->name = safe_string_dup(name);

  panel->fixedwidget = gtk_fixed_new();
  //gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(panel->scrollwin), panel->fixedwidget);


  if( visible )
      control_panel_register_panel( panel, name, TRUE );
  else
      panel->visible = FALSE;

  panel->sheet = sheet;

  //gtk_widget_show(panel->scrollwin);
  gtk_widget_show(panel->fixedwidget);
  if (!GTK_WIDGET_REALIZED(panel->fixedwidget))
    gtk_widget_realize(panel->fixedwidget);
  return panel;
}

PUBLIC ControlPanel *control_panel_unpickle(ObjectStoreItem *item) {

    ControlPanel *cp;
    if( item == NULL )
	return NULL;

    cp = objectstore_get_object( item );
    if( cp == NULL ) {
	char *name = objectstore_item_get_string(item, "name", "Panel" );
	ObjectStoreItem *sitem = objectstore_item_get_object( item, "sheet" );
	//gboolean visible = objectstore_item_get_integer( item, "visible", TRUE );
	cp = control_panel_new( name, TRUE, NULL );
	objectstore_set_object(item, cp);
	cp->sheet = ( sitem == NULL ? NULL : sheet_unpickle( sitem ) );
    }

    return cp;
}

PUBLIC ObjectStoreItem *control_panel_pickle(ControlPanel *cp, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_get_item(db, cp);

  if (item == NULL) {
    item = objectstore_new_item(db, "ControlPanel", cp);
    if (cp->name)
	objectstore_item_set_string(item, "name", cp->name);
    if( cp->sheet )
	objectstore_item_set_object( item, "sheet", sheet_pickle( cp->sheet, db ) );

    objectstore_item_set_integer( item, "visible", cp->visible );
  }

  return item;
}

PUBLIC void init_control(void) {

  control_panel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(control_panel), "gAlan Control Panel");
  gtk_window_position(GTK_WINDOW(control_panel), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(control_panel), TRUE, TRUE, FALSE);
  gtk_window_set_wmclass(GTK_WINDOW(control_panel), "gAlan_controls", "gAlan");
  gtk_widget_set_usize(control_panel, 400, 300);
  gtk_signal_connect(GTK_OBJECT(control_panel), "delete_event",
		     GTK_SIGNAL_FUNC(control_panel_delete_handler), NULL);


  control_notebook = gtk_notebook_new();
  gtk_widget_show( control_notebook );
  gtk_container_add(GTK_CONTAINER(control_panel), control_notebook);
}

PUBLIC void show_control_panel(void) {
  gtk_widget_show(control_panel);
}

PUBLIC void hide_control_panel(void) {
  gtk_widget_hide(control_panel);
}

PUBLIC void reset_control_panel(void) {
  /* A NOOP currently.
   * When I get round to figuring out a good way of making newly-created controls
   * appear in an empty spot, this routine may become useful. */
}

PUBLIC void control_set_value(Control *c, gfloat value) {
  GtkAdjustment *adj;

  switch (c->desc->kind) {
    case CONTROL_KIND_SLIDER: adj = gtk_slider_get_adjustment(GTK_SLIDER(c->widget)); break;
    case CONTROL_KIND_KNOB: adj = gtk_knob_get_adjustment(GTK_KNOB(c->widget)); break;

    case CONTROL_KIND_TOGGLE:
      value = MAX(MIN(value, 1), 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c->widget), value >= 0.5);
      return;

    default:
      return;
  }

  if (adj != NULL) {
    adj->value = value;
    gtk_signal_emit_by_name(GTK_OBJECT(adj), "value_changed");
  }
}

PUBLIC void control_int32_updater(Control *c) {
  control_set_value(c, * (gint32 *) ((gpointer) c->g->data + (size_t) c->desc->refresh_data));
}

PUBLIC void control_double_updater(Control *c) {
  control_set_value(c, * (gdouble *) ((gpointer) c->g->data + (size_t) c->desc->refresh_data));
}
