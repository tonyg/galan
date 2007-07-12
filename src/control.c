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
#define GRANULARITY 1

PUBLIC GtkWidget *control_panel = NULL;
PRIVATE ControlPanel *global_panel = NULL;
PRIVATE GtkWidget *control_notebook = NULL;
PRIVATE GList *control_panels = NULL;
PRIVATE GThread *update_thread;
PRIVATE GAsyncQueue *update_queue;

PRIVATE char *pixmap_path = NULL;

PRIVATE void mylayout_sizerequest( GtkContainer *container, GtkRequisition *requisition ) {

    GList *list = gtk_container_get_children( container );
    GList *listX = list;

    requisition->width = 0;
    requisition->height = 0;
    

    for( listX=list; listX != NULL; listX = g_list_next( listX ) ) {

	GtkWidget *widget = listX->data;
	gint x,y,w,h;
	GtkRequisition req;

	gtk_container_child_get( container, widget, "x", &x, NULL ); 
	gtk_container_child_get( container, widget, "y", &y, NULL ); 

	gtk_widget_size_request( widget, &req );
	w=req.width;
	h=req.height;

	if( x+w > requisition->width ) requisition->width = x+w;
	if( y+h > requisition->height ) requisition->height = y+h;

    }
    gtk_layout_set_size( GTK_LAYOUT( container ), requisition->width, requisition->height );
}



PUBLIC void control_panel_register_panel( ControlPanel *panel, char *name, gboolean add_fixed) {

    panel->scrollwin = gtk_scrolled_window_new( NULL, NULL );
    if( add_fixed ) {
	//gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( panel->scrollwin ), panel->fixedwidget );
	gtk_container_add( GTK_CONTAINER( panel->scrollwin ), panel->fixedwidget );
	//gtk_layout_set_size( GTK_LAYOUT( panel->fixedwidget ), 1024, 2048 );
	gtk_layout_set_vadjustment( GTK_LAYOUT( panel->fixedwidget ),
		gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW( panel->scrollwin ) ) );
	gtk_layout_set_hadjustment( GTK_LAYOUT( panel->fixedwidget ),
		gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW( panel->scrollwin ) ) );

    }

    gtk_widget_show(panel->scrollwin);

    gtk_notebook_append_page( GTK_NOTEBOOK( control_notebook ), panel->scrollwin, gtk_label_new( name ) );
    gtk_container_check_resize( GTK_CONTAINER( panel->fixedwidget ) );
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

PUBLIC void update_panel_name( ControlPanel *panel ) {
    if( panel->visible )
	gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( control_notebook ), panel->scrollwin, panel->name );
    else
	control_update_names( panel->sheet->panel_control );
}

PUBLIC void control_moveto(Control *c, int x, int y) {
  x = floor((x + (GRANULARITY>>1)) / ((gdouble) GRANULARITY)) * GRANULARITY;
  y = floor((y + (GRANULARITY>>1)) / ((gdouble) GRANULARITY)) * GRANULARITY;

  if (x != c->x || y != c->y) {

    ControlPanel *panel = c->panel == NULL ? global_panel : c->panel;

    x = MAX(x, 0);
    y = MAX(y, 0);

    gtk_layout_move(GTK_LAYOUT(panel->fixedwidget),
		c->whole, x, y);

    if( c->move_callback )
	c->move_callback( c );

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

#if 0
PUBLIC void control_update_bg(Control *c) {
    GError *err = NULL;

    if( c->desc->kind != CONTROL_KIND_PANEL )
	return;


    if( c->testbg_active || c->this_panel->current_bg ) {

	GdkPixbuf *pb = NULL;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	if( c->testbg_active ) {
	    pb = gdk_pixbuf_new_from_file( PIXMAPDIRIFY( "galan-bg-ref.png" ), &err );
	} else {
	    if( c->this_panel->current_bg )
		pb = gdk_pixbuf_new_from_file( c->this_panel->current_bg, &err );
	}

	if( ! GTK_WIDGET_MAPPED( c->widget ) )
	    return;

	if( !pb ) {
	    popup_msgbox("Error Loading Pixmap", MSGBOX_OK, 120000, MSGBOX_OK,
		    "File not found, or file format error: %s",
		    c->this_panel->current_bg);
	    return;
	}

	gdk_pixbuf_render_pixmap_and_mask( pb, &pixmap, &mask, 100 );
	gdk_window_set_back_pixmap( GTK_LAYOUT(c->widget)->bin_window, pixmap, FALSE );

    } else {
	gtk_style_set_background(c->widget->style, GTK_LAYOUT(c->widget)->bin_window, GTK_STATE_NORMAL);
    }
}
#endif
PUBLIC void control_update_bg(Control *c) {
    GError *err = NULL;

    if( c->desc->kind != CONTROL_KIND_PANEL )
	return;


    if( c->testbg_active || (c->this_panel->bg_type != CONTROL_PANEL_BG_DEFAULT) ) {

	GdkPixbuf *pb = NULL;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	if( c->testbg_active ) {
	    pb = gdk_pixbuf_new_from_file( PIXMAPDIRIFY( "galan-bg-ref.png" ), &err );
	    gdk_pixbuf_render_pixmap_and_mask( pb, &pixmap, &mask, 100 );
	} else {
	    if( ! GTK_WIDGET_MAPPED( c->widget ) )
		return;
	    switch( c->this_panel->bg_type ) {
		case CONTROL_PANEL_BG_IMAGE:
		    if( c->this_panel->bg_image_name )
			pb = gdk_pixbuf_new_from_file( c->this_panel->bg_image_name, &err );
		    if( !pb ) {
			popup_msgbox("Error Loading Pixmap", MSGBOX_OK, 120000, MSGBOX_OK,
				"File not found, or file format error: %s",
				c->this_panel->bg_image_name);
			return;
		    }

		    gdk_pixbuf_render_pixmap_and_mask( pb, &pixmap, &mask, 100 );
		    break;
		case CONTROL_PANEL_BG_GRADIENT: 
		    {
			GdkWindow *win = GTK_LAYOUT(c->widget)->bin_window;
			gint w,h;
			gdk_drawable_get_size( GDK_DRAWABLE( win ), &w, &h );
			//pb = gdk_pixbuf_new( GDK_COLORSPACE_RGB, FALSE, 8, w, h );
			pixmap = gdk_pixmap_new( GDK_DRAWABLE( win ), w, h, -1 );
			if( gdk_drawable_get_colormap( GDK_DRAWABLE( win ) ) == NULL )
			    gdk_drawable_set_colormap( GDK_DRAWABLE( win ), gdk_colormap_get_system() );

			cairo_t *cr = gdk_cairo_create( GDK_DRAWABLE(pixmap) );
			if( !cr ) {
			    return;
			}
			//cairo_scale( cr, w, h );
			cairo_pattern_t *pat = cairo_pattern_create_linear( 0,0,0,h );
			cairo_pattern_add_color_stop_rgb( pat, 0,
				c->this_panel->color1.red/65536.0,
				c->this_panel->color1.green/65536.0,
				c->this_panel->color1.blue/65536.0 );
			cairo_pattern_add_color_stop_rgb( pat, 1,
				c->this_panel->color2.red/65536.0,
				c->this_panel->color2.green/65536.0,
				c->this_panel->color2.blue/65536.0 );
			cairo_set_source( cr, pat );
			cairo_rectangle( cr, 0,0,w,h );
			cairo_fill( cr );
			cairo_set_source_rgba( cr,
				c->this_panel->frame_color.red/65536.0,
				c->this_panel->frame_color.green/65536.0,
				c->this_panel->frame_color.blue/65536.0,
				c->this_panel->frame_alpha/65536.0);
			cairo_rectangle( cr, 0,0,w,h );
			cairo_set_line_width (cr, 8.0);
			cairo_stroke( cr );
			cairo_pattern_destroy( pat );
			cairo_destroy( cr );

			break;
		    }
		case CONTROL_PANEL_BG_DEFAULT:
		case CONTROL_PANEL_BG_COLOR:
		    break;
	    }
	}


	gdk_window_set_back_pixmap( GTK_LAYOUT(c->widget)->bin_window, pixmap, FALSE );
	gtk_widget_queue_draw( c->widget );

    } else {
	gtk_style_set_background(c->widget->style, GTK_LAYOUT(c->widget)->bin_window, GTK_STATE_NORMAL);
    }
}

#if 0
PRIVATE void change_bg_callback(GtkWidget *widget, GtkWidget *fs) {

  const char *newname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
  Control *c = gtk_object_get_user_data( GTK_OBJECT( fs ));

  if( c->desc->kind != CONTROL_KIND_PANEL )
      return;

  if( c->this_panel->bg_image_name )
      free( c->this_panel->bg_image_name );

  c->this_panel->bg_image_name = safe_string_dup( newname );

  control_update_bg( c ); 
  
  gtk_widget_destroy(fs);
}

PRIVATE void change_bg_handler(GtkWidget *widget, Control *c) {
  GtkWidget *fs = gtk_file_selection_new("Load Background");

  if (c->this_panel->bg_image_name != NULL)
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), c->this_panel->bg_image_name);

  gtk_object_set_user_data( GTK_OBJECT(fs), c );

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		     GTK_SIGNAL_FUNC(change_bg_callback), fs);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

  gtk_widget_show(fs);
}
#endif

PRIVATE void change_bg_handler(GtkWidget *widget, Control *c) {
    GtkWidget *dialog, *vbox2, *color1, *color2, *filename, *framecolor; //*type;

    dialog = gtk_dialog_new_with_buttons( "Panel Background Settings", GTK_WINDOW(control_panel), GTK_DIALOG_DESTROY_WITH_PARENT,
	    "Image", CONTROL_PANEL_BG_IMAGE, "Gradient", CONTROL_PANEL_BG_GRADIENT, "Default", CONTROL_PANEL_BG_DEFAULT, NULL );

    vbox2 = gtk_vbox_new( FALSE, 10 );
    color1 = g_object_new( GTK_TYPE_COLOR_BUTTON, "color", &(c->this_panel->color1), "title", "color1", "use-alpha", FALSE, NULL );
    color2 = g_object_new( GTK_TYPE_COLOR_BUTTON, "color", &(c->this_panel->color2), "title", "color2", "use-alpha", FALSE, NULL );
    framecolor = g_object_new( GTK_TYPE_COLOR_BUTTON, 
	    "color", &(c->this_panel->frame_color), "title", "frame color", "use-alpha", TRUE, "alpha", c->this_panel->frame_alpha, NULL );

    filename = gtk_file_chooser_button_new( "Background Image", GTK_FILE_CHOOSER_ACTION_OPEN );
    

    if( c->this_panel->bg_image_name ) {
	if( ! g_path_is_absolute( c->this_panel->bg_image_name ) ) {
	    gchar *current_dir = g_get_current_dir();
	    gchar *abspath     = g_build_filename( current_dir, c->this_panel->bg_image_name, NULL );
	    g_free(  c->this_panel->bg_image_name );
	    c->this_panel->bg_image_name = abspath;
	    g_free( current_dir );
	}
	
	gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(filename), c->this_panel->bg_image_name );
    } else {
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(filename), pixmap_path);
    }

    gtk_container_add( GTK_CONTAINER( vbox2 ), color1 );
    gtk_container_add( GTK_CONTAINER( vbox2 ), color2 );
    gtk_container_add( GTK_CONTAINER( vbox2 ), framecolor );
    gtk_container_add( GTK_CONTAINER( vbox2 ), filename );
    
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog )->vbox ), vbox2 );
    gtk_widget_show_all( dialog );

    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );
    if( response != GTK_RESPONSE_DELETE_EVENT ) {
	gtk_color_button_get_color( GTK_COLOR_BUTTON(color1), &(c->this_panel->color1) );
	gtk_color_button_get_color( GTK_COLOR_BUTTON(color2), &(c->this_panel->color2) );
	gtk_color_button_get_color( GTK_COLOR_BUTTON(framecolor), &(c->this_panel->frame_color) );
	c->this_panel->frame_alpha = gtk_color_button_get_alpha( GTK_COLOR_BUTTON(framecolor) );

	if( c->this_panel->bg_image_name ) free( c->this_panel->bg_image_name );
	c->this_panel->bg_image_name = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(filename) );
	c->this_panel->bg_type = response;
    }

    
    gtk_widget_destroy( dialog );

    control_update_bg( c );
    
    

    
}


PRIVATE GtkWidget *ctrl_rename_text_widget = NULL;

PRIVATE void ctrl_rename_handler(MsgBoxResponse action_taken, Control *c) {
  if (action_taken == MSGBOX_OK) {
    const char *newname = gtk_entry_get_text(GTK_ENTRY(ctrl_rename_text_widget));

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

PRIVATE void control_visible_ctrl_handler(GtkWidget *widget, Control *c) {

    c->control_visible = !(c->control_visible);

    if( c->control_visible )
	gtk_widget_show( c->widget );
    else
	gtk_widget_hide( c->widget );

    gtk_widget_queue_resize( c->whole );
}

PRIVATE void control_panel_test_bg_ctrl_handler(GtkWidget *widget, Control *c) {

    c->testbg_active = !(c->testbg_active);

    control_update_bg( c  );
}

PRIVATE void entry_visible_ctrl_handler(GtkWidget *widget, Control *c) {

    c->entry_visible = !(c->entry_visible);

    if( c->entry_visible )
	gtk_widget_show( c->entry );
    else
	gtk_widget_hide( c->entry );

    gtk_widget_queue_resize( c->whole );
}

PRIVATE void frame_visible_ctrl_handler(GtkWidget *widget, Control *c) {
    c->frame_visible = !(c->frame_visible);

    if( ! c->frame_visible ){
        gtk_frame_set_shadow_type (GTK_FRAME (c->title_frame) , GTK_SHADOW_NONE);
        gtk_frame_set_label (GTK_FRAME (c->title_frame) , NULL);
    }else{
        gtk_frame_set_shadow_type (GTK_FRAME (c->title_frame) , GTK_SHADOW_ETCHED_IN);
        gtk_label_set_text(GTK_LABEL(c->title_label),c->desc->name);
    }
    gtk_widget_queue_resize( c->whole );
}

PRIVATE void name_visible_ctrl_handler(GtkWidget *widget, Control *c) {
    c->name_visible = !(c->name_visible);

    if( ! c->name_visible ){

        /* In order for this to work, you need the pixmap to bring up the menu...
         * gtk_widget_hide( c->title_label );
         * the following is a temp hack.
         */
        gtk_label_set_text(GTK_LABEL(c->title_label),"    ");

    }else{
        /* In order for this to work, you need the pixmap to bring up the menu...
         * gtk_widget_show( c->title_label );
         * the following is a temp hack
         */
        control_update_names(c);
    }
    gtk_widget_queue_resize( c->whole );
}
PRIVATE void control_panel_sizer_visible_ctrl_handler(GtkWidget *widget, Control *c) {

    c->this_panel->sizer_visible = !(c->this_panel->sizer_visible);

    if( c->this_panel->sizer_visible ) {
	gtk_widget_show( c->this_panel->sizer_image );
	gtk_layout_move( GTK_LAYOUT(c->this_panel->fixedwidget), c->this_panel->sizer_ebox,
		c->this_panel->sizer_x, c->this_panel->sizer_y );
    }

    else
    {
	gtk_widget_hide( c->this_panel->sizer_image );
	gtk_layout_move( GTK_LAYOUT(c->this_panel->fixedwidget), c->this_panel->sizer_ebox,
		c->this_panel->sizer_x + 16, c->this_panel->sizer_y + 16 );
    }
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

  /*
  item = gtk_check_menu_item_new_with_label( "Folded" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->folded );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(fold_ctrl_handler), c);

  item = gtk_check_menu_item_new_with_label( "Discreet" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->discreet );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(discreet_ctrl_handler), c);
  */

  item = gtk_check_menu_item_new_with_label( "Frame" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->frame_visible );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(frame_visible_ctrl_handler), c);

  item = gtk_check_menu_item_new_with_label( "Name" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->name_visible );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(name_visible_ctrl_handler), c);

  if( c->entry ) {
      item = gtk_check_menu_item_new_with_label( "Entry" );
      gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->entry_visible );
      gtk_widget_show(item);
      gtk_menu_append(GTK_MENU(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(entry_visible_ctrl_handler), c);
  }

  item = gtk_check_menu_item_new_with_label( "Control" );
  gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->control_visible );
  gtk_widget_show(item);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(control_visible_ctrl_handler), c);

  if( c->desc->kind == CONTROL_KIND_PANEL ) {
      item = gtk_menu_item_new_with_label("Set Background");
      gtk_widget_show(item);
      gtk_menu_append(GTK_MENU(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(change_bg_handler), c);

      item = gtk_check_menu_item_new_with_label( "Reference Background" );
      gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->testbg_active );
      gtk_widget_show(item);
      gtk_menu_append(GTK_MENU(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(control_panel_test_bg_ctrl_handler), c);

      item = gtk_check_menu_item_new_with_label( "Sizer" );
      gtk_check_menu_item_set_state( GTK_CHECK_MENU_ITEM( item ), c->this_panel->sizer_visible );
      gtk_widget_show(item);
      gtk_menu_append(GTK_MENU(menu), item);
      gtk_signal_connect(GTK_OBJECT(item), "toggled", GTK_SIGNAL_FUNC(control_panel_sizer_visible_ctrl_handler), c);

  }

  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		 be->button, be->time);

  g_object_ref( menu );
  old_popup_menu = menu;
}

PRIVATE gboolean eventbox_handler(GtkWidget *eventbox, GdkEvent *event, Control *c) {

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

      gtk_widget_queue_draw( eventbox );

      return TRUE;
    }

    default:
      return FALSE;
  }
}

PRIVATE gboolean control_map_handler(GtkWidget *eventbox, Control *c) {

    gtk_widget_queue_resize( c->whole );
    control_update_bg( c );

    return FALSE;
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

  c->frame_visible = TRUE;
  c->name_visible = TRUE;
  c->entry_visible = TRUE;
  c->control_visible = TRUE;
  c->testbg_active = FALSE;

  if( panel == NULL && global_panel == NULL )
      global_panel = control_panel_new( "Global", TRUE, NULL );

  c->panel = panel;

  c->moving = c->saved_x = c->saved_y = 0;
  c->x = 0;
  c->y = 0;
  c->events_flow = TRUE;
  c->kill_me = FALSE;
  c->update_refcount = 0;

  c->whole = NULL;
  c->g = g;
  c->data = NULL;

  c->move_callback = NULL;

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
      c->entry = gtk_entry_new();
      gtk_widget_set_usize(c->entry, GAUGE_SIZE, 0);
      gtk_widget_show(c->entry);

      gtk_box_pack_start(GTK_BOX(vbox), c->entry, FALSE, FALSE, 0);

      gtk_signal_connect(GTK_OBJECT(c->entry), "activate",
			 GTK_SIGNAL_FUNC(entry_changed), adj);
      gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
			 GTK_SIGNAL_FUNC(update_entry), c->entry);
    } else {
    	c->entry = NULL;
    }
    
    c->whole = gtk_event_box_new();

    gtk_signal_connect_after(GTK_OBJECT(c->whole), "map", GTK_SIGNAL_FUNC(control_map_handler), c);

    g_object_ref( G_OBJECT(c->whole) );
    g_object_set_data( G_OBJECT(c->whole), "Control", c );
    gtk_container_add(GTK_CONTAINER(c->whole), c->title_frame );
    gtk_widget_show( c->whole );

    gtk_layout_put(GTK_LAYOUT(panel == NULL ? global_panel->fixedwidget : panel->fixedwidget),
	    c->whole, c->x, c->y);

    g_object_ref( G_OBJECT( panel == NULL ? global_panel->fixedwidget : panel->fixedwidget ) );

    if (!GTK_WIDGET_REALIZED(eventbox))
      gtk_widget_realize(eventbox);

    gdk_window_set_events(eventbox->window, 
	    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK  );
  }

  if( c->desc->kind != CONTROL_KIND_PANEL ) {
      gen_register_control(c->g, c);
      gen_update_controls( c->g, -1 );
  }


  return c;
}

/**
 * \brief Kill a Control
 *
 * \param c The Control to be removed
 *
 * with the gdk_threads_enter/leave pair i need to assure this is a different thread.
 * it could be safe to call check for kill Control from the update processor.
 *
 * i will put gdk_threads enter leave outside of this function.
 * So make sure you hold the gdk-lock when you call this function.
 */

PUBLIC void control_kill_control(Control *c) {
  g_return_if_fail(c != NULL);

  if (c->g != NULL)
    gen_deregister_control(c->g, c);

  //gdk_threads_enter();
  if( c->desc->destroy != NULL )
      c->desc->destroy( c );
  //gtk_widget_hide(c->whole);
  gtk_container_remove(GTK_CONTAINER(c->panel == NULL ? global_panel->fixedwidget : c->panel->fixedwidget), c->whole);
  g_object_unref( G_OBJECT(c->whole) );
  g_object_unref( G_OBJECT(c->panel == NULL ? global_panel->fixedwidget : c->panel->fixedwidget) );

  if (c->name != NULL)
    safe_free(c->name);

  //gdk_threads_leave();
  //



  safe_free(c);
  // FIXING a race condition with the updater_thread. let the updater delete the control.
  //c->kill_me = TRUE;
  //control_update_value( c );
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

    control->this_panel->sheet->panel_control_active = FALSE;
    control->this_panel->sheet->panel_control = NULL;

    control_panel_register_panel( control->this_panel, control->this_panel->name, FALSE );

    g_object_ref( G_OBJECT(control->this_panel->fixedwidget) );
    gtk_widget_reparent( control->this_panel->fixedwidget, control->this_panel->scrollwin );
    g_object_unref( G_OBJECT(control->this_panel->fixedwidget) );

    gtk_layout_set_vadjustment( GTK_LAYOUT( control->this_panel->fixedwidget ),
	    gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW( control->this_panel->scrollwin ) ) );
    gtk_layout_set_hadjustment( GTK_LAYOUT( control->this_panel->fixedwidget ),
	    gtk_scrolled_window_get_hadjustment( GTK_SCROLLED_WINDOW( control->this_panel->scrollwin ) ) );

    gtk_widget_show( control->this_panel->fixedwidget );
    gtk_widget_queue_resize( control->this_panel->fixedwidget );
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
  int folded, discreet;

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

  folded = objectstore_item_get_integer(item, "folded", 0);
  discreet = objectstore_item_get_integer(item, "discreet", 0);

  if( ! (c->frame_visible = objectstore_item_get_integer( item, "frame_visible", ! discreet ) ) ) {
        gtk_frame_set_shadow_type (GTK_FRAME (c->title_frame) , GTK_SHADOW_NONE);
        gtk_frame_set_label (GTK_FRAME (c->title_frame) , NULL);
        //gtk_label_set_text(GTK_LABEL(c->title_label),"    ");
  }

  if( ! (c->name_visible = objectstore_item_get_integer( item, "name_visible", c->frame_visible ) ) ) {
        gtk_label_set_text(GTK_LABEL(c->title_label),"    ");
  }

  if( ! (c->entry_visible = objectstore_item_get_integer( item, "entry_visible", ! discreet ) ) ) {
	if( c->entry )
		gtk_widget_hide( c->entry );
  }

  if( ! (c->control_visible = objectstore_item_get_integer( item, "control_visible", ! folded ) ) ) {
        gtk_widget_hide( c->widget );
  }



  // XXX: always or default ?
  if( c->this_panel && c->this_panel->bg_image_name ) {
      
      
      control_update_bg( c  );
  }

  x = objectstore_item_get_integer(item, "x_coord", 0);
  y = objectstore_item_get_integer(item, "y_coord", 0);
  control_moveto(c, x, y);
  c->events_flow = TRUE;
  c->kill_me = FALSE;
  c->update_refcount = 0;

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
//  objectstore_item_set_integer(item, "folded", c->folded);
//  objectstore_item_set_integer(item, "discreet", c->discreet);
  objectstore_item_set_integer(item, "control_visible", c->control_visible);
  objectstore_item_set_integer(item, "frame_visible", c->frame_visible);
  objectstore_item_set_integer(item, "name_visible", c->name_visible);
  objectstore_item_set_integer(item, "entry_visible", c->entry_visible);


  /* don't save c->data in any form, Controls are MVC views, not models. */
  return item;
}

Control *control_clone( Control *c, Generator *g, ControlPanel *cp ) {

    Control *retval =  control_new_control( c->desc, g, cp );

  retval->name = c->name ? safe_string_dup(c->name) : NULL;
  if (retval->name)
    control_update_names(retval);


  retval->frame_visible = c->frame_visible;
  if( ! (c->frame_visible) ) {
        gtk_frame_set_shadow_type (GTK_FRAME (retval->title_frame) , GTK_SHADOW_NONE);
        gtk_frame_set_label (GTK_FRAME (retval->title_frame) , NULL);
        gtk_label_set_text(GTK_LABEL(retval->title_label),"    ");
  }

  retval->entry_visible = c->entry_visible;
  if( ! (c->entry_visible) ) {
	if( retval->entry )
		gtk_widget_hide( retval->entry );
  }

  retval->control_visible = c->control_visible;
  if( ! (c->control_visible) ) {
        gtk_widget_hide( retval->widget );
  }
  retval->min = c->min;
  retval->max = c->max;
  retval->step = c->step;
  retval->page = c->page;

  control_moveto( retval, c->x, c->y );

  return retval;
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

  if( c->frame_visible ) {

      if( c->g != NULL )
          gtk_frame_set_label(GTK_FRAME(c->title_frame), c->g->name);
      else
          gtk_frame_set_label(GTK_FRAME(c->title_frame), c->this_panel->name );
  }

  if( c->name_visible ) {
      gtk_label_set_text(GTK_LABEL(c->title_label), c->name ? c->name : c->desc->name);
  }
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
    //c->update_refcount++;
    g_async_queue_push( update_queue, c );
}

PRIVATE  void control_update_value_real(Control *c) {
//    if( c->kill_me ) {
//	if( c->update_refcount > 0 ) {
//	    printf( "update_refcount = %d\n", c->update_refcount );
//	} else {
//	    safe_free( c );
//	}
//  } else {

	c->events_flow = FALSE;	/* as already stated... not very elegant. */

	if (c->desc->refresh != NULL)
	    c->desc->refresh(c);

	c->events_flow = TRUE;

//    }
}

PRIVATE gboolean control_panel_delete_handler(GtkWidget *cp, GdkEvent *event) {
  hide_control_panel();
  return TRUE;
}


PRIVATE gboolean sizerbox_handler(GtkWidget *eventbox, GdkEvent *event, ControlPanel *panel) {

    switch (event->type) {
	case GDK_BUTTON_PRESS: 
	    {
		GdkEventButton *be = (GdkEventButton *) event;

		switch (be->button) {
		    case 1:
			if (!panel->sizer_moving) {
			    panel->sizer_moving = 1;
			    gtk_grab_add(eventbox);
			    panel->sizer_saved_x = panel->sizer_x - be->x_root;
			    panel->sizer_saved_y = panel->sizer_y - be->y_root;
			}
			break;
		}
		return TRUE;
	    }

	case GDK_BUTTON_RELEASE:
	    {
		if (panel->sizer_moving) {
		    panel->sizer_moving = 0;
		    gtk_grab_remove(eventbox);
		}
		return TRUE;
	    }

	case GDK_MOTION_NOTIFY:
	    {
		GdkEventMotion *me = (GdkEventMotion *) event;

		if (panel->sizer_moving) {
		    gtk_layout_move( GTK_LAYOUT(panel->fixedwidget), eventbox,
			    panel->sizer_saved_x + me->x_root,
			    panel->sizer_saved_y + me->y_root );

		    panel->sizer_x = panel->sizer_saved_x + me->x_root;
		    panel->sizer_y = panel->sizer_saved_y + me->y_root;
		}

		return TRUE;
	    }

	default:
	    return FALSE;
    }
}

PRIVATE void control_invoke_move_callback( GtkWidget *control_widget, gpointer user_data ) {
    Control *c = g_object_get_data( G_OBJECT(control_widget), "Control" );
    if( c && c->move_callback )
	c->move_callback( c );
}

PRIVATE void control_panel_scroll_handler( GtkAdjustment *adjustment, ControlPanel *cp ) {
    gtk_container_foreach( GTK_CONTAINER(cp->fixedwidget), control_invoke_move_callback, NULL );
}

PUBLIC ControlPanel *control_panel_new( char *name, gboolean visible, Sheet *sheet ) {

    ControlPanel *panel = safe_malloc( sizeof( ControlPanel ) );
    panel->scrollwin = NULL; //gtk_scrolled_window_new(NULL, NULL);
    panel->name = safe_string_dup(name);

    panel->fixedwidget = gtk_layout_new(NULL,NULL);


    panel->w = 0;
    panel->h = 0;

    panel->sizer_x = 0;
    panel->sizer_y = 0;
    panel->sizer_moving = 0;
    panel->sizer_visible = 0;
    panel->bg_image_name = NULL;
	gdk_color_white( gdk_colormap_get_system(), &(panel->color1) );
	gdk_color_black( gdk_colormap_get_system(), &(panel->color2) );

    g_signal_connect( G_OBJECT( panel->fixedwidget ), "size_request", G_CALLBACK(mylayout_sizerequest), NULL );



    if( visible )
	control_panel_register_panel( panel, name, TRUE );
    else
	panel->visible = FALSE;

    g_signal_connect_after( gtk_layout_get_hadjustment( GTK_LAYOUT( panel->fixedwidget ) ),
	    "value-changed", (GCallback) control_panel_scroll_handler, panel );
    g_signal_connect_after( gtk_layout_get_vadjustment( GTK_LAYOUT( panel->fixedwidget ) ),
	    "value-changed", (GCallback) control_panel_scroll_handler, panel );
	    
    panel->sheet = sheet;

    gtk_widget_show(panel->fixedwidget);

//    if (!GTK_WIDGET_REALIZED(panel->fixedwidget))
//	gtk_widget_realize(panel->fixedwidget);

    gtk_container_check_resize( GTK_CONTAINER(panel->fixedwidget) );
    update_panel_name( panel );

    // code for the sizer.

    panel->sizer_image = gtk_image_new_from_file( PIXMAPDIRIFY( "corner-widget.png" ) );
    panel->sizer_ebox = gtk_event_box_new();

    gtk_container_add( GTK_CONTAINER( panel->sizer_ebox ), panel->sizer_image );
    
    gtk_layout_put( GTK_LAYOUT( panel->fixedwidget ), panel->sizer_ebox, 0, 0 );
    
    gtk_widget_show( panel->sizer_ebox );

    gtk_signal_connect(GTK_OBJECT(panel->sizer_ebox), "event", GTK_SIGNAL_FUNC(sizerbox_handler), panel);


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
	cp->sizer_x = objectstore_item_get_integer( item, "sizer_x", 0 );
	cp->sizer_y = objectstore_item_get_integer( item, "sizer_y", 0 );
	cp->sheet = ( sitem == NULL ? NULL : sheet_unpickle( sitem ) );
	cp->bg_type = CONTROL_PANEL_BG_DEFAULT;
	cp->bg_image_name = objectstore_item_get_string( item, "current_bg", NULL );
	if( cp->bg_image_name ) {
	    if( g_file_test( cp->bg_image_name, G_FILE_TEST_EXISTS ) ) {
		cp->bg_image_name = safe_string_dup( cp->bg_image_name );
		cp->bg_type = CONTROL_PANEL_BG_IMAGE;
	    } else {
		char *bg_basename = g_path_get_basename( cp->bg_image_name );
		char *bg_in_pixmap_dir = g_build_filename( pixmap_path, bg_basename, NULL );

		if( g_file_test( bg_in_pixmap_dir, G_FILE_TEST_EXISTS ) ) {
		    cp->bg_image_name = bg_in_pixmap_dir;
		    cp->bg_type = CONTROL_PANEL_BG_IMAGE;
		} else {
		    cp->bg_image_name = NULL;
		    g_free( bg_in_pixmap_dir );
		}

		g_free( bg_basename );
	    }
	}
	cp->color1.red   = objectstore_item_get_integer( item, "color1_red", 65535 );
	cp->color1.green = objectstore_item_get_integer( item, "color1_green", 65535 );
	cp->color1.blue  = objectstore_item_get_integer( item, "color1_blue", 65535 );
	cp->color2.red   = objectstore_item_get_integer( item, "color2_red", 0 );
	cp->color2.green = objectstore_item_get_integer( item, "color2_green", 0 );
	cp->color2.blue  = objectstore_item_get_integer( item, "color2_blue", 0 );
	cp->frame_color.red   = objectstore_item_get_integer( item, "frame_color_red", 0 );
	cp->frame_color.green = objectstore_item_get_integer( item, "frame_color_green", 0 );
	cp->frame_color.blue  = objectstore_item_get_integer( item, "frame_color_blue", 0 );
	cp->frame_alpha       = objectstore_item_get_integer( item, "frame_alpha", 0 );
	cp->bg_type      = objectstore_item_get_integer( item, "bg_type", cp->bg_type );

	gtk_layout_move( GTK_LAYOUT(cp->fixedwidget), cp->sizer_ebox,
		cp->sizer_x + 16, cp->sizer_y + 16 );
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

  if ( cp->bg_image_name )
      objectstore_item_set_string( item, "current_bg", cp->bg_image_name );
    objectstore_item_set_integer( item, "visible", cp->visible );
    objectstore_item_set_integer( item, "sizer_x", cp->sizer_x );
    objectstore_item_set_integer( item, "sizer_y", cp->sizer_y );

     objectstore_item_set_integer( item, "color1_red", cp->color1.red );
     objectstore_item_set_integer( item, "color1_green", cp->color1.green );
     objectstore_item_set_integer( item, "color1_blue", cp->color1.blue );
     objectstore_item_set_integer( item, "color2_red", cp->color2.red );
     objectstore_item_set_integer( item, "color2_green", cp->color2.green );
     objectstore_item_set_integer( item, "color2_blue", cp->color2.blue );
     objectstore_item_set_integer( item, "frame_color_red", cp->frame_color.red );
     objectstore_item_set_integer( item, "frame_color_green", cp->frame_color.green );
     objectstore_item_set_integer( item, "frame_color_blue", cp->frame_color.blue );
     objectstore_item_set_integer( item, "frame_alpha", cp->frame_alpha );
     objectstore_item_set_integer( item, "bg_type", cp->bg_type );
  }

  return item;
}

PRIVATE gpointer update_processor( gpointer data ) {
    while( 1 ) {
	Control *c = g_async_queue_pop( update_queue );
	gdk_threads_enter();
	//c->update_refcount--;
	control_update_value_real( c );
	gdk_threads_leave();
    }
    return NULL;
}

PUBLIC void init_control_thread(void) {
  GError *err;
  update_thread = g_thread_create( update_processor, NULL, TRUE, &err );
}

PUBLIC void init_control(void) {

  pixmap_path = getenv("GALAN_PIXMAP_PATH");
  if( ! pixmap_path )
      pixmap_path = SITE_PKGDATA_DIR G_DIR_SEPARATOR_S "pixmaps";

  if( ! g_path_is_absolute( pixmap_path ) ) {
      gchar *current_dir = g_get_current_dir();
      gchar *abspath     = g_build_filename( current_dir, pixmap_path, NULL );
      //g_free(  pixmap_path );
      pixmap_path = abspath;
      g_free( current_dir );
  }

  update_queue = g_async_queue_new();
  
  control_panel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(control_panel), "gAlan Control Panel");
  gtk_window_position(GTK_WINDOW(control_panel), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(control_panel), TRUE, TRUE, FALSE);
  gtk_window_set_wmclass(GTK_WINDOW(control_panel), "gAlan_controls", "gAlan");
  gtk_widget_set_usize(control_panel, 400, 300);
  gtk_widget_set_name( control_panel, "control_panel" );
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
    //adj->value = value;
    //gtk_signal_emit_by_name(GTK_OBJECT(adj), "value_changed");
    gtk_adjustment_set_value( GTK_ADJUSTMENT( adj ), value );
  }
}

PUBLIC void control_int32_updater(Control *c) {
  control_set_value(c, * (gint32 *) ((gpointer) c->g->data + (size_t) c->desc->refresh_data));
}

PUBLIC void control_double_updater(Control *c) {
  control_set_value(c, * (gdouble *) ((gpointer) c->g->data + (size_t) c->desc->refresh_data));
}
