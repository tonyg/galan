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

#define SHCOMP_ICONLENGTH	48
#define SHCOMP_TITLEHEIGHT	15
#define SHCOMP_CONNECTOR_SPACE	5
#define SHCOMP_CONNECTOR_WIDTH	10
#define SHCOMP_BORDER_WIDTH	(SHCOMP_CONNECTOR_WIDTH + SHCOMP_CONNECTOR_SPACE)


//PRIVATE int next_component_number = 1;
//PRIVATE ComponentClass InterSheetComponentClass;	/* forward reference */


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

PRIVATE void shcomp_resize(Component *c) {
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

  c->width = body_horiz + 2 * SHCOMP_BORDER_WIDTH;
  c->height = body_vert + 2 * SHCOMP_BORDER_WIDTH;
}

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

PRIVATE int shcomp_initialize(Component *c, gpointer init_data) {
  ShCompData *d = safe_malloc(sizeof(ShCompData));
  ShCompInitData *id = (ShCompInitData *) init_data;
  InterSheetLinks *isl = find_intersheet_links( id->sheet );

  d->sheet = id->sheet;
  
  d->isl = *isl;


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

PRIVATE void shcomp_destroy(Component *c) {
  ShCompData *d = c->data;

  //free(d->name);
  //g_list_free( d->isl.inputevents );
  //g_list_free( d->isl.outputevents );
  //g_list_free( d->isl.inputsignals);
  //g_list_free( d->isl.outputsignals );

  free(d);
}

PRIVATE void shcomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  ShCompData *d = safe_malloc(sizeof(ShCompData));
  
  d->sheet = sheet_unpickle( objectstore_item_get_object( item, "othersheet" ), NULL );
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
    gdk_draw_arc(drawable, style->white_gc, FALSE,
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

    gdk_draw_line(drawable, style->white_gc,
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
  gdk_gc_set_foreground(gc, &style->black);

  gdk_draw_text(drawable, style->font, style->white_gc,
		c->x + SHCOMP_BORDER_WIDTH + (SHCOMP_CONNECTOR_WIDTH>>1),
		c->y + SHCOMP_BORDER_WIDTH + SHCOMP_TITLEHEIGHT - 3,
		d->sheet->name, strlen(d->sheet->name));

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

PRIVATE void shcomp_unlink_outbound(Component *c, ConnectorReference *src,
				     ConnectorReference *dst) {

  Component *linked = shcomp_get_linked_componenent( c, src );
  ISCompData *isdata = linked->data;
  ConnectorReference *tref = isdata->ref;
  
  return tref->c->klass->unlink_outbound( tref->c, tref, dst );
}

PRIVATE void shcomp_unlink_inbound(Component *c, ConnectorReference *src,
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

PRIVATE GtkWidget *rename_text_widget = NULL;

PRIVATE void rename_handler(MsgBoxResponse action_taken, Component *c) {
  if (action_taken == MSGBOX_OK) {
    ShCompData *d = c->data;
    free(d->sheet->name);
    d->sheet->name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(rename_text_widget)));
    update_sheet_name( d->sheet );

    sheet_queue_redraw_component(c->sheet, c);	/* to 'erase' the old size */
    shcomp_resize(c);
    sheet_queue_redraw_component(c->sheet, c);	/* to 'fill in' the new size */
  }
}

PRIVATE void do_rename(Component *c, guint action, GtkWidget *widget) {
  ShCompData *d = c->data;
  GtkWidget *hb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Rename Sheet:");
  GtkWidget *text = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

  gtk_widget_show(label);
  gtk_widget_show(text);

 gtk_entry_set_text(GTK_ENTRY(text), d->sheet->name);

  rename_text_widget = text;
  popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	       (MsgBoxResponseHandler) rename_handler, c);
}

//PRIVATE void do_props(Component *c, guint action, GtkWidget *widget) {
//  ShCompData *d = c->data;
//  if (d->propgen)
//    d->propgen(c, d->g);
//}

PRIVATE void do_delete(Component *c, guint action, GtkWidget *widget) {
  sheet_delete_component(c->sheet, c);
}

PRIVATE GtkItemFactoryEntry popup_items[] = {
  { "/_Rename...",	NULL,	do_rename, 0,		NULL },
//  { "/New _Control",	NULL,	NULL, 0,		"<Branch>" },
//  { "/_Properties...",	NULL,	do_props, 0,		NULL },
  { "/sep1",		NULL,	NULL, 0,		"<Separator>" },
  { "/_Delete",		NULL,	do_delete, 0,		NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

//PRIVATE void new_control_callback(Component *c, guint control_index, GtkWidget *menuitem) {
//  ShCompData *d = c->data;
//
//  control_new_control(&d->g->klass->controls[control_index], d->g);
//}

#define NEW_CONTROL_PREFIX "/New Control/"

PRIVATE GtkWidget *shcomp_build_popup(Component *c) {
  //ShCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<shcomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);

//  for (i = 0; i < d->g->klass->numcontrols; i++) {
//    GtkItemFactoryEntry ent = { NULL, NULL, new_control_callback, i, NULL };
//    char *name = malloc(strlen(NEW_CONTROL_PREFIX) + strlen(d->g->klass->controls[i].name) + 1);
//
//    strcpy(name, NEW_CONTROL_PREFIX);
//    strcat(name, d->g->klass->controls[i].name);
//    ent.path = name;
//
//    gtk_item_factory_create_item(ifact, &ent, c, 1);
//    free(name);
//  }

  result = gtk_item_factory_get_widget(ifact, "<shcomp-popup>");

#ifndef NATIVE_WIN32
  /* %%% Why does gtk_item_factory_get_item() not exist in the gtk-1.3 libraries?
     Maybe it's something I'm doing wrong. I'll have another go at fixing it later. */
//  if (d->g->klass->numcontrols == 0)
//    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<shcomp-popup>/New Control"),
//			 GTK_STATE_INSENSITIVE);
//
//  if (d->propgen == NULL)
//    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<shcomp-popup>/Properties..."),
//			 GTK_STATE_INSENSITIVE);
#endif

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass SheetComponentClass = {
  "shcomp",

  shcomp_initialize,
  shcomp_destroy,

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

//PUBLIC void shcomp_register_class(GeneratorClass *k, gboolean prefer,
//					    char *menupath, char *iconpath,
//					    PropertiesCallback propgen) {
//  ShCompInitData *id = safe_malloc(sizeof(ShCompInitData));
//
//  id->k = k;
//  id->iconpath = safe_string_dup(iconpath);
//  id->propgen = propgen;
//
//  comp_add_newmenu_item(menupath, &GeneratorComponentClass, id);
//
//  /* Only insert into hash table if this name is not already taken, or if we are preferred. */
//  {
//    ShCompInitData *oldid = g_hash_table_lookup(generatorclasses, k->name);
//
//    if (oldid == NULL)
//      g_hash_table_insert(generatorclasses, k->name, id);
//    else if (prefer) {
//      g_hash_table_remove(generatorclasses, k->name);
//      g_hash_table_insert(generatorclasses, k->name, id);
//    }
//  }
//}
ShCompInitData initdata =  { NULL };

PUBLIC void shcomp_register_sheet( Sheet *sheet ) {

    ShCompInitData *initdata = safe_malloc( sizeof( ShCompInitData ) );
    gchar *str = safe_malloc( sizeof("Sheets/") + strlen( sheet->name ) );
    sprintf( str, "Sheets/%s", sheet->name );

    initdata->sheet = sheet;
    comp_add_newmenu_item( str, &SheetComponentClass, initdata );
}

PUBLIC void init_shcomp(void) {
  comp_register_componentclass(&SheetComponentClass);
}

PUBLIC void done_shcomp(void) {
  //g_hash_table_destroy(generatorclasses);
  //generatorclasses = NULL;
}
