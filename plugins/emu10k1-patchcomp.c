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
 * emupatchcomp only means a reference to a connector therefor
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
#include "gencomp.h"


#include "emu10k1-include/dsp.h"

#define EMUPATCHCOMP_ICONLENGTH	48
#define EMUPATCHCOMP_TITLEHEIGHT	15
#define EMUPATCHCOMP_CONNECTOR_SPACE	5
#define EMUPATCHCOMP_CONNECTOR_WIDTH	10
#define EMUPATCHCOMP_BORDER_WIDTH	(EMUPATCHCOMP_CONNECTOR_WIDTH + EMUPATCHCOMP_CONNECTOR_SPACE)


typedef struct EmuPatchCompInitData {
    GeneratorClass *gc;
    char *patchname;
    char *genname;
    char *patchpath;
    int anzinputevents;
    char *gprnames[5];
} EmuPatchCompInitData;

typedef struct EmuPatchCompData {
    char *name;
    char *patchpath;
    int anzinputevents;
    ConnectorReference *input, *output;
    gboolean connected;

    Generator *g;
    char *id_tag;
    EmuPatchCompInitData *id;
} EmuPatchCompData;

// Private Variables:
PRIVATE int next_component_number = 0;
PRIVATE GHashTable *init_datas = NULL;

// Prototypes:
PRIVATE void emupatch_gen_event_handler( Generator *g, AEvent *ev );
extern struct dsp_patch_manager *get_patch_manager( void );
PRIVATE EmuPatchCompInitData *get_init_data( char *name );
PRIVATE void emupatchcomp_load_patches( Component *c );


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
  int startpos = is_outbound ? (EMUPATCHCOMP_BORDER_WIDTH * 2
				+ (is_signal ? hsize : vsize)
				- (EMUPATCHCOMP_CONNECTOR_WIDTH >> 1))
			     : (EMUPATCHCOMP_CONNECTOR_WIDTH >> 1);
  int x = is_signal ? startpos : EMUPATCHCOMP_BORDER_WIDTH + spacing;
  int y = is_signal ? EMUPATCHCOMP_BORDER_WIDTH + spacing : startpos;
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

PRIVATE void emupatchcomp_resize(Component *c) {
  int body_vert, body_horiz;
  EmuPatchCompData *d = c->data;

  int in_sig_count=0, out_sig_count=0, in_count=0;
  gboolean icon=FALSE;
  
  out_sig_count=1;
  in_sig_count=1;
  in_count=d->anzinputevents;

  body_vert =
    EMUPATCHCOMP_CONNECTOR_WIDTH
    + MAX(MAX(in_sig_count, out_sig_count) * EMUPATCHCOMP_CONNECTOR_WIDTH,
	  EMUPATCHCOMP_TITLEHEIGHT + (icon ? EMUPATCHCOMP_ICONLENGTH : 0));
  body_horiz =
    EMUPATCHCOMP_CONNECTOR_WIDTH
    + MAX(2, sheet_get_textwidth(c->sheet, d->name) );

  resize_connectors(c, in_sig_count, 0, 1, body_horiz, body_vert);
  resize_connectors(c, out_sig_count, 1, 1, body_horiz, body_vert);
  resize_connectors(c, in_count, 0, 0, body_horiz, body_vert);

  c->width = body_horiz + 2 * EMUPATCHCOMP_BORDER_WIDTH;
  c->height = body_vert + 2 * EMUPATCHCOMP_BORDER_WIDTH;
}

PRIVATE int emupatchcomp_initialize(Component *c, gpointer init_data) {

  EmuPatchCompData *d = safe_malloc(sizeof(EmuPatchCompData));
  EmuPatchCompInitData *id = (EmuPatchCompInitData *) init_data;

  d->name = safe_malloc( strlen( id->patchname ) + 10 );
  sprintf( d->name, "%s%d", id->patchname, next_component_number++ );
  d->patchpath = safe_string_dup( id->patchpath );
  d->anzinputevents = id->anzinputevents;
  d->input = d->output = NULL;
  d->id = init_data;
  d->id_tag = id->patchname;
  d->connected = FALSE;

  build_connectors(c, id->anzinputevents, 0, 0 );
  build_connectors(c, 1, 1, 1);
  build_connectors(c, 1, 0, 1);
  

  c->x -= EMUPATCHCOMP_BORDER_WIDTH;
  c->y -= EMUPATCHCOMP_BORDER_WIDTH;
  c->width = c->height = 0;

  // Init the generator...

  /**
   * i think i will need multiple inheritance here in the future.
   * ich habe hier dann keinen generator mehr, wenn ich die generatoren
   * aus dem prozess in einen anderen migriere.
   *
   * das wird ne voellig neue klasse.
   * eigentlich sollten ladspa plugins auch ne andere klasse sein, da sie sich anders (besser)
   * verbinden lassen.
   *
   * diese klassen haben dann verschiedene signal typen, die sich miteinander ueber adapter
   * verbinden lassen. Diese adapter stehen in einer typen matrix und koennen so einfach gefunden
   * werden.
   *
   * ein adapter hat jeweils eine instanz auf beiden typen graphen. die er miteinander verknuepft.
   * adapter von CPU nach dsp ist eine kombo aus pcm_out und emu_input
  */
  d->g = gen_new_generator( id->gc, d->name );
  d->g->data = c;

  c->data = d;


  emupatchcomp_resize(c);

  return 1;
}

PRIVATE void emupatchcomp_destroy(Component *c) {
  EmuPatchCompData *d = c->data;

  gen_kill_generator( d->g );

  free(d->name);

  free(d);
}


PRIVATE void emupatchcomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
    
  ObjectStoreItem *it;
  EmuPatchCompData *d = safe_malloc(sizeof(EmuPatchCompData));
  //g_print( "data = %d\n", d );

  d->name = safe_string_dup( objectstore_item_get_string(item, "patch_name", "what ???" ) );
  d->patchpath = safe_string_dup( objectstore_item_get_string(item, "patch_patchpath", "what ???" ) );
  d->anzinputevents = objectstore_item_get_integer(item, "patch_anzinputevents", 0 );

  c->data = d;

  d->g = gen_unpickle( objectstore_item_get_object( item, "patch_gen" ) );
  d->g->data = c;

  d->input = NULL;
  d->output = NULL;
  it = objectstore_item_get_object( item, "patch_input" );
  if( it )
      d->input = unpickler_for_connectorreference( it );

  it = objectstore_item_get_object( item, "patch_output" );
  if( it )
      d->output = unpickler_for_connectorreference( it );
  else 
  
  d->connected = objectstore_item_get_integer( item, "patch_connected", 0 );
  d->connected = FALSE;

  d->id_tag = safe_string_dup( objectstore_item_get_string(item, "patch_id_tag", "flanger" ) );

  d->id = get_init_data( d->id_tag );


  //d->gc = gen_new_generatorclass( d->name, FALSE, d->anzinputevents, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
  //g_print( "gc: %x (%s)\n", d->gc, d->name );
  //for( i=0; i<d->anzinputevents; i++ )
  //    gen_configure_event_input( d->gc, i, d->id->gprnames[i], emupatch_gen_event_handler ); 

  //g_print( "g: %x, gc:%x\n", d->g, d->id->gc );

  
  //validate_connectors(c);
  emupatchcomp_resize(c);	/* because things may be different if loading on a different
				   system from the one we saved with */
  emupatchcomp_load_patches( c );
}

PRIVATE void emupatchcomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  EmuPatchCompData *d = c->data;

  objectstore_item_set_string(item, "patch_name", d->name);
  objectstore_item_set_string(item, "patch_patchpath", d->patchpath);
  objectstore_item_set_integer(item, "patch_anzinputevents", d->anzinputevents);
  if( d->input )
      objectstore_item_set_object(item, "patch_input", pickle_connectorreference( d->input, db ) );
  if( d->output )
      objectstore_item_set_object(item, "patch_output", pickle_connectorreference( d->output, db ) );
  objectstore_item_set_object( item, "patch_gen", gen_pickle( d->g, db ) );
  objectstore_item_set_integer(item, "patch_connected", d->connected);
  objectstore_item_set_string(item, "patch_id_tag", d->id_tag);
}

/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE void emupatchcomp_paint(Component *c, GdkRectangle *area,
			   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  EmuPatchCompData *d = c->data;
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
		 con->x + c->x - (EMUPATCHCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (EMUPATCHCOMP_CONNECTOR_WIDTH>>1),
		 EMUPATCHCOMP_CONNECTOR_WIDTH,
		 EMUPATCHCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);
    gdk_draw_arc(drawable, style->white_gc, FALSE,
		 con->x + c->x - (EMUPATCHCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (EMUPATCHCOMP_CONNECTOR_WIDTH>>1),
		 EMUPATCHCOMP_CONNECTOR_WIDTH,
		 EMUPATCHCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);

    x = ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->x - (EMUPATCHCOMP_CONNECTOR_SPACE + (EMUPATCHCOMP_CONNECTOR_WIDTH>>1))
	    : con->x + (EMUPATCHCOMP_CONNECTOR_WIDTH>>1))
	 : con->x) + c->x;

    y = ((con->ref.kind == COMP_EVENT_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->y - (EMUPATCHCOMP_CONNECTOR_SPACE + (EMUPATCHCOMP_CONNECTOR_WIDTH>>1))
	    : con->y + (EMUPATCHCOMP_CONNECTOR_WIDTH>>1))
	 : con->y) + c->y;

    gdk_draw_line(drawable, style->white_gc,
		  x, y,
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? x + EMUPATCHCOMP_CONNECTOR_SPACE : x),
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? y : y + EMUPATCHCOMP_CONNECTOR_SPACE));

    l = g_list_next(l);
  }

  gdk_gc_set_foreground(gc, &style->black);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + EMUPATCHCOMP_BORDER_WIDTH,
		     c->y + EMUPATCHCOMP_BORDER_WIDTH,
		     c->width - 2 * EMUPATCHCOMP_BORDER_WIDTH,
		     c->height - 2 * EMUPATCHCOMP_BORDER_WIDTH);
  gdk_gc_set_foreground(gc, &colors[4]);
  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + EMUPATCHCOMP_BORDER_WIDTH,
		     c->y + EMUPATCHCOMP_BORDER_WIDTH,
		     c->width - 2 * EMUPATCHCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * EMUPATCHCOMP_BORDER_WIDTH - 1);
  gdk_gc_set_foreground(gc, &style->black);

  gdk_draw_text(drawable, gtk_style_get_font(style), style->white_gc,
		c->x + EMUPATCHCOMP_BORDER_WIDTH + (EMUPATCHCOMP_CONNECTOR_WIDTH>>1),
		c->y + EMUPATCHCOMP_BORDER_WIDTH + EMUPATCHCOMP_TITLEHEIGHT - 3,
		d->name, strlen(d->name));

  if (NULL != NULL)
    gdk_draw_pixmap(drawable, gc, NULL, 0, 0,
		    c->x + (c->width>>1) - (EMUPATCHCOMP_ICONLENGTH>>1),
		    c->y + ((c->height-EMUPATCHCOMP_TITLEHEIGHT)>>1) - (EMUPATCHCOMP_ICONLENGTH>>1)
		         + EMUPATCHCOMP_TITLEHEIGHT,
		    EMUPATCHCOMP_ICONLENGTH,
		    EMUPATCHCOMP_ICONLENGTH);
}

PRIVATE int emupatchcomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {
  GList *l = c->connectors;

  x -= c->x;
  y -= c->y;

  while (l != NULL) {
    Connector *con = l->data;

    if (ABS(x - con->x) < (EMUPATCHCOMP_CONNECTOR_WIDTH>>1) &&
	ABS(y - con->y) < (EMUPATCHCOMP_CONNECTOR_WIDTH>>1)) {
      if (ref != NULL)
	*ref = con->ref;
      return 1;
    }

    l = g_list_next(l);
  }

  return 0;
}

PRIVATE int emupatchcomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;

  if (dx >= EMUPATCHCOMP_BORDER_WIDTH &&
      dy >= EMUPATCHCOMP_BORDER_WIDTH &&
      dx < (c->width - EMUPATCHCOMP_BORDER_WIDTH) &&
      dy < (c->height - EMUPATCHCOMP_BORDER_WIDTH))
    return 1;

  return emupatchcomp_find_connector_at(c, x, y, NULL);
}

PRIVATE void emupatch_gen_event_handler( Generator *g, AEvent *ev ) {
    Component *c = g->data;
    EmuPatchCompData *d = c->data;

    if( d->connected )
	dsp_set_control_gpr_value( get_patch_manager(), d->name, d->id->gprnames[ev->dst_q], (int) ((ev->d.number) * ((gdouble)0x7fffffff)));
    
}

PRIVATE int emupatchcomp_find_io_queue( ConnectorReference *ref, gboolean dir ) {

    ConnectorReference *tmp = ref;

    while( tmp && strcmp(tmp->c->klass->class_tag, "emuiocomp") )
    {
	EmuPatchCompData *d = tmp->c->data;
	if( !d )
	    return -1;

	tmp = dir ? d->output : d->input;
    }

    if( tmp )
	return tmp->queue_number;
    else 
	return -1;
}

PRIVATE void emupatchcomp_load_patches( Component *c ) {

    EmuPatchCompData *data = c->data;
    int input  = emupatchcomp_find_io_queue( data->input, FALSE );
    int output = emupatchcomp_find_io_queue( data->output, TRUE );
    ConnectorReference *last = &(((Connector *)c->connectors->data)->ref), *tmp = data->input;

    if( input<0 || output<0 )
	return;

    dsp_add_route( get_patch_manager(), input, output );
    dsp_load( get_patch_manager() );
    g_print( "Added a route from %d to %d\n", input, output );

    while( tmp && strcmp(tmp->c->klass->class_tag, "emuiocomp") )
    {
	EmuPatchCompData *d = tmp->c->data;
	last = tmp;
	tmp = d->input;
    }

    tmp = last;
    while( tmp && strcmp(tmp->c->klass->class_tag, "emuiocomp") )
    {
	EmuPatchCompData *d = tmp->c->data;

	g_print( "Load patch %s,%d,%d returns %d\n", d->patchpath, input, output, 
	dsp_read_patch( get_patch_manager(), d->patchpath, &input, &input, 1,1,1,d->name,0 ) );
	
	d->connected = TRUE;

	tmp = d->output;
    }

    dsp_load( get_patch_manager() );

    
}

PRIVATE void emupatchcomp_unload_patches( Component *c ) {

    EmuPatchCompData *data = c->data;
    int input  = emupatchcomp_find_io_queue( data->input, FALSE );
    int output = emupatchcomp_find_io_queue( data->output, TRUE );
    ConnectorReference *last = &(((Connector *)c->connectors->data)->ref), *tmp = data->output;

    if( input<0 || output<0 )
	return;


    while( tmp && strcmp(tmp->c->klass->class_tag, "emuiocomp") )
    {
	EmuPatchCompData *d = tmp->c->data;
	last = tmp;
	tmp = d->output;
    }

    tmp = last;
    while( tmp && strcmp(tmp->c->klass->class_tag, "emuiocomp") )
    {
	EmuPatchCompData *d = tmp->c->data;

	g_print( "UnLoad patch %s returns %d\n", d->name, 
	dsp_unload_patch( get_patch_manager(), d->name ) );
	
	d->connected = TRUE;

	tmp = d->input;
    }

    dsp_load( get_patch_manager() );

    dsp_del_route( get_patch_manager(), input, output );
    dsp_load( get_patch_manager() );
    g_print( "Remove route from %d to %d\n", input, output );
    
}

PRIVATE gboolean emupatchcomp_accept_outbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    EmuPatchCompData *data = c->data;

    if( src->kind == COMP_SIGNAL_CONNECTOR ) {

	if( data->output )
	    return FALSE;


	if( !strcmp(dst->c->klass->class_tag,"emuiocomp" ) ) {

	    data->output = safe_malloc( sizeof( ConnectorReference ) );
	    *(data->output) = *dst;
	    emupatchcomp_load_patches( c );
	    return TRUE;

	} else if( !strcmp(dst->c->klass->class_tag,"emupatchcomp" ) ) {

	    data->output = safe_malloc( sizeof( ConnectorReference ) );
	    *(data->output) = *(dst);
	    emupatchcomp_load_patches( c );
	    return TRUE;
	}

	return FALSE;
    }
    return FALSE;
}

PRIVATE gboolean emupatchcomp_accept_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    EmuPatchCompData *data = c->data;

    if( dst->kind == COMP_SIGNAL_CONNECTOR ) {

	if( !strcmp(src->c->klass->class_tag,"emuiocomp" ) ) {

	    data->input = safe_malloc( sizeof( ConnectorReference ) );
	    *(data->input) = *src;
	    emupatchcomp_load_patches( c );
	    return TRUE;

	} else if( !strcmp(src->c->klass->class_tag,"emupatchcomp" ) ) {

	    data->input = safe_malloc( sizeof( ConnectorReference ) );
	    *(data->input) = *src;
	    emupatchcomp_load_patches( c );
	    return TRUE;
	}

	return FALSE;
    } else if( !strcmp( src->c->klass->class_tag, "gencomp" ) ) {
	GenCompData *otherdata = src->c->data;
	return (gen_link( FALSE, otherdata->g, src->queue_number, data->g, dst->queue_number ) != NULL);
    } else
	return TRUE;
}

PRIVATE gboolean emupatchcomp_unlink_outbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {
  EmuPatchCompData *data = c->data;
  emupatchcomp_unload_patches( c );
  free( data->output );
  data->output = NULL;

  return TRUE;
}

PRIVATE gboolean emupatchcomp_unlink_inbound(Component *c, ConnectorReference *src, ConnectorReference *dst) {

    EmuPatchCompData *data = c->data;
    if( dst->kind == COMP_SIGNAL_CONNECTOR ) {
	emupatchcomp_unload_patches( c );
	free( data->input );
	data->input = NULL;
    } else {
	GenCompData *otherdata = src->c->data;
	gen_unlink( gen_find_link(FALSE, otherdata->g, src->queue_number, data->g, dst->queue_number) );

    }

    return TRUE;
}

PRIVATE char *emupatchcomp_get_title(Component *c) {

  EmuPatchCompData *d = c->data;
  return safe_string_dup( d->name );
}

PRIVATE char *emupatchcomp_get_connector_name(Component *c, ConnectorReference *ref) {

  EmuPatchCompData *d = c->data;
  GeneratorClass *k = d->id->gc;
  
  if( (ref->kind == COMP_EVENT_CONNECTOR) && ( !ref->is_output ) )
      return safe_string_dup( k->in_names[ref->queue_number] );
  else
      return safe_string_dup( ref->is_output ? "Output" : "Input" );
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

PRIVATE GtkWidget *emupatchcomp_build_popup(Component *c) {
  //EmuPatchCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<emupatchcomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);


  result = gtk_item_factory_get_widget(ifact, "<emupatchcomp-popup>");

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

PRIVATE ComponentClass EmuPatchComponentClass = {
  "emupatchcomp",

  emupatchcomp_initialize,
  emupatchcomp_destroy,

  emupatchcomp_unpickle,
  emupatchcomp_pickle,

  emupatchcomp_paint,

  emupatchcomp_find_connector_at,
  emupatchcomp_contains_point,

  emupatchcomp_accept_outbound,
  emupatchcomp_accept_inbound,
  emupatchcomp_unlink_outbound,
  emupatchcomp_unlink_inbound,

  emupatchcomp_get_title,
  emupatchcomp_get_connector_name,

  emupatchcomp_build_popup
};

EmuPatchCompInitData id_eq = { NULL, "5band-eq", "emu10k1-5bandeq","/usr/local/share/emu10k1/5band-eq.bin", 5,
    { "F_100Hz", "F_316Hz", "F_1000Hz", "F_3160Hz", "F_10000Hz" } };
EmuPatchCompInitData id_flanger = { NULL, "flanger", "emu10k1-flanger", "/usr/local/share/emu10k1/flanger.bin", 5,
    { "speed", "delay", "width", "forward", "feedback" } };
EmuPatchCompInitData id_chorus = { NULL, "chorus", "emu10k1-chorus", "/usr/local/share/emu10k1/chorus.bin", 4,
    { "speed", "delay", "width", "mix", NULL } };
EmuPatchCompInitData id_vibro = { NULL, "vibro", "emu10k1-vibro", "/usr/local/share/emu10k1/vibrato.bin", 2,
    { "delta", "depth", NULL, NULL, NULL } };
EmuPatchCompInitData id_vol = { NULL, "vol", "emu10k1-vol", "/usr/local/share/emu10k1/vol.bin", 1,
    { "Vol", NULL, NULL, NULL, NULL } };

PRIVATE void register_init_data( EmuPatchCompInitData *id ) {

    int i;
    
    g_hash_table_insert(init_datas, id->patchname, id);
    
    id->gc = gen_new_generatorclass( id->genname, FALSE, id->anzinputevents, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
    for( i=0; i<id->anzinputevents; i++ )
	gen_configure_event_input( id->gc, i, id->gprnames[i], emupatch_gen_event_handler ); 
}

PRIVATE EmuPatchCompInitData *get_init_data( char *name ) {
    return (EmuPatchCompInitData *) g_hash_table_lookup(init_datas, name);
}

PUBLIC void init_emupatchcomp(void) {

  comp_register_componentclass(&EmuPatchComponentClass);
  init_datas = g_hash_table_new(g_str_hash, g_str_equal);
  
  register_init_data( &id_eq );
  comp_add_newmenu_item( "Emu10k1/5 Band EQ", &EmuPatchComponentClass, &id_eq );

  register_init_data( &id_flanger );
  comp_add_newmenu_item( "Emu10k1/Flanger", &EmuPatchComponentClass, &id_flanger );

  register_init_data( &id_chorus );
  comp_add_newmenu_item( "Emu10k1/Chorus", &EmuPatchComponentClass, &id_chorus );

  register_init_data( &id_vibro );
  comp_add_newmenu_item( "Emu10k1/Vibro Effect", &EmuPatchComponentClass, &id_vibro );
  
  register_init_data( &id_vol );
  comp_add_newmenu_item( "Emu10k1/Volume", &EmuPatchComponentClass, &id_vol );
}

PUBLIC void done_emupatchcomp(void) {
}
