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
 * emuiocomp only means a reference to a connector therefor
 * it only stores a ConnectorReference.
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "sheet.h"
#include "msgbox.h"
#include "control.h"

#include "emu10k1-include/dsp.h"

#define EMUIOCOMP_ICONLENGTH	48
#define EMUIOCOMP_TITLEHEIGHT	15
#define EMUIOCOMP_CONNECTOR_SPACE	5
#define EMUIOCOMP_CONNECTOR_WIDTH	10
#define EMUIOCOMP_BORDER_WIDTH	(EMUIOCOMP_CONNECTOR_WIDTH + EMUIOCOMP_CONNECTOR_SPACE)

typedef enum EmuIOType {
    EMU_INPUT = 0,
    EMU_OUTPUT
} EmuIOType;

typedef struct EmuIOCompData {
    EmuIOType type;
    int anzconnectors;
    struct dsp_patch_manager *mgr;
    char *name;
} EmuIOCompData;

typedef struct EmuIOCompInitData {
    EmuIOType type;
} EmuIOCompInitData;

PRIVATE struct dsp_patch_manager *patch_mgr = NULL;

PUBLIC struct dsp_patch_manager *get_patch_manager( void ) {

    if( patch_mgr == NULL )
    {
	patch_mgr = safe_malloc( sizeof( struct dsp_patch_manager ) );
	patch_mgr->mixer_fd = open( "/dev/mixer", O_RDWR, 0 );
	patch_mgr->dsp_fd = open( "/dev/dsp", O_RDWR, 0 );
	dsp_init( patch_mgr );
	dsp_unload_all( patch_mgr );
	dsp_load( patch_mgr );
    }
    
    return patch_mgr;

    
//    int inputs[2] = {0,1};
//    int outputs[2] = {0,1};
//    dsp_add_route( &mgr, 0, 0 );
//    dsp_add_route( &mgr, 1, 1 );
//    dsp_read_patch( &mgr, "/usr/local/share/emu10k1/chorus_2.bin", inputs, outputs, 2,2,1,"brr",0 );
}

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
  int startpos = is_outbound ? (EMUIOCOMP_BORDER_WIDTH * 2
				+ (is_signal ? hsize : vsize)
				- (EMUIOCOMP_CONNECTOR_WIDTH >> 1))
			     : (EMUIOCOMP_CONNECTOR_WIDTH >> 1);
  int x = is_signal ? startpos : EMUIOCOMP_BORDER_WIDTH + spacing;
  int y = is_signal ? EMUIOCOMP_BORDER_WIDTH + spacing : startpos;
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

PRIVATE void emuiocomp_resize(Component *c) {
  int body_vert, body_horiz;
  EmuIOCompData *d = c->data;

  int in_sig_count=0, out_sig_count=0;
  gboolean icon=FALSE;
  
  switch( d->type ) {
      case EMU_INPUT:
	  out_sig_count=d->anzconnectors;
	  break;
      case EMU_OUTPUT:
	  in_sig_count=d->anzconnectors;
	  break;
  }

  body_vert =
    EMUIOCOMP_CONNECTOR_WIDTH
    + MAX(MAX(in_sig_count, out_sig_count) * EMUIOCOMP_CONNECTOR_WIDTH,
	  EMUIOCOMP_TITLEHEIGHT + (icon ? EMUIOCOMP_ICONLENGTH : 0));
  body_horiz =
    EMUIOCOMP_CONNECTOR_WIDTH
    + MAX(2, sheet_get_textwidth(c->sheet, d->name) );

  resize_connectors(c, in_sig_count, 0, 1, body_horiz, body_vert);
  resize_connectors(c, out_sig_count, 1, 1, body_horiz, body_vert);

  c->width = body_horiz + 2 * EMUIOCOMP_BORDER_WIDTH;
  c->height = body_vert + 2 * EMUIOCOMP_BORDER_WIDTH;
}

PRIVATE int emuiocomp_initialize(Component *c, gpointer init_data) {
  EmuIOCompData *d = safe_malloc(sizeof(EmuIOCompData));
  EmuIOCompInitData *id = (EmuIOCompInitData *) init_data;

  d->name = (id->type == EMU_INPUT ? "emu_inputs" : "emu_outputs");
  d->type = id->type;

  d->anzconnectors = (id->type == EMU_INPUT ? DSP_NUM_INPUTS : DSP_NUM_OUTPUTS);

  switch( d->type ) {
      case EMU_INPUT:
	  build_connectors(c, d->anzconnectors, 1, 1);
	  break;
      case EMU_OUTPUT:
	  build_connectors(c, d->anzconnectors, 0, 1);
	  break;
  }

  c->x -= EMUIOCOMP_BORDER_WIDTH;
  c->y -= EMUIOCOMP_BORDER_WIDTH;
  c->width = c->height = 0;
  c->data = d;


  emuiocomp_resize(c);

  return 1;
}

PRIVATE void emuiocomp_destroy(Component *c) {
  EmuIOCompData *d = c->data;

  //FIXME: Name is const string shall i change that ???
  //free(d->name);

  free(d);
}

PRIVATE void emuiocomp_update_routes( Component *c ) {

    GList *conlist = c->connectors;
    while( conlist ) {
	Connector *con = conlist->data;
	GList *reflist = con->refs;
	while( reflist ) {
	    ConnectorReference *ref = reflist->data;
	    dsp_add_route( get_patch_manager(), ref->queue_number, con->ref.queue_number );
	    reflist = g_list_next( reflist );
	}

	conlist = g_list_next( conlist );
    }
    dsp_load( get_patch_manager() );
}

PRIVATE void emuiocomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  EmuIOCompData *d = safe_malloc(sizeof(EmuIOCompData));

  d->type = objectstore_item_get_integer(item, "type", EMU_INPUT );
  d->name = d->type == EMU_INPUT ? "emu_inputs" : "emu_outputs";
  d->anzconnectors = d->type == EMU_INPUT ? DSP_NUM_INPUTS : DSP_NUM_OUTPUTS;


  c->data = d;
  //validate_connectors(c);
  if( d->type == EMU_OUTPUT )
      emuiocomp_update_routes(c);
  emuiocomp_resize(c);	/* because things may be different if loading on a different
			   system from the one we saved with */
}

PRIVATE void emuiocomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  EmuIOCompData *d = c->data;
  objectstore_item_set_integer(item, "type", d->type);
}

/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE void emuiocomp_paint(Component *c, GdkRectangle *area,
			   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  EmuIOCompData *d = c->data;
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
//      colidx = (con->refs == NULL) ? COMP_COLOR_BLUE : COMP_COLOR_GREEN;

    colidx = COMP_COLOR_BLUE;
    
    gdk_gc_set_foreground(gc, &colors[colidx]);
    gdk_draw_arc(drawable, gc, TRUE,
		 con->x + c->x - (EMUIOCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (EMUIOCOMP_CONNECTOR_WIDTH>>1),
		 EMUIOCOMP_CONNECTOR_WIDTH,
		 EMUIOCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);
    gdk_draw_arc(drawable, style->white_gc, FALSE,
		 con->x + c->x - (EMUIOCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (EMUIOCOMP_CONNECTOR_WIDTH>>1),
		 EMUIOCOMP_CONNECTOR_WIDTH,
		 EMUIOCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);

    x = ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->x - (EMUIOCOMP_CONNECTOR_SPACE + (EMUIOCOMP_CONNECTOR_WIDTH>>1))
	    : con->x + (EMUIOCOMP_CONNECTOR_WIDTH>>1))
	 : con->x) + c->x;

    y = ((con->ref.kind == COMP_EVENT_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->y - (EMUIOCOMP_CONNECTOR_SPACE + (EMUIOCOMP_CONNECTOR_WIDTH>>1))
	    : con->y + (EMUIOCOMP_CONNECTOR_WIDTH>>1))
	 : con->y) + c->y;

    gdk_draw_line(drawable, style->white_gc,
		  x, y,
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? x + EMUIOCOMP_CONNECTOR_SPACE : x),
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? y : y + EMUIOCOMP_CONNECTOR_SPACE));

    l = g_list_next(l);
  }

  gdk_gc_set_foreground(gc, &style->black);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + EMUIOCOMP_BORDER_WIDTH,
		     c->y + EMUIOCOMP_BORDER_WIDTH,
		     c->width - 2 * EMUIOCOMP_BORDER_WIDTH,
		     c->height - 2 * EMUIOCOMP_BORDER_WIDTH);
  gdk_gc_set_foreground(gc, &colors[4]);
  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + EMUIOCOMP_BORDER_WIDTH,
		     c->y + EMUIOCOMP_BORDER_WIDTH,
		     c->width - 2 * EMUIOCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * EMUIOCOMP_BORDER_WIDTH - 1);
  gdk_gc_set_foreground(gc, &style->black);

  gdk_draw_text(drawable, gtk_style_get_font(style), style->white_gc,
		c->x + EMUIOCOMP_BORDER_WIDTH + (EMUIOCOMP_CONNECTOR_WIDTH>>1),
		c->y + EMUIOCOMP_BORDER_WIDTH + EMUIOCOMP_TITLEHEIGHT - 3,
		d->name, strlen(d->name));

  if (NULL != NULL)
    gdk_draw_pixmap(drawable, gc, NULL, 0, 0,
		    c->x + (c->width>>1) - (EMUIOCOMP_ICONLENGTH>>1),
		    c->y + ((c->height-EMUIOCOMP_TITLEHEIGHT)>>1) - (EMUIOCOMP_ICONLENGTH>>1)
		         + EMUIOCOMP_TITLEHEIGHT,
		    EMUIOCOMP_ICONLENGTH,
		    EMUIOCOMP_ICONLENGTH);
}

PRIVATE int emuiocomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {
  GList *l = c->connectors;

  x -= c->x;
  y -= c->y;

  while (l != NULL) {
    Connector *con = l->data;

    if (ABS(x - con->x) < (EMUIOCOMP_CONNECTOR_WIDTH>>1) &&
	ABS(y - con->y) < (EMUIOCOMP_CONNECTOR_WIDTH>>1)) {
      if (ref != NULL)
	*ref = con->ref;
      return 1;
    }

    l = g_list_next(l);
  }

  return 0;
}

PRIVATE int emuiocomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;

  if (dx >= EMUIOCOMP_BORDER_WIDTH &&
      dy >= EMUIOCOMP_BORDER_WIDTH &&
      dx < (c->width - EMUIOCOMP_BORDER_WIDTH) &&
      dy < (c->height - EMUIOCOMP_BORDER_WIDTH))
    return 1;

  return emuiocomp_find_connector_at(c, x, y, NULL);
}

PRIVATE gboolean emuiocomp_accept_outbound(Component *c, ConnectorReference *src,
					 ConnectorReference *dst) {
  //EmuIOCompData *data = c->data;

  if( strcmp(dst->c->klass->class_tag, "emupatchcomp") &&  strcmp(dst->c->klass->class_tag, "emuiocomp") )
      return FALSE;

  return TRUE;
}

PRIVATE gboolean emuiocomp_accept_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    //EmuIOCompData *data = c->data;

    if( !strcmp(src->c->klass->class_tag, "emupatchcomp") ) {
	return TRUE;
    }


    if( !strcmp(src->c->klass->class_tag, "emuiocomp") ) {

	gboolean res;
	res = (dsp_add_route( get_patch_manager(), src->queue_number, dst->queue_number ) == 0);
	dsp_load( get_patch_manager() );
	return res;
    }

    return FALSE;
}

PRIVATE gboolean emuiocomp_unlink_outbound(Component *c, ConnectorReference *src,
				     ConnectorReference *dst) {
  //EmuIOCompData *data = c->data;
  return TRUE;
}

PRIVATE gboolean emuiocomp_unlink_inbound(Component *c, ConnectorReference *src,
				    ConnectorReference *dst) {
  //EmuIOCompData *data = c->data;
  dsp_del_route( get_patch_manager(), src->queue_number, dst->queue_number );
  dsp_load( get_patch_manager() );

  return TRUE;
}

PRIVATE char *emuiocomp_get_title(Component *c) {

  EmuIOCompData *d = c->data;
  return safe_string_dup( d->name );
}

extern char dsp_in_name[][];
extern char dsp_out_name[][];
PRIVATE char *emuiocomp_get_connector_name(Component *c, ConnectorReference *ref) {

  EmuIOCompData *d = c->data;
  if( d->type == EMU_INPUT ) 
      return safe_string_dup( dsp_in_name[ref->queue_number] );
  else
      return safe_string_dup( dsp_out_name[ref->queue_number] );
}

PRIVATE void do_delete(Component *c, guint action, GtkWidget *widget) {
  sheet_delete_component(c->sheet, c);
}

PRIVATE GtkItemFactoryEntry popup_items[] = {
  { "/_Delete",		NULL,	do_delete, 0,		NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

PRIVATE GtkWidget *emuiocomp_build_popup(Component *c) {
  //EmuIOCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<emuiocomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);


  result = gtk_item_factory_get_widget(ifact, "<emuiocomp-popup>");

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass EmuIOComponentClass = {
  "emuiocomp",

  emuiocomp_initialize,
  emuiocomp_destroy,

  emuiocomp_unpickle,
  emuiocomp_pickle,

  emuiocomp_paint,

  emuiocomp_find_connector_at,
  emuiocomp_contains_point,

  emuiocomp_accept_outbound,
  emuiocomp_accept_inbound,
  emuiocomp_unlink_outbound,
  emuiocomp_unlink_inbound,

  emuiocomp_get_title,
  emuiocomp_get_connector_name,

  emuiocomp_build_popup
};

EmuIOCompInitData id_input = { EMU_INPUT };
EmuIOCompInitData id_output = { EMU_OUTPUT };


PUBLIC void init_emuiocomp(void) {

  comp_register_componentclass(&EmuIOComponentClass);
  comp_add_newmenu_item( "Emu10k1/Inputs", &EmuIOComponentClass, &id_input );
  comp_add_newmenu_item( "Emu10k1/Outputs", &EmuIOComponentClass, &id_output );
}

PUBLIC void done_emuiocomp(void) {
}
