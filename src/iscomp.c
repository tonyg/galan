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
 * this is gencomp.c from tony. I modifie it to be iscom.c
 *
 * iscomp only means a reference to a connector therefor
 * it only stores a ConnectorReference.
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
#include "iscomp.h"

#define ISCOMP_ICONLENGTH	48
#define ISCOMP_TITLEHEIGHT	15
#define ISCOMP_CONNECTOR_SPACE	5
#define ISCOMP_CONNECTOR_WIDTH	10
#define ISCOMP_BORDER_WIDTH	(ISCOMP_CONNECTOR_WIDTH + ISCOMP_CONNECTOR_SPACE)


PRIVATE int next_component_number = 1;
PRIVATE ComponentClass InterSheetComponentClass;	/* forward reference */


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
  int startpos = is_outbound ? (ISCOMP_BORDER_WIDTH * 2
				+ (is_signal ? hsize : vsize)
				- (ISCOMP_CONNECTOR_WIDTH >> 1))
			     : (ISCOMP_CONNECTOR_WIDTH >> 1);
  int x = is_signal ? startpos : ISCOMP_BORDER_WIDTH + spacing;
  int y = is_signal ? ISCOMP_BORDER_WIDTH + spacing : startpos;
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

PRIVATE void iscomp_resize(Component *c) {
  int body_vert, body_horiz;
  ISCompData *d = c->data;

  int in_sig_count=0, out_sig_count=0, in_count=0, out_count=0;
  gboolean icon=FALSE;
  
  switch( d->reftype ) {
      case SIGIN:
	  in_sig_count=1;
	  break;
      case SIGOUT:
	  out_sig_count=1;
	  break;
      case EVTIN:
	  in_count=1;
	  break;
      case EVTOUT:
	  out_count=1;
	  break;
  }

  body_vert =
    ISCOMP_CONNECTOR_WIDTH
    + MAX(MAX(in_sig_count, out_sig_count) * ISCOMP_CONNECTOR_WIDTH,
	  ISCOMP_TITLEHEIGHT + (icon ? ISCOMP_ICONLENGTH : 0));
  body_horiz =
    ISCOMP_CONNECTOR_WIDTH
    + MAX(2,
	  MAX(sheet_get_textwidth(c->sheet, d->name),
	      MAX(in_count * ISCOMP_CONNECTOR_WIDTH,
		  out_count * ISCOMP_CONNECTOR_WIDTH)));

  resize_connectors(c, in_count, 0, 0, body_horiz, body_vert);
  resize_connectors(c, in_sig_count, 0, 1, body_horiz, body_vert);
  resize_connectors(c, out_count, 1, 0, body_horiz, body_vert);
  resize_connectors(c, out_sig_count, 1, 1, body_horiz, body_vert);

  c->width = body_horiz + 2*ISCOMP_BORDER_WIDTH + 1;
  c->height = body_vert + 2*ISCOMP_BORDER_WIDTH + 1;
}

PRIVATE int iscomp_initialize(Component *c, gpointer init_data) {
  ISCompData *d = safe_malloc(sizeof(ISCompData));
  ISCompInitData *id = (ISCompInitData *) init_data;

  d->name = safe_malloc(strlen("con") + 20);
  sprintf(d->name, "con%d", next_component_number++);

  d->ref = NULL;

  d->reftype = id->reftype;
  d->poly = id->poly;
  switch( d->reftype ) {
      case EVTIN:
	  build_connectors(c, 1, 0, 0);
	  break;
      case SIGIN:
	  build_connectors(c, 1, 0, 1);
	  break;
      case EVTOUT:
	  build_connectors(c, 1, 1, 0);
	  break;
      case SIGOUT:
	  build_connectors(c, 1, 1, 1);
  }

  c->x -= ISCOMP_BORDER_WIDTH;
  c->y -= ISCOMP_BORDER_WIDTH;
  c->width = c->height = 0;
  c->data = d;


  iscomp_resize(c);

  return 1;
}

PRIVATE void iscomp_destroy(Component *c) {
  ISCompData *d = c->data;

  free(d->name);

  free(d);
}

PRIVATE void iscomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  ISCompData *d = safe_malloc(sizeof(ISCompData));

  d->ref = NULL;

  d->name = safe_string_dup( objectstore_item_get_string(item, "name", "was geht denn hier ???" ) );
  d->reftype = objectstore_item_get_integer(item, "reftype", SIGIN );
  d->poly = objectstore_item_get_integer(item, "poly", FALSE );

  if( objectstore_item_get_integer( item, "refvalid", 1 ) )
      d->ref = unpickle_connectorreference( d->ref, objectstore_item_get_object( item, "ref" ) );

  c->data = d;
  //validate_connectors(c);
  iscomp_resize(c);	/* because things may be different if loading on a different
			   system from the one we saved with */
}

PRIVATE void iscomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  ISCompData *d = c->data;
  objectstore_item_set_string(item, "name", d->name);
  objectstore_item_set_integer(item, "reftype", d->reftype);
  objectstore_item_set_integer(item, "poly", d->poly);
  if( d->ref )
      objectstore_item_set_object( item, "ref", pickle_connectorreference( d->ref, db ) );
      
  objectstore_item_set_integer( item, "refvalid", (d->ref != NULL) );
}

PRIVATE Component *iscomp_clone( Component *c, Sheet *sheet ) {
    ISCompInitData id;
    ISCompData *srcdata = c->data;
    ISCompData *clonedata;
    Component *clone;

    id.reftype = srcdata->reftype;
    id.poly = srcdata->poly;
    
    clone = comp_new_component(  c->klass, &id, sheet, 0, 0 );
    clonedata = clone->data;
    g_free( clonedata->name );
	clonedata->name = safe_string_dup( srcdata->name );
    
    iscomp_resize(clone);
    //sheet_queue_redraw_component( c->sheet, c );

    return clone;
}

/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE void iscomp_paint(Component *c, GdkRectangle *area, GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  ISCompData *d = c->data;
  GList *l = c->connectors;
  GdkGC *gc = style->black_gc;

  while (l != NULL) {
    Connector *con = l->data;
    int colidx;
    int x, y;

//    if ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
//	&& ((con->ref.is_output
//	     ? d->g->klass->out_sigs[con->ref.queue_number].flags
//	     : d->g->klass->in_sigs[con->ref.queue_number].flags) & SIG_FLAG_RANDOMACCESS))
//     colidx = (con->refs == NULL) ? COMP_COLOR_RED : COMP_COLOR_YELLOW;
//    else
    if( !connectorreference_equal( &(con->ref), &(c->sheet->highlight_ref) ) )
	colidx = COMP_COLOR_VIOLET;
    else
	colidx = (con->refs == NULL) ? COMP_COLOR_BLUE : COMP_COLOR_GREEN;

//    colidx = COMP_COLOR_GREEN;
    
    gdk_gc_set_foreground(gc, &colors[colidx]);
    gdk_draw_arc(drawable, gc, TRUE,
		 con->x + c->x - (ISCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (ISCOMP_CONNECTOR_WIDTH>>1),
		 ISCOMP_CONNECTOR_WIDTH,
		 ISCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);
    gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
    gdk_draw_arc(drawable, gc, FALSE,
		 con->x + c->x - (ISCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (ISCOMP_CONNECTOR_WIDTH>>1),
		 ISCOMP_CONNECTOR_WIDTH,
		 ISCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);

    x = ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->x - (ISCOMP_CONNECTOR_SPACE + (ISCOMP_CONNECTOR_WIDTH>>1))
	    : con->x + (ISCOMP_CONNECTOR_WIDTH>>1))
	 : con->x) + c->x;

    y = ((con->ref.kind == COMP_EVENT_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->y - (ISCOMP_CONNECTOR_SPACE + (ISCOMP_CONNECTOR_WIDTH>>1))
	    : con->y + (ISCOMP_CONNECTOR_WIDTH>>1))
	 : con->y) + c->y;

    gdk_draw_line(drawable, gc,
		  x, y,
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? x + ISCOMP_CONNECTOR_SPACE : x),
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? y : y + ISCOMP_CONNECTOR_SPACE));

    l = g_list_next(l);
  }

  gdk_gc_set_foreground(gc, &style->black);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + ISCOMP_BORDER_WIDTH,
		     c->y + ISCOMP_BORDER_WIDTH,
		     c->width - 2 * ISCOMP_BORDER_WIDTH,
		     c->height - 2 * ISCOMP_BORDER_WIDTH);
  if( d->poly )
      gdk_gc_set_foreground(gc, &colors[COMP_COLOR_YELLOW]);
  else
      gdk_gc_set_foreground(gc, &colors[4]);

  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + ISCOMP_BORDER_WIDTH,
		     c->y + ISCOMP_BORDER_WIDTH,
		     c->width - 2 * ISCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * ISCOMP_BORDER_WIDTH - 1);

    gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
  gdk_draw_text(drawable, gtk_style_get_font(style), gc,
		c->x + ISCOMP_BORDER_WIDTH + (ISCOMP_CONNECTOR_WIDTH>>1),
		c->y + ISCOMP_BORDER_WIDTH + ISCOMP_TITLEHEIGHT - 3,
		d->name, strlen(d->name));
  gdk_gc_set_foreground(gc, &style->black);

  if (NULL != NULL)
    gdk_draw_pixmap(drawable, gc, NULL, 0, 0,
		    c->x + (c->width>>1) - (ISCOMP_ICONLENGTH>>1),
		    c->y + ((c->height-ISCOMP_TITLEHEIGHT)>>1) - (ISCOMP_ICONLENGTH>>1)
		         + ISCOMP_TITLEHEIGHT,
		    ISCOMP_ICONLENGTH,
		    ISCOMP_ICONLENGTH);
}

PRIVATE int iscomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {
  GList *l = c->connectors;

  x -= c->x;
  y -= c->y;

  while (l != NULL) {
    Connector *con = l->data;

    if (ABS(x - con->x) < (ISCOMP_CONNECTOR_WIDTH>>1) &&
	ABS(y - con->y) < (ISCOMP_CONNECTOR_WIDTH>>1)) {
      if (ref != NULL)
	*ref = con->ref;
      return 1;
    }

    l = g_list_next(l);
  }

  return 0;
}

PRIVATE int iscomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;

  if (dx >= ISCOMP_BORDER_WIDTH &&
      dy >= ISCOMP_BORDER_WIDTH &&
      dx < (c->width - ISCOMP_BORDER_WIDTH) &&
      dy < (c->height - ISCOMP_BORDER_WIDTH))
    return 1;

  return iscomp_find_connector_at(c, x, y, NULL);
}



/**
 * \brief tries to establich a connection between \a src and \a dst.
 *
 * Note that galan graphs are directed.
 *
 * \param c The component with the source connector.
 * \param src from ConnectorReference.
 * \param dst to ConnectorReference.
 */

PRIVATE gboolean iscomp_accept_outbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {
  ISCompData *data = c->data;

  if( data->ref != NULL )
      return FALSE;

  data->ref = safe_malloc( sizeof( ConnectorReference ) );
  *(data->ref) = *dst;
  return TRUE;
}

PRIVATE gboolean iscomp_accept_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {
  ISCompData *data = c->data;

  if( data->ref != NULL )
      return FALSE;

  data->ref = safe_malloc( sizeof( ConnectorReference ) );
  *(data->ref) = *src;
  return TRUE;
}

PRIVATE gboolean iscomp_unlink_outbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    if( c->sheet->referring_sheets != NULL ) {
	popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		"Sheet %s is connected to other sheets.\n"
		"I cant unconnect this link. Please unconnect first.", c->sheet->name );
	return FALSE;
    } else {

	ISCompData *data = c->data;
	free( data->ref );
	data->ref = NULL;

	return TRUE;
    }
}

PRIVATE gboolean iscomp_unlink_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    if( c->sheet->referring_sheets != NULL ) {
	popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		"Sheet %s is connected to other sheets.\n"
		"I cant unconnect this link. Please unconnect first.", c->sheet->name );
	return FALSE;
    } else {
	ISCompData *data = c->data;
	free( data->ref );
	data->ref = NULL;

	return TRUE;
    }
}

PRIVATE char *iscomp_get_title(Component *c) {

  ISCompData *d = c->data;
  return safe_string_dup( d->name );
}

PRIVATE char *iscomp_get_connector_name(Component *c, ConnectorReference *ref) {

  ISCompData *d = c->data;
  return safe_string_dup( d->name );
}

//PRIVATE void rename_controls(Control *c, Generator *g) {
//  control_update_names(c);
//}

PRIVATE GtkWidget *rename_text_widget = NULL;

PRIVATE void rename_handler(MsgBoxResponse action_taken, Component *c) {
    if (action_taken == MSGBOX_OK) {
	ISCompData *d = c->data;
	free(d->name);
	d->name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(rename_text_widget)));

	sheet_queue_redraw_component(c->sheet, c);	/* to 'erase' the old size */
	iscomp_resize(c);
	sheet_queue_redraw_component(c->sheet, c);	/* to 'fill in' the new size */
    }
}

PRIVATE void do_rename(Component *c, guint action, GtkWidget *widget) {
    ISCompData *d = c->data;
    GtkWidget *hb = gtk_hbox_new(FALSE, 5);
    GtkWidget *label = gtk_label_new("Rename InterSheet:");
    GtkWidget *text = gtk_entry_new();

    gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

    gtk_widget_show(label);
    gtk_widget_show(text);

    gtk_entry_set_text(GTK_ENTRY(text), d->name);

    rename_text_widget = text;
    popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	    (MsgBoxResponseHandler) rename_handler, c);
}

//PRIVATE void do_props(Component *c, guint action, GtkWidget *widget) {
//  ISCompData *d = c->data;
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
//  { "/sep1",		NULL,	NULL, 0,		"<Separator>" },
  { "/_Delete",		NULL,	do_delete, 0,		NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

//PRIVATE void new_control_callback(Component *c, guint control_index, GtkWidget *menuitem) {
//  ISCompData *d = c->data;
//
//  control_new_control(&d->g->klass->controls[control_index], d->g);
//}

#define NEW_CONTROL_PREFIX "/New Control/"

PRIVATE GtkWidget *iscomp_build_popup(Component *c) {
  //ISCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<iscomp-popup>", NULL);
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

  result = gtk_item_factory_get_widget(ifact, "<iscomp-popup>");

#ifndef NATIVE_WIN32
  /* %%% Why does gtk_item_factory_get_item() not exist in the gtk-1.3 libraries?
     Maybe it's something I'm doing wrong. I'll have another go at fixing it later. */
//  if (d->g->klass->numcontrols == 0)
//    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<iscomp-popup>/New Control"),
//			 GTK_STATE_INSENSITIVE);
//
//  if (d->propgen == NULL)
//    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<iscomp-popup>/Properties..."),
//			 GTK_STATE_INSENSITIVE);
#endif

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass InterSheetComponentClass = {
  "iscomp",

  iscomp_initialize,
  iscomp_destroy,
  iscomp_clone,

  iscomp_unpickle,
  iscomp_pickle,

  iscomp_paint,

  iscomp_find_connector_at,
  iscomp_contains_point,

  iscomp_accept_outbound,
  iscomp_accept_inbound,
  iscomp_unlink_outbound,
  iscomp_unlink_inbound,

  iscomp_get_title,
  iscomp_get_connector_name,

  iscomp_build_popup
};

//PUBLIC void iscomp_register_class(GeneratorClass *k, gboolean prefer,
//					    char *menupath, char *iconpath,
//					    PropertiesCallback propgen) {
//  ISCompInitData *id = safe_malloc(sizeof(ISCompInitData));
//
//  id->k = k;
//  id->iconpath = safe_string_dup(iconpath);
//  id->propgen = propgen;
//
//  comp_add_newmenu_item(menupath, &GeneratorComponentClass, id);
//
//  /* Only insert into hash table if this name is not already taken, or if we are preferred. */
//  {
//    ISCompInitData *oldid = g_hash_table_lookup(generatorclasses, k->name);
//
//    if (oldid == NULL)
//      g_hash_table_insert(generatorclasses, k->name, id);
//    else if (prefer) {
//      g_hash_table_remove(generatorclasses, k->name);
//      g_hash_table_insert(generatorclasses, k->name, id);
//    }
//  }
//}
ISCompInitData id_sigin =  { SIGIN, FALSE };
ISCompInitData id_sigout = { SIGOUT, FALSE };
ISCompInitData id_evtin =  { EVTIN, FALSE };
ISCompInitData id_evtout = { EVTOUT, FALSE };
ISCompInitData id_poly_sigin =  { SIGIN, TRUE };
ISCompInitData id_poly_sigout = { SIGOUT, TRUE };
ISCompInitData id_poly_evtin =  { EVTIN, TRUE };
ISCompInitData id_poly_evtout = { EVTOUT, TRUE };

PUBLIC void init_iscomp(void) {
//  generatorclasses = g_hash_table_new(g_str_hash, g_str_equal);
  comp_register_componentclass(&InterSheetComponentClass);
  comp_add_newmenu_item( "InterSheet/SigOUT", &InterSheetComponentClass, &id_sigin );
  comp_add_newmenu_item( "InterSheet/SigIN", &InterSheetComponentClass, &id_sigout );
  comp_add_newmenu_item( "InterSheet/EvtOUT", &InterSheetComponentClass, &id_evtin );
  comp_add_newmenu_item( "InterSheet/EvtIN", &InterSheetComponentClass, &id_evtout );
  comp_add_newmenu_item( "InterSheet/SigOUT Poly", &InterSheetComponentClass, &id_poly_sigin );
  comp_add_newmenu_item( "InterSheet/SigIN Poly", &InterSheetComponentClass, &id_poly_sigout );
  comp_add_newmenu_item( "InterSheet/EvtOUT Poly", &InterSheetComponentClass, &id_poly_evtin );
  comp_add_newmenu_item( "InterSheet/EvtIN Poly", &InterSheetComponentClass, &id_poly_evtout );
}

PUBLIC void done_iscomp(void) {
  //g_hash_table_destroy(generatorclasses);
  //generatorclasses = NULL;
}
