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

/* Template gAlan plugin file. Please distribute your plugins according to
   the GNU General Public License! (You don't have to, but I'd prefer it.) */

/* yep GPL apllies */

/* modified to be a scope plugin... requires changes to galan... but only
 * because of me being lazy #include "sample-display.c" should do the job..
 *
 * but because sample display is so powerful i plan to use it for sample input
 * to... (well.. should be in rart.c for loops)
 *
 * missing features:
 *  - display n*SAMPLE_RATE samples
 *  - trigger
 *  - resizing
 *  - you know what a hardware scope can do :-)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#include "sample-display.h"

#define GENERATOR_CLASS_NAME	"sampler"
#define GENERATOR_CLASS_PATH	"Misc/Sampler"
#define GENERATOR_CLASS_PIXMAP	"template.xpm"

#define SIG_INPUT		1
#define SIG_OUTPUT		0

#define EVT_TRIGGER		0
#define EVT_YSCALE		1
#define EVT_XSCALE		2
#define NUM_EVENT_INPUTS	3

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	0


typedef struct Data {
	/* state, to be pickled, unpickled */
  gint32 phase;
  gdouble ysize; /* sizeof(intbuf) = ysize * SAMPLE_RATE */
  gdouble xsize;

  gint32 loop_start, loop_end;

    /* transient the table size is yscale */
  gboolean go;
  gint8  *intbuf;
  SAMPLE *samplebuf;
} Data;

PRIVATE void setup_tables(void) {
  /* Put any plugin-wide initialisation here. */
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {

    /* TODO: the EVENT contains event->d.integer samples.
     *       i convert these samples to gint8 and put them
     *       into data->intbuf[phase++] until phase >= ysize;
     *       first i discard the old data.
     *
     *       when ysize is reached, i draw the buffer.
     *
     */

    Data *data = g->data;

    switch (event->kind)
    {
	case AE_REALTIME:

	    if( data->go ) {
		gint32 oldphase;
		GList *l;
		SAMPLE *buf, *bufX;

		int bufbytes    = event->d.integer * sizeof(SAMPLE);
		int intbufbytes = sizeof(gint8)*(SAMPLE_RATE*data->ysize);

		buf    = safe_malloc(bufbytes);

		if (!gen_read_realtime_input(g, 0, -1, buf, event->d.integer))
		    memset(buf, 0, bufbytes);


		for( bufX=buf,oldphase = data->phase; (data->phase-oldphase)<event->d.integer && (data->phase < intbufbytes); (data->phase)++,bufX++ )
		{
		    data->intbuf[data->phase]=CLIP_SAMPLE(*bufX * data->xsize)*127;
		    data->samplebuf[data->phase]=*bufX;
		}

		if( data->phase >= intbufbytes )
		{	
		    for( l=g_list_first(g->controls); l != NULL; l=g_list_next(l) )
		    {
			Control *controlx = l->data;
			g_assert( controlx->data != NULL );
			sample_display_set_data_8( (SampleDisplay *)controlx->data,
				data->intbuf, intbufbytes, TRUE );
			sample_display_set_loop( (SampleDisplay *)controlx->data, 0, intbufbytes-1 );
		    }
		    data->phase = 0;
		    data->go = FALSE;
		}
		free(buf);
	    }

	    break;


	default:
	    g_warning("scope module doesn't care for events of kind %d.", event->kind);
	    break;
    }
}


PRIVATE gboolean init_instance(Generator *g) {
	/* TODO: need to allocate the intbuf of size yscale */
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->phase = 0;
  data->go = FALSE;
  data->ysize = 0.1;
  data->xsize = 1;

  data->intbuf = safe_malloc( sizeof(gint8)*(SAMPLE_RATE*data->ysize+1));
  data->samplebuf = safe_malloc( sizeof(SAMPLE)*(SAMPLE_RATE*data->ysize+1));
  gen_register_realtime_fn(g, realtime_handler);

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  gen_deregister_realtime_fn(g, realtime_handler);
  free(((Data *)(g->data))->intbuf);
  free(((Data *)(g->data))->samplebuf);
  free(g->data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
	
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->go = FALSE;
  data->phase = objectstore_item_get_integer(item, "scope_phase", 0);
  data->ysize = objectstore_item_get_double(item, "scope_ysize", 0.1);
  data->xsize = objectstore_item_get_double(item, "scope_xsize", 1.0);
  data->intbuf = (gint8 *)safe_malloc( sizeof(gint8)*(SAMPLE_RATE*data->ysize+1));
  data->samplebuf = safe_malloc( sizeof(SAMPLE)*(SAMPLE_RATE*data->ysize+1));
  gen_register_realtime_fn(g, realtime_handler);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "scope_phase", data->phase);
  objectstore_item_set_double(item, "scope_ysize", data->ysize);
  objectstore_item_set_double(item, "scope_xsize", data->xsize);
}

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  Data *data = g->data;
  return data->loop_end - data->loop_start;
}

PRIVATE gboolean output_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int duration = data->loop_end - data->loop_start;
  int len, sil;

  if (duration == 0 || offset >= duration)
    return FALSE;

  len = MIN(MAX(duration - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->samplebuf[offset+data->loop_start], len * sizeof(SAMPLE));

  sil = buflen - len;
  memset(&buf[len], 0, sil * sizeof(SAMPLE));
  return TRUE;
}

PRIVATE void loop_handler( SampleDisplay *s, int start, int end ) {
    Control *c = gtk_object_get_user_data( GTK_OBJECT( s ) );
    Data *data = c->g->data;

    data->loop_start = start;
    data->loop_end = end;
}

PRIVATE void loop_selection_handler( GtkWidget *b, Control *c ) {

    //Data *data = c->g->data;
    SampleDisplay *sc = c->data;

    sample_display_set_loop( sc, sc->sel_start, sc->sel_end );
}

PRIVATE void zoom_selection_handler( GtkWidget *b, Control *c ) {

    //Data *data = c->g->data;
    SampleDisplay *sc = c->data;

    sample_display_set_window( sc, sc->sel_start, sc->sel_end );
}

PRIVATE void zoom_out_handler( GtkWidget *b, Control *c ) {

    Data *data = c->g->data;
    SampleDisplay *sc = c->data;

    sample_display_set_window( sc, MAX( 0, sc->win_start - sc->win_length/2 ),
	    MIN( data->ysize * SAMPLE_RATE - 1, sc->win_start + sc->win_length*2 ));
}
PRIVATE void zoom_in_handler( GtkWidget *b, Control *c ) {

    //Data *data = c->g->data;
    SampleDisplay *sc = c->data;

    sample_display_set_window( sc, sc->win_start + sc->win_length/4, sc->win_start + sc->win_length * 0.75 );
}
PRIVATE void init_scope( Control *control ) {

	GtkWidget *sc, *vb, *hb, *zoom_in, *zoom_out, *set_loop, *sel_loop;

	vb=gtk_vbox_new( 0,FALSE );
	hb=gtk_hbox_new( 0,FALSE );
	g_assert( vb != NULL );

	zoom_in = gtk_button_new_with_label( "+" );
	gtk_signal_connect( GTK_OBJECT( zoom_in ), "clicked", GTK_SIGNAL_FUNC( zoom_in_handler ), control );
	zoom_out = gtk_button_new_with_label( "-" );
	gtk_signal_connect( GTK_OBJECT( zoom_out ), "clicked", GTK_SIGNAL_FUNC( zoom_out_handler ), control );
	sel_loop = gtk_button_new_with_label( "zoom_sel" );
	gtk_signal_connect( GTK_OBJECT( sel_loop ), "clicked", GTK_SIGNAL_FUNC( zoom_selection_handler ), control );
	set_loop = gtk_button_new_with_label( "loop" );
	gtk_signal_connect( GTK_OBJECT( set_loop ), "clicked", GTK_SIGNAL_FUNC( loop_selection_handler ), control );

	sc = sample_display_new(TRUE);
	gtk_widget_set_usize( sc, 250, 100 );
	gtk_object_set_user_data( GTK_OBJECT(sc), control );
	gtk_signal_connect( GTK_OBJECT( sc ), "loop_changed", GTK_SIGNAL_FUNC( loop_handler ), NULL );

	gtk_box_pack_start( GTK_BOX( hb ), zoom_in, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( hb ), zoom_out, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( hb ), sel_loop, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( hb ), set_loop, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( vb ), sc, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX( vb ), hb, TRUE, TRUE, 0 );
	
	gtk_widget_show_all( vb );
	
	control->widget = vb;
	control->data = sc;
}

PRIVATE void done_scope(Control *control) {
}

PRIVATE void refresh_scope(Control *control) {
}


PRIVATE void evt_trigger_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_TRIGGER
   * set phase=0;
   */
  Data *data = g->data;
  data->phase=0;
  data->go = TRUE;
}

PRIVATE void evt_yscale_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_YSCALE
   * when yscale changes need to resize buffer
   * first free, malloc, then be smart
   * i also drop all data in the buffer. could be better...
   * lets see if there is demand..
   */

  Data *data = g->data;

  free( data->intbuf );
  free( data->samplebuf );

  data->ysize = event->d.number;
  data->intbuf = (gint8 *)safe_malloc( sizeof(gint8)*(SAMPLE_RATE*data->ysize+1));
  data->samplebuf = (SAMPLE *)safe_malloc( sizeof(SAMPLE)*(SAMPLE_RATE*data->ysize+1));
  data->phase = 0; 
}

PRIVATE void evt_xscale_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_XSCALE
   * this only changes XSCALE factor which is only used for
   * generating the gint8[] so nothing special here
   */
  Data *data = g->data;
  data->xsize = event->d.number;
}


PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Samples", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, output_generator } } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_USERDEF, "sampledisplay", 0,0,0,0, 0,FALSE, 0,0, init_scope, done_scope, refresh_scope },
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_TRIGGER, "Trigger", evt_trigger_handler);
  gen_configure_event_input(k, EVT_YSCALE, "YSize", evt_yscale_handler);
  gen_configure_event_input(k, EVT_XSCALE, "XSize", evt_xscale_handler);
  //gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
				  NULL);
}

PUBLIC void init_plugin_sampler(void) {
  setup_tables();
  setup_class();
}
