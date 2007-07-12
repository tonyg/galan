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

/*
 *
 * this is gencomp.c from tony. I modifie it to be shcomp.c
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "sheet.h"
#include "msgbox.h"
#include "control.h"
#include "gencomp.h"
#include "shcomp.h"
#include "iscomp.h"
#include "gui.h"

/* %%% Win32: dirent.h seems to conflict with glib-1.3, so ignore dirent.h */
#ifndef G_PLATFORM_WIN32
#include <dirent.h>
#endif

/* On Win32, these headers seem to need to follow glib.h */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHCOMP_ICONLENGTH	48
#define SHCOMP_TITLEHEIGHT	15
#define SHCOMP_CONNECTOR_SPACE	5
#define SHCOMP_CONNECTOR_WIDTH	10
#define SHCOMP_BORDER_WIDTH	(SHCOMP_CONNECTOR_WIDTH + SHCOMP_CONNECTOR_SPACE)


//PRIVATE int next_component_number = 1;
//PRIVATE ComponentClass InterSheetComponentClass;	/* forward reference */
PRIVATE ComponentClass SheetComponentClass;
PRIVATE void do_control(Component *c, guint action, GtkWidget *widget);

PRIVATE void build_connectors(Component *c, int count, gboolean is_outbound, gboolean is_signal) {
  int i;

  for (i = 0; i < count; i++)
    comp_new_connector(c, is_signal? COMP_SIGNAL_CONNECTOR : COMP_EVENT_CONNECTOR,
		       is_outbound, i,
		       0, 0);
}

PRIVATE void resize_connectors(Component *c, int count,
			       gboolean is_outbound, gboolean is_signal,
			       int hsize, int vsize) {
  int spacing = (is_signal ? vsize : hsize) / (count + 1);
  int startpos = is_outbound ? (SHCOMP_BORDER_WIDTH * 2
				+ (is_signal ? hsize : vsize)
				- (SHCOMP_CONNECTOR_WIDTH >> 1))
			     : (SHCOMP_CONNECTOR_WIDTH >> 1);
  int x = is_signal ? startpos : SHCOMP_BORDER_WIDTH + spacing;
  int y = is_signal ? SHCOMP_BORDER_WIDTH + spacing : startpos;
  int dx = is_signal ? 0 : spacing;
  int dy = is_signal ? spacing : 0;
  int i;

  for (i = 0; i < count; i++, x += dx, y += dy) {
    ConnectorReference ref = { c, is_signal ? COMP_SIGNAL_CONNECTOR : COMP_EVENT_CONNECTOR,
			       is_outbound, i };
    Connector *con = comp_get_connector(&ref);

    con->x = x;
    con->y = y;
  }
}

PUBLIC void shcomp_resize(Component *c) {
  int body_vert, body_horiz;
  ShCompData *data = c->data;

  gboolean icon=FALSE;
  

  body_vert =
    SHCOMP_CONNECTOR_WIDTH
    + MAX(MAX(data->isl.anzinputsignals, data->isl.anzoutputsignals) * SHCOMP_CONNECTOR_WIDTH,
	  SHCOMP_TITLEHEIGHT + (icon ? SHCOMP_ICONLENGTH : 0));
  body_horiz =
    SHCOMP_CONNECTOR_WIDTH
    + MAX(2,
	  MAX(sheet_get_textwidth(c->sheet, data->sheet->name),
	      MAX(data->isl.anzinputevents * SHCOMP_CONNECTOR_WIDTH,
		  data->isl.anzoutputevents * SHCOMP_CONNECTOR_WIDTH)));

  resize_connectors(c, data->isl.anzinputevents, 0, 0, body_horiz, body_vert);
  resize_connectors(c, data->isl.anzinputsignals, 0, 1, body_horiz, body_vert);
  resize_connectors(c, data->isl.anzoutputevents, 1, 0, body_horiz, body_vert);
  resize_connectors(c, data->isl.anzoutputsignals, 1, 1, body_horiz, body_vert);

  c->width = body_horiz + 2 * SHCOMP_BORDER_WIDTH + 1;
  c->height = body_vert + 2 * SHCOMP_BORDER_WIDTH + 1;
}

/**
 * make InterSheetLinks ->  list<list< Components >>.
 * but why ? 
 * what are the components for ?
 */

PRIVATE InterSheetLinks *find_intersheet_links( Sheet *sheet ) {

    gboolean warned = FALSE;
    GList *lst = sheet->components;
    InterSheetLinks *isl = safe_malloc( sizeof(InterSheetLinks) );
    isl->inputevents   = NULL;
    isl->outputevents  = NULL;
    isl->inputsignals  = NULL;
    isl->outputsignals = NULL;

    isl->anzinputevents   = 0;
    isl->anzoutputevents  = 0;
    isl->anzinputsignals  = 0;
    isl->anzoutputsignals = 0;

    while (lst != NULL) {
	Component *c = lst->data;

	if ( ! strcmp( c->klass->class_tag, "iscomp" ) ) {
	    
	    ISCompData *d=c->data;
	    if( d->ref != NULL )
	    {	
		switch( ((ISCompData *) c->data)->reftype ) {
		    case SIGIN:
			isl->outputsignals =  g_list_append(isl->outputsignals, c );
			isl->anzoutputsignals++;
			break;
		    case SIGOUT:
			isl->inputsignals =  g_list_append(isl->inputsignals, c );
			isl->anzinputsignals++;
			break;
		    case EVTIN:
			isl->outputevents =  g_list_append(isl->outputevents, c );
			isl->anzoutputevents++;
			break;
		    case EVTOUT:
			isl->inputevents =  g_list_append(isl->inputevents, c );
			isl->anzinputevents++;
			break;
		}
	    } else {
		if( !warned ) {
		    warned = TRUE;
		    popup_msgbox("Warning", MSGBOX_OK, 120000, MSGBOX_OK,
			    "Unconnected Intersheet Component on Sheet %s..", sheet->name );
		}
	    }
	}
	lst = g_list_next(lst);
    }
    return isl;
}


/**
 * 
 * There must be a different init_data for the load from file
 * library components... 
 *
 * ansonsten ist das aber das selbe hier mit dem ganzen Kram...
 * ich habe dann zwei component klassen, die sich nur durch
 * den konstruktor unterscheiden... sp"ater aber auch durch
 * das pickle unpickle geraffel... obwohl
 * das probleme gibt, wegen den generator links :(
 *
 * erster Versuch ist 
 *
 * konstruktor mit load file, register sheet, aufruf von shcomp_init
 *
 * ansonsten das selbe...
 *
 * TODO: shared component code in eigenes Teil packen...
 *
 */

PRIVATE int shcomp_initialize(Component *c, gpointer init_data) {

  ShCompData *d = safe_malloc(sizeof(ShCompData));
  ShCompInitData *id = (ShCompInitData *) init_data;

  InterSheetLinks *isl = find_intersheet_links( id->sheet );

  d->sheet = id->sheet;

  sheet_register_ref( id->sheet, c );
  
  d->isl = *isl;

  //d->panel_control_active = FALSE;

  build_connectors(c, d->isl.anzinputevents, 0, 0);
  build_connectors(c, d->isl.anzinputsignals, 0, 1);
  build_connectors(c, d->isl.anzoutputevents, 1, 0);
  build_connectors(c, d->isl.anzoutputsignals, 1, 1);

  c->x -= SHCOMP_BORDER_WIDTH;
  c->y -= SHCOMP_BORDER_WIDTH;
  c->width = c->height = 0;
  c->data = d;


  shcomp_resize(c);

  return 1;
}

PRIVATE Component *shcomp_clone(Component *c, Sheet *sheet) {
  ShCompData *d = c->data;

  Sheet *newsheet = sheet_clone( d->sheet );
 
  ShCompInitData id;
  Component *retval;
  
  id.sheet = newsheet;
  retval = comp_new_component( &SheetComponentClass, &id, sheet, 0, 0 );

  if( d->sheet->panel_control_active ) {

      do_control( retval, 0, NULL );
      newsheet->panel_control->frame_visible = d->sheet->panel_control->frame_visible;
      
      if( ! (newsheet->panel_control->frame_visible) ) {
	  gtk_frame_set_shadow_type (GTK_FRAME (newsheet->panel_control->title_frame) , GTK_SHADOW_NONE);
	  gtk_frame_set_label (GTK_FRAME (newsheet->panel_control->title_frame) , NULL);
	  gtk_label_set_text(GTK_LABEL(newsheet->panel_control->title_label),"    ");
      }

      newsheet->panel_control->control_visible = d->sheet->panel_control->control_visible;

      if( ! (newsheet->panel_control->control_visible) ) {
	  gtk_widget_hide( newsheet->panel_control->widget );
      }
      
      control_moveto( newsheet->panel_control, d->sheet->panel_control->x, d->sheet->panel_control->y );
  }
  //shcomp_resize( retval );

  return retval;
}

PRIVATE int fileshcomp_initialize(Component *c, gpointer init_data) {
  FileShCompInitData *id = (FileShCompInitData *) init_data;
  int retval;

  ShCompInitData *shcid = safe_malloc( sizeof( ShCompInitData ) );
  //printf( "hi %s\n", id->filename );
  FILE *f = fopen( id->filename, "rb" );

  sheet_set_load_hidden( TRUE );
  shcid->sheet = sheet_loadfrom( NULL, f );
  sheet_set_load_hidden( FALSE );
  fclose( f );
  retval =  shcomp_initialize( c, shcid );
  free(shcid);
  return retval;
}

PRIVATE void shcomp_destroy(Component *c) {
  ShCompData *d = c->data;

  sheet_unregister_ref( d->sheet, c );

  if( !sheet_has_refs( d->sheet ) && d->sheet->panel_control_active )
      control_kill_control( d->sheet->panel_control );

  if( !sheet_has_refs( d->sheet ) && ! d->sheet->visible )
      sheet_remove( d->sheet );

  //free(d->name);
  //g_list_free( d->isl.inputevents );
  //g_list_free( d->isl.outputevents );
  //g_list_free( d->isl.inputsignals);
  //g_list_free( d->isl.outputsignals );


  safe_free(d);
}

PRIVATE void shcomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  ShCompData *d = safe_malloc(sizeof(ShCompData));
  
  d->sheet = sheet_unpickle( objectstore_item_get_object( item, "othersheet" ) );

  sheet_register_ref( d->sheet, c );
  //ShCompInitData *id;

  //d->name = safe_string_dup( objectstore_item_get_string(item, "name", "was geht denn hier ???" ) );
  //d->reftype = objectstore_item_get_integer(item, "reftype", SIGIN );

  d->isl.anzinputevents = objectstore_item_get_integer(item, "isl_anzinputevents", 0 );
  d->isl.anzoutputevents = objectstore_item_get_integer(item, "isl_anzoutputevents", 0 );
  d->isl.anzinputsignals = objectstore_item_get_integer(item, "isl_anzinputsignals", 0 );
  d->isl.anzoutputsignals = objectstore_item_get_integer(item, "isl_anzoutputsignals", 0 );

  d->isl.inputevents = objectstore_extract_list_of_items( objectstore_item_get( item, "isl_inputevents"),
	  item->db, (objectstore_unpickler_t) comp_unpickle);
  d->isl.outputevents = objectstore_extract_list_of_items( objectstore_item_get( item, "isl_outputevents"),
	  item->db, (objectstore_unpickler_t) comp_unpickle);
  d->isl.inputsignals = objectstore_extract_list_of_items( objectstore_item_get( item, "isl_inputsignals"),
	  item->db, (objectstore_unpickler_t) comp_unpickle);
  d->isl.outputsignals = objectstore_extract_list_of_items( objectstore_item_get( item, "isl_outputsignals"),
	  item->db, (objectstore_unpickler_t) comp_unpickle);

  
  c->data = d;
  //validate_connectors(c);
  shcomp_resize(c);	/* because things may be different if loading on a different
			   system from the one we saved with */
}

PRIVATE void shcomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  ShCompData *d = c->data;

  objectstore_item_set_object(item, "othersheet", sheet_pickle(d->sheet, db) );
  objectstore_item_set_integer(item, "isl_anzinputevents", d->isl.anzinputevents );
  objectstore_item_set_integer(item, "isl_anzoutputevents", d->isl.anzoutputevents );
  objectstore_item_set_integer(item, "isl_anzinputsignals", d->isl.anzinputsignals );
  objectstore_item_set_integer(item, "isl_anzoutputsignals", d->isl.anzoutputsignals );
  objectstore_item_set(item, "isl_inputevents", objectstore_create_list_of_items( d->isl.inputevents, db, 
	      (objectstore_pickler_t) comp_pickle) );
  objectstore_item_set(item, "isl_outputevents", objectstore_create_list_of_items( d->isl.outputevents, db, 
	      (objectstore_pickler_t) comp_pickle) );
  objectstore_item_set(item, "isl_inputsignals", objectstore_create_list_of_items( d->isl.inputsignals, db, 
	      (objectstore_pickler_t) comp_pickle) );
  objectstore_item_set(item, "isl_outputsignals", objectstore_create_list_of_items( d->isl.outputsignals, db, 
	      (objectstore_pickler_t) comp_pickle) );

  //objectstore_item_set_integer(item, "reftype", d->reftype);
}


/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE Component *shcomp_get_linked_componenent( Component *c, ConnectorReference *ref ) {

  Component *cc;
  ShCompData *d = c->data;
  GList *lst = (ref->kind == COMP_SIGNAL_CONNECTOR)
    ? (ref->is_output
	    ? d->isl.outputsignals : d->isl.inputsignals )
    : (ref->is_output
	    ? d->isl.outputevents : d->isl.inputevents );
  
  cc = g_list_nth( lst, ref->queue_number )->data;
  return cc;
}


PRIVATE void shcomp_paint(Component *c, GdkRectangle *area,
			   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  ShCompData *d = c->data;
  GList *l = c->connectors;
  GdkGC *gc = style->black_gc;

  while (l != NULL) {
    Connector *con = l->data;
    Component *linked = shcomp_get_linked_componenent( c, &(con->ref) );
    ISCompData *isdata = linked->data;
    ConnectorReference *tref = isdata->ref;
  
    int colidx;
    int x, y;

    if( !connectorreference_equal( &(con->ref), &(c->sheet->highlight_ref) ) )
	colidx = COMP_COLOR_VIOLET;
    else
	if ((tref->kind == COMP_SIGNAL_CONNECTOR)
		&& ((tref->is_output
			? ((GenCompData *)tref->c->data)->g->klass->out_sigs[tref->queue_number].flags
			: ((GenCompData *)tref->c->data)->g->klass->in_sigs[tref->queue_number].flags) & SIG_FLAG_RANDOMACCESS))
	    colidx = (con->refs == NULL) ? COMP_COLOR_RED : COMP_COLOR_YELLOW;
	else
	    colidx = (con->refs == NULL) ? COMP_COLOR_BLUE : COMP_COLOR_GREEN;

    
    gdk_gc_set_foreground(gc, &colors[colidx]);
    gdk_draw_arc(drawable, gc, TRUE,
		 con->x + c->x - (SHCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (SHCOMP_CONNECTOR_WIDTH>>1),
		 SHCOMP_CONNECTOR_WIDTH,
		 SHCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);
    gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
    gdk_draw_arc(drawable, gc, FALSE,
		 con->x + c->x - (SHCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (SHCOMP_CONNECTOR_WIDTH>>1),
		 SHCOMP_CONNECTOR_WIDTH,
		 SHCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);

    x = ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->x - (SHCOMP_CONNECTOR_SPACE + (SHCOMP_CONNECTOR_WIDTH>>1))
	    : con->x + (SHCOMP_CONNECTOR_WIDTH>>1))
	 : con->x) + c->x;

    y = ((con->ref.kind == COMP_EVENT_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->y - (SHCOMP_CONNECTOR_SPACE + (SHCOMP_CONNECTOR_WIDTH>>1))
	    : con->y + (SHCOMP_CONNECTOR_WIDTH>>1))
	 : con->y) + c->y;

    gdk_draw_line(drawable, gc,
		  x, y,
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? x + SHCOMP_CONNECTOR_SPACE : x),
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? y : y + SHCOMP_CONNECTOR_SPACE));

    l = g_list_next(l);
  }

  gdk_gc_set_foreground(gc, &style->black);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + SHCOMP_BORDER_WIDTH,
		     c->y + SHCOMP_BORDER_WIDTH,
		     c->width - 2 * SHCOMP_BORDER_WIDTH,
		     c->height - 2 * SHCOMP_BORDER_WIDTH);
  gdk_gc_set_foreground(gc, &colors[2]);
  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + SHCOMP_BORDER_WIDTH,
		     c->y + SHCOMP_BORDER_WIDTH,
		     c->width - 2 * SHCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * SHCOMP_BORDER_WIDTH - 1);

  gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
  gdk_draw_text(drawable, gtk_style_get_font(style), gc,
		c->x + SHCOMP_BORDER_WIDTH + (SHCOMP_CONNECTOR_WIDTH>>1),
		c->y + SHCOMP_BORDER_WIDTH + SHCOMP_TITLEHEIGHT - 3,
		d->sheet->name, strlen(d->sheet->name));

  gdk_gc_set_foreground(gc, &style->black);

  if (NULL != NULL)
    gdk_draw_pixmap(drawable, gc, NULL, 0, 0,
		    c->x + (c->width>>1) - (SHCOMP_ICONLENGTH>>1),
		    c->y + ((c->height-SHCOMP_TITLEHEIGHT)>>1) - (SHCOMP_ICONLENGTH>>1)
		         + SHCOMP_TITLEHEIGHT,
		    SHCOMP_ICONLENGTH,
		    SHCOMP_ICONLENGTH);
}

PRIVATE int shcomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {
  GList *l = c->connectors;

  x -= c->x;
  y -= c->y;

  while (l != NULL) {
    Connector *con = l->data;

    if (ABS(x - con->x) < (SHCOMP_CONNECTOR_WIDTH>>1) &&
	ABS(y - con->y) < (SHCOMP_CONNECTOR_WIDTH>>1)) {
      if (ref != NULL)
	*ref = con->ref;
      return 1;
    }

    l = g_list_next(l);
  }

  return 0;
}

PRIVATE int shcomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;

  if (dx >= SHCOMP_BORDER_WIDTH &&
      dy >= SHCOMP_BORDER_WIDTH &&
      dx < (c->width - SHCOMP_BORDER_WIDTH) &&
      dy < (c->height - SHCOMP_BORDER_WIDTH))
    return 1;

  return shcomp_find_connector_at(c, x, y, NULL);
}


PRIVATE gboolean shcomp_accept_outbound(Component *c, ConnectorReference *src,
					 ConnectorReference *dst) {

  Component *linked = shcomp_get_linked_componenent( c, src );
  ISCompData *isdata = linked->data;
  ConnectorReference *tref = isdata->ref;
  
  return tref->c->klass->accept_outbound( tref->c, tref, dst );
}

PRIVATE gboolean shcomp_accept_inbound(Component *c, ConnectorReference *src,
					ConnectorReference *dst) {

  Component *linked = shcomp_get_linked_componenent( c, dst );
  ISCompData *isdata = linked->data;
  ConnectorReference *tref = isdata->ref;

  return src->c->klass->accept_outbound( src->c, src, tref );
}

PRIVATE gboolean shcomp_unlink_outbound(Component *c, ConnectorReference *src,
				     ConnectorReference *dst) {

  Component *linked = shcomp_get_linked_componenent( c, src );
  ISCompData *isdata = linked->data;
  ConnectorReference *tref = isdata->ref;
  
  return tref->c->klass->unlink_outbound( tref->c, tref, dst );
}

PRIVATE gboolean shcomp_unlink_inbound(Component *c, ConnectorReference *src,
				    ConnectorReference *dst) {
  Component *linked = shcomp_get_linked_componenent( c, dst );
  ISCompData *isdata = linked->data;
  ConnectorReference *tref = isdata->ref;

  return src->c->klass->unlink_outbound( src->c, src, tref );
}

PRIVATE char *shcomp_get_title(Component *c) {

  ShCompData *d = c->data;
  return safe_string_dup( d->sheet->name );
}

PRIVATE char *shcomp_get_connector_name(Component *c, ConnectorReference *ref) {

  Component *cc;
  ShCompData *d = c->data;
  GList *lst = (ref->kind == COMP_SIGNAL_CONNECTOR)
    ? (ref->is_output
	    ? d->isl.outputsignals : d->isl.inputsignals )
    : (ref->is_output
	    ? d->isl.outputevents : d->isl.inputevents );
  
  cc = g_list_nth( lst, ref->queue_number )->data;
  return cc->klass->get_title( cc );
}



PRIVATE void do_rename(Component *c, guint action, GtkWidget *widget) {
  ShCompData *d = c->data;
  sheet_rename( d->sheet );
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
    gtk_widget_show( viewport );
    gtk_widget_reparent( control->this_panel->fixedwidget, viewport );
}

PRIVATE  ControlDescriptor desc = 
  { CONTROL_KIND_PANEL, "panel", 0,0,0,0, 0,FALSE, TRUE,0, init_panel, done_panel, NULL, NULL };

PRIVATE void do_control(Component *c, guint action, GtkWidget *widget) {
  ShCompData *d = c->data;

  //gtk_object_ref( GTK_OBJECT(d->sheet->control_panel->fixedwidget) );
  desc.refresh_data = d->sheet->control_panel;
  panel_fixed = d->sheet->control_panel->fixedwidget;
  d->sheet->panel_control = control_new_control( &desc, NULL, c->sheet->control_panel );
  d->sheet->panel_control_active = TRUE;
  control_panel_unregister_panel( d->sheet->control_panel );
}

PUBLIC void shcomp_do_control( Component *c ) {
    do_control( c, 0, NULL );
}

//PRIVATE void do_props(Component *c, guint action, GtkWidget *widget) {
//  ShCompData *d = c->data;
//  if (d->propgen)
//    d->propgen(c, d->g);
//}



PRIVATE void do_visible(Component *c, guint action, GtkWidget *widget) {

    ShCompData *d = c->data;
    GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM( widget );
    gboolean visible = gtk_check_menu_item_get_active( check );

    if( d->sheet->visible == visible )
	return;

    if( visible )
    {
	d->sheet->visible = TRUE;
	gui_register_sheet( d->sheet );
    }
    else
    {
	d->sheet->visible = TRUE;
	gui_unregister_sheet( d->sheet );
	d->sheet->visible = FALSE;
    }

}

PRIVATE void do_delete(Component *c, guint action, GtkWidget *widget) {
  sheet_delete_component(c->sheet, c);
}

PRIVATE GtkItemFactoryEntry popup_items[] = {
  { "/_Rename...",	NULL,	do_rename, 0,		NULL },
  { "/Add _Control",	NULL,	do_control, 0,		NULL },
  { "/Sheet visible",	NULL,	do_visible, 0,		"<ToggleItem>" },
//  { "/_Properties...",	NULL,	do_props, 0,		NULL },
  { "/sep1",		NULL,	NULL, 0,		"<Separator>" },
  { "/_Delete",		NULL,	do_delete, 0,		NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

PRIVATE GtkWidget *shcomp_build_popup(Component *c) {
  ShCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<shcomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);

  result = gtk_item_factory_get_widget(ifact, "<shcomp-popup>");

  if( d->sheet->panel_control_active )
      gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<shcomp-popup>/Add Control"),
	      GTK_STATE_INSENSITIVE);

  gtk_check_menu_item_set_active( 
	  GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item( ifact, "<shcomp-popup>/Sheet visible" )),
	  d->sheet->visible );


  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass SheetComponentClass = {
  "shcomp",

  shcomp_initialize,
  shcomp_destroy,
  shcomp_clone,

  shcomp_unpickle,
  shcomp_pickle,

  shcomp_paint,

  shcomp_find_connector_at,
  shcomp_contains_point,

  shcomp_accept_outbound,
  shcomp_accept_inbound,
  shcomp_unlink_outbound,
  shcomp_unlink_inbound,

  shcomp_get_title,
  shcomp_get_connector_name,

  shcomp_build_popup
};

PRIVATE ComponentClass FileSheetComponentClass = {
  "shcomp",

  fileshcomp_initialize,
  shcomp_destroy,
  shcomp_clone,

  shcomp_unpickle,
  shcomp_pickle,

  shcomp_paint,

  shcomp_find_connector_at,
  shcomp_contains_point,

  shcomp_accept_outbound,
  shcomp_accept_inbound,
  shcomp_unlink_outbound,
  shcomp_unlink_inbound,

  shcomp_get_title,
  shcomp_get_connector_name,

  shcomp_build_popup
};


// Code for the Sheet Library
// nice and compact :)

PRIVATE void add_gsheet(char *plugin, char *leafname) {

    FileShCompInitData *id = safe_malloc( sizeof( FileShCompInitData ) );

    id->filename = g_strdup_printf( "%s", plugin );

    comp_add_newmenu_item( leafname, &FileSheetComponentClass, id );
}

PRIVATE void load_all_gsheets(char *dir, char *menupos );	/* forward decl */

PRIVATE int check_gsheet_validity(char *name, char *menupos, const char *dirname ) {
    struct stat sb;

    if (stat(name, &sb) == -1)
	return 0;

    if (S_ISDIR(sb.st_mode)) {
	// XXX: how do i get this name nicely ?
	//      dont care now works....

	char *newmenupos = g_strdup_printf( "%s/%s", menupos, dirname );
	load_all_gsheets(name, newmenupos );
	free( newmenupos );
    }

    if( strlen(name) < 8 || strcmp(name+(strlen(name)-7), ".gsheet" ) )
	return 0;

    return S_ISREG(sb.st_mode);
}

PRIVATE void load_all_gsheets(char *dir, char *menupos) {
    GDir *d = g_dir_open(dir, 0, NULL);
    const char *filename;

    if (d == NULL)
	/* the plugin directory cannot be read */
	return;

    while ((filename = g_dir_read_name(d)) != NULL) {
	char *fullname;

	fullname = g_strdup_printf( "%s%s%s", dir, G_DIR_SEPARATOR_S, filename ); 

	if (check_gsheet_validity(fullname, menupos, filename)) {
	    char *menuname = g_strdup_printf( "%s/%s", menupos, filename );
	    add_gsheet(fullname, menuname);
	    free( menuname );
	}

	free(fullname);
    }

    g_dir_close(d);
}

PRIVATE void scan_library_dir( void ) {
    char *sheetdir = getenv("GALAN_SHEET_DIR");

    if( sheetdir )
	load_all_gsheets( sheetdir, "Lib" );
    else
	load_all_gsheets(SITE_PKGDATA_DIR G_DIR_SEPARATOR_S "sheets", "Lib" );
}


PUBLIC void shcomp_register_sheet( Sheet *sheet ) {

    ShCompInitData *initdata = safe_malloc( sizeof( ShCompInitData ) );

    gchar *str = g_strdup_printf( "Sheets/%s", sheet->name );

    initdata->sheet = sheet;
    comp_add_newmenu_item( str, &SheetComponentClass, initdata );

    g_free( str );
}

PUBLIC void init_shcomp(void) {
  comp_register_componentclass(&SheetComponentClass);
  comp_register_componentclass(&FileSheetComponentClass);

  scan_library_dir();
}

PUBLIC void done_shcomp(void) {
}

