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
 * cocomp only means a reference to a connector therefor
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
#include "cocomp.h"

#define COCOMP_ICONLENGTH	48
#define COCOMP_TITLEHEIGHT	15
#define COCOMP_CONNECTOR_SPACE	5
#define COCOMP_CONNECTOR_WIDTH	10
#define COCOMP_BORDER_WIDTH	(COCOMP_CONNECTOR_WIDTH + COCOMP_CONNECTOR_SPACE)


PRIVATE int next_component_number = 1;
PRIVATE ComponentClass CommentComponentClass;	/* forward reference */




PRIVATE void cocomp_resize(Component *c) {
  int body_vert, body_horiz;
  COCompData *d = c->data;
  //COCompData *d = c->data;


  body_vert = sheet_get_textheight( c->sheet, d->name ) + (COCOMP_CONNECTOR_WIDTH);
//    COCOMP_CONNECTOR_WIDTH
//    + MAX(MAX(in_sig_count, out_sig_count) * COCOMP_CONNECTOR_WIDTH,
//	  COCOMP_TITLEHEIGHT + (icon ? COCOMP_ICONLENGTH : 0));
  body_horiz = sheet_get_textwidth(c->sheet, d->name) + (COCOMP_CONNECTOR_WIDTH);
//    COCOMP_CONNECTOR_WIDTH
//    + MAX(2,
//	  MAX(sheet_get_textwidth(c->sheet, d->name),
//	      MAX(in_count * COCOMP_CONNECTOR_WIDTH,
//		  out_count * COCOMP_CONNECTOR_WIDTH)));


  c->width = body_horiz + 2 * COCOMP_BORDER_WIDTH;
  c->height = body_vert + 2 * COCOMP_BORDER_WIDTH;
}

PRIVATE int cocomp_initialize(Component *c, gpointer init_data) {
  COCompData *d = safe_malloc(sizeof(COCompData));

  d->name = safe_malloc(strlen("com") + 20);
  sprintf(d->name, "con%d", next_component_number++);


  c->x -= COCOMP_BORDER_WIDTH;
  c->y -= COCOMP_BORDER_WIDTH;
  c->width = c->height = 0;
  c->data = d;


  cocomp_resize(c);

  return 1;
}

PRIVATE void cocomp_destroy(Component *c) {
  COCompData *d = c->data;

  free(d->name);

  free(d);
}

PRIVATE Component *cocomp_clone( Component *c, Sheet *sheet ) {
    COCompData *data = c->data;
    Component *clone = comp_new_component( c->klass, NULL, sheet, 0, 0 );
    COCompData *dstdata = clone->data;
    
    free( dstdata->name );
    dstdata->name = safe_string_dup( data->name );
    cocomp_resize( clone );

    return clone;
}

PRIVATE void cocomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  COCompData *d = safe_malloc(sizeof(COCompData));


  d->name = safe_string_dup( objectstore_item_get_string(item, "name", "was geht denn hier ???" ) );


  c->data = d;
  
  cocomp_resize(c);	/* because things may be different if loading on a different
			   system from the one we saved with */
}

PRIVATE void cocomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  COCompData *d = c->data;
  objectstore_item_set_string(item, "name", d->name);
}

/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
/* XXX: can that be removed for gtk 2 ? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE void cocomp_paint(Component *c, GdkRectangle *area,
			   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  COCompData *d = c->data;
  GdkGC *gc = style->black_gc;
  
  PangoLayout *layout = gtk_widget_create_pango_layout( c->sheet->drawingwidget, d->name );


  gdk_gc_set_foreground(gc, &style->black);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + COCOMP_BORDER_WIDTH,
		     c->y + COCOMP_BORDER_WIDTH,
		     c->width - 2 * COCOMP_BORDER_WIDTH,
		     c->height - 2 * COCOMP_BORDER_WIDTH);
  gdk_gc_set_foreground(gc, &colors[4]);
  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + COCOMP_BORDER_WIDTH,
		     c->y + COCOMP_BORDER_WIDTH,
		     c->width - 2 * COCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * COCOMP_BORDER_WIDTH - 1);

    gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
  gdk_draw_layout(drawable, gc,
		c->x + COCOMP_BORDER_WIDTH + (COCOMP_CONNECTOR_WIDTH>>1),
		c->y + COCOMP_BORDER_WIDTH + (COCOMP_CONNECTOR_WIDTH>>1),
		layout);
  gdk_gc_set_foreground(gc, &style->black);

  g_object_unref( G_OBJECT( layout ) );
}

PRIVATE int cocomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {

  return 0;
}

PRIVATE int cocomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;
  
 if (dx >= COCOMP_BORDER_WIDTH &&
      dy >= COCOMP_BORDER_WIDTH &&
      dx < (c->width - COCOMP_BORDER_WIDTH) &&
      dy < (c->height - COCOMP_BORDER_WIDTH))
    return 1;

  return 0;
}

PRIVATE gboolean cocomp_accept_outbound(Component *c, ConnectorReference *src,
					 ConnectorReference *dst) {

      return FALSE;

}

PRIVATE gboolean cocomp_accept_inbound(Component *c, ConnectorReference *src,
					ConnectorReference *dst) {
  //COCompData *data = c->data;

  return TRUE;
}

PRIVATE gboolean cocomp_unlink_outbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    return FALSE;
}

PRIVATE gboolean cocomp_unlink_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

	return FALSE;
}

PRIVATE char *cocomp_get_title(Component *c) {

  COCompData *d = c->data;
  return safe_string_dup( d->name );
}

PRIVATE char *cocomp_get_connector_name(Component *c, ConnectorReference *ref) {

  COCompData *d = c->data;
  return safe_string_dup( d->name );
}


PRIVATE GtkWidget *rename_text_widget = NULL;

PRIVATE void rename_handler(MsgBoxResponse action_taken, Component *c) {
    if (action_taken == MSGBOX_OK) {
	COCompData *d = c->data;
	GtkTextBuffer *textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(rename_text_widget));
	GtkTextIter start,end;

	free(d->name);

	gtk_text_buffer_get_start_iter( textbuf, &start );
	gtk_text_buffer_get_end_iter( textbuf, &end );
	d->name = safe_string_dup(
		gtk_text_buffer_get_text( textbuf, &start,
						   &end,
						   TRUE
		    ));

	sheet_queue_redraw_component(c->sheet, c);	/* to 'erase' the old size */
	cocomp_resize(c);
	sheet_queue_redraw_component(c->sheet, c);	/* to 'fill in' the new size */
    }
}

PRIVATE void do_rename(Component *c, guint action, GtkWidget *widget) {
    COCompData *d = c->data;
    GtkWidget *hb = gtk_vbox_new(FALSE, 5);
    GtkWidget *label = gtk_label_new("Edit Comment");
    GtkWidget *text = gtk_text_view_new();
    GtkTextBuffer *textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));

    gtk_text_buffer_set_text( textbuf, d->name, strlen( d->name ) );

    gtk_widget_set_usize( text, 200, 200 );

    gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

    gtk_widget_show(label);
    gtk_widget_show(text);

    rename_text_widget = text;
    popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	    (MsgBoxResponseHandler) rename_handler, c);
}

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

PRIVATE GtkWidget *cocomp_build_popup(Component *c) {
  //COCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<cocomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);


  result = gtk_item_factory_get_widget(ifact, "<cocomp-popup>");

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass CommentComponentClass = {
  "cocomp",

  cocomp_initialize,
  cocomp_destroy,
  cocomp_clone,

  cocomp_unpickle,
  cocomp_pickle,

  cocomp_paint,

  cocomp_find_connector_at,
  cocomp_contains_point,

  cocomp_accept_outbound,
  cocomp_accept_inbound,
  cocomp_unlink_outbound,
  cocomp_unlink_inbound,

  cocomp_get_title,
  cocomp_get_connector_name,

  cocomp_build_popup
};


PUBLIC void init_cocomp(void) {
  comp_register_componentclass(&CommentComponentClass);
  comp_add_newmenu_item( "Comment", &CommentComponentClass, NULL );
}

PUBLIC void done_cocomp(void) {
  //g_hash_table_destroy(generatorclasses);
  //generatorclasses = NULL;
}
