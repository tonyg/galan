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
#include <stddef.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#include "gtkknob.h"

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"

#define GENERATOR_CLASS_NAME	"adsr"
#define GENERATOR_CLASS_PATH	"Levels/ADSR (lin-exp-const-lin) Envelope"

#define SIG_OUTPUT		0

#define EVT_ATTACK_TIME		0
#define EVT_DECAY_HALFTIME	1
#define EVT_SUSTAIN_TIME	2
#define EVT_RELEASE_TIME	3
#define EVT_SUSTAIN_LEVEL	4
#define NUM_EVENT_INPUTS	5

#define NUM_EVENT_OUTPUTS	0

typedef struct Data {
  /* Authoritative parameters */
  gdouble attack, decay, sustain, release;	/* times in seconds */
  SAMPLE sustain_level;

  /* Pre-calculated for convenience */
  SAMPLETIME duration;
  SAMPLETIME sustain_duration;
  SAMPLE attack_delta, release_delta;
  gdouble decay_k;

  /* Pre-calculated for speed :-) */
  SAMPLE *env;
} Data;

typedef struct AllData {
	GtkObject *att, *dec, *sus, *rel, *lev;
	
} AllData;

PRIVATE void precalc_data(Data *d) {
  int i;
  int state;
  SAMPLE level;
  int sustain_time;

  d->duration = SAMPLE_RATE * (d->attack + d->sustain + d->release);
  d->sustain_duration = SAMPLE_RATE * d->sustain;
  d->attack_delta = d->attack > 0 ? 1.0 / (d->attack * SAMPLE_RATE) : 1;
  d->release_delta = d->release > 0
    ? d->sustain_level / (d->release * SAMPLE_RATE)
    : d->sustain_level;
  d->decay_k = d->decay > 0 ? pow(0.5, 1.0 / (d->decay * SAMPLE_RATE)) : 0;

  if (d->env)
    free(d->env);

  d->env = malloc(sizeof(SAMPLE) * d->duration);

  state = 0;
  level = 0;
  sustain_time = 0;

  for (i = 0; i < d->duration; i++) {
    switch (state) {
      case 0:	/* Attack. */
	level += d->attack_delta;
	if (level >= 1)
	  state = 1;
	break;

      case 1:	/* Decay */
	level = d->sustain_level - (d->sustain_level - level) * d->decay_k;
	sustain_time++;
	if (sustain_time >= d->sustain_duration)
	  state = 2;
	break;

      case 2:	/* Release */
	level -= d->release_delta;
	if (level > 0)
	  break;
	/* FALL THROUGH */
      
      default:
	level = 0;
	break;
    }

    d->env[i] = level;
  }
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->attack = 0;
  data->decay = 0;
  data->sustain = 1;
  data->release = 0;
  data->sustain_level = 1.0;
  data->env = NULL;
  precalc_data(data);

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->attack = objectstore_item_get_double(item, "adsr_attack_time", 0);
  data->decay = objectstore_item_get_double(item, "adsr_decay_halftime", 0);
  data->sustain = objectstore_item_get_double(item, "adsr_sustain_time", 1);
  data->release = objectstore_item_get_double(item, "adsr_release_time", 0);
  data->sustain_level = objectstore_item_get_double(item, "adsr_sustain_level", 1.0);
  data->env = NULL;
  precalc_data(data);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_double(item, "adsr_attack_time", data->attack);
  objectstore_item_set_double(item, "adsr_decay_halftime", data->decay);
  objectstore_item_set_double(item, "adsr_sustain_time", data->sustain);
  objectstore_item_set_double(item, "adsr_release_time", data->release);
  objectstore_item_set_double(item, "adsr_sustain_level", data->sustain_level);
}

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  return ((Data *) g->data)->duration;
}

PRIVATE gboolean output_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int len, sil;

  if (data->duration == 0 || offset >= data->duration)
    return FALSE;

  len = MIN(MAX(data->duration - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->env[offset], len * sizeof(SAMPLE));

  sil = buflen - len;
  memset(&buf[len], 0, sil * sizeof(SAMPLE));
  return TRUE;
}

PRIVATE void evt_attack_time_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->attack = event->d.number;
  precalc_data((Data *) g->data);
  gen_update_controls( g, -1 );
}

PRIVATE void evt_decay_halftime_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->decay = event->d.number;
  precalc_data((Data *) g->data);
  gen_update_controls( g, -1 );
}

PRIVATE void evt_sustain_time_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->sustain = event->d.number;
  precalc_data((Data *) g->data);
  gen_update_controls( g, -1 );
}

PRIVATE void evt_release_time_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->release = event->d.number;
  precalc_data((Data *) g->data);
  gen_update_controls( g, -1 );
}

PRIVATE void evt_sustain_level_handler(Generator *g, AEvent *event) {
  ((Data *) g->data)->sustain_level = event->d.number;
  precalc_data((Data *) g->data);
  gen_update_controls( g, -1 );
}

PRIVATE void all_emit(Control *c, int queue_number, gdouble number) {
  AEvent e;

  if (!c->events_flow)
    return;

  gen_init_aevent(&e, AE_NUMBER, NULL, 0, c->g, queue_number, gen_get_sampletime());
  e.d.number = number;

  if (c->desc->is_dst_gen)
    gen_post_aevent(&e);	/* send to c->g as dest */
  else
    gen_send_events(c->g, queue_number, -1, &e);	/* send *from* c->g */
}

PRIVATE void attack_change_handler(GtkWidget *adj, Control *c) {
//	Data *dat;
//
//	dat = ((Data *) c->g->data);
//	dat->attack = GTK_ADJUSTMENT(adj)->value;
//	precalc_data( dat );

	all_emit( c, EVT_ATTACK_TIME, GTK_ADJUSTMENT(adj)->value );
}

PRIVATE void decay_change_handler(GtkWidget *adj, Control *c) {
//	Data *dat;
//
//	dat = ((Data *) c->g->data);
//	dat->decay = GTK_ADJUSTMENT(adj)->value;
//	precalc_data( dat );
	all_emit( c, EVT_DECAY_HALFTIME, GTK_ADJUSTMENT(adj)->value );
}

PRIVATE void sustain_change_handler(GtkWidget *adj, Control *c) {
//	Data *dat;
//
//	dat = ((Data *) c->g->data);
//	dat->sustain = GTK_ADJUSTMENT(adj)->value;
//	precalc_data( dat );
	all_emit( c, EVT_SUSTAIN_TIME, GTK_ADJUSTMENT(adj)->value );
}

PRIVATE void release_change_handler(GtkWidget *adj, Control *c) {
//	Data *dat;
//
//	dat = ((Data *) c->g->data);
//	dat->release = GTK_ADJUSTMENT(adj)->value;
//	precalc_data( dat );
	all_emit( c, EVT_RELEASE_TIME, GTK_ADJUSTMENT(adj)->value );
}

PRIVATE void level_change_handler(GtkWidget *adj, Control *c) {
//	Data *dat;
//
//	dat = ((Data *) c->g->data);
//	dat->sustain_level = GTK_ADJUSTMENT(adj)->value;
//	precalc_data( dat );
	all_emit( c, EVT_SUSTAIN_LEVEL, GTK_ADJUSTMENT(adj)->value );
}


PRIVATE GtkWidget *makeKnob( Control *control, char *label, GtkSignalFunc func,
		double lower, double upper, double step, double page )
{
	GtkWidget *vb, *la;
	GtkKnob	  *kn;
	GtkAdjustment *adj;

	vb = gtk_vbox_new( FALSE, 0 );
	la = gtk_label_new( label );
	gtk_widget_show( GTK_WIDGET( vb ) );
	gtk_widget_show( GTK_WIDGET( la ) );

	kn = (GtkKnob *) gtk_knob_new(NULL);
	g_assert( kn != NULL );
	adj = gtk_knob_get_adjustment( GTK_KNOB( kn ) );

	gtk_signal_connect( GTK_OBJECT(adj), "value_changed", func, control );
	gtk_widget_show( GTK_WIDGET(kn) );
	gtk_box_pack_start( GTK_BOX(vb), GTK_WIDGET(kn), FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX(vb), GTK_WIDGET(la), FALSE, FALSE, 0 );

	adj->lower = lower;
	adj->upper = upper;
	adj->step_increment = step;
	adj->page_increment = page;
	gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
	
	gtk_object_set_user_data( GTK_OBJECT( vb ), kn );

	return vb;
}


PRIVATE void refresh_knob(GtkObject *knoblabel, gdouble val) {
	GtkKnob *kn;
	GtkAdjustment *adj;

	kn = gtk_object_get_user_data( knoblabel );
	adj = gtk_knob_get_adjustment( kn );
	adj->value = val;
	gtk_signal_emit_by_name( GTK_OBJECT( adj ), "value_changed" );
}

PRIVATE void refresh_all(Control *control) {
	Data *data = control->g->data;

	refresh_knob( ((AllData *)control->data)->att, data->attack );
	refresh_knob( ((AllData *)control->data)->dec, data->decay );
	refresh_knob( ((AllData *)control->data)->sus, data->sustain );
	refresh_knob( ((AllData *)control->data)->rel, data->release );
	refresh_knob( ((AllData *)control->data)->lev, data->sustain_level );
}

PRIVATE void init_all( Control *control ) {

	/* FIXME: die Knoepfe sind direkt mit den Werten in der Data Struktur verbunden
	 * 	  Das geht aber nicht. Wenn ich ein altes Control da raus hole werden die
	 * 	  Werte nicht in das all Control uebernommen und andersrum. Ausserdem werden
	 * 	  die Regler beim laden nicht richtig gesetzt.
	 *
	 * 	  Es sollte einen richtigen Mechanismus zum zusammenfassen von mehreren Controls
	 * 	  geben. Aber der gehoert nicht hier hin. Und dann muessen auch Komponenten moeglich
	 * 	  sein. bla bla bla...   Also immer schoen weiter machen.
	 */

	GtkWidget *hb, *knoblabel;
	control->data = safe_malloc( sizeof( AllData ) );

	hb = gtk_hbox_new( FALSE, 0 );
	g_assert( hb != NULL );

	knoblabel = makeKnob( control, "att", GTK_SIGNAL_FUNC( attack_change_handler ), 0, 0.5, 0.001, 0.001 ); 	
	((AllData *)control->data)->att = GTK_OBJECT( knoblabel );
	gtk_box_pack_start( GTK_BOX(hb), knoblabel, FALSE, FALSE, 0 );
	knoblabel = makeKnob( control, "dec", GTK_SIGNAL_FUNC( decay_change_handler ), 0, 0.5, 0.001, 0.001 ); 	
	((AllData *)control->data)->dec = GTK_OBJECT( knoblabel );
	gtk_box_pack_start( GTK_BOX(hb), knoblabel, FALSE, FALSE, 0 );
	knoblabel = makeKnob( control, "sus", GTK_SIGNAL_FUNC( sustain_change_handler ), 0, 1, 0.001, 0.001 ); 	
	((AllData *)control->data)->sus = GTK_OBJECT( knoblabel );
	gtk_box_pack_start( GTK_BOX(hb), knoblabel, FALSE, FALSE, 0 );
	knoblabel = makeKnob( control, "rel", GTK_SIGNAL_FUNC( release_change_handler ), 0, 0.5, 0.001, 0.001 ); 	
	((AllData *)control->data)->rel = GTK_OBJECT( knoblabel );
	gtk_box_pack_start( GTK_BOX(hb), knoblabel, FALSE, FALSE, 0 );
	knoblabel = makeKnob( control, "lev", GTK_SIGNAL_FUNC( level_change_handler ), 0, 1, 0.01, 0.01 ); 	
	((AllData *)control->data)->lev = GTK_OBJECT( knoblabel );
	gtk_box_pack_start( GTK_BOX(hb), knoblabel, FALSE, FALSE, 0 );

	gtk_widget_show( GTK_WIDGET( hb ) );
	
	control->widget = hb;
//	refresh_all( control );
}

PRIVATE void done_all(Control *control) {
}



PRIVATE InputSignalDescriptor input_sigs[] = {
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, output_generator } } },
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "attack", 0,5,0.01,0.01, 0,TRUE, TRUE,EVT_ATTACK_TIME,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, attack) },
  { CONTROL_KIND_KNOB, "decay", 0,5,0.01,0.01, 0,TRUE, TRUE,EVT_DECAY_HALFTIME,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, decay) },
  { CONTROL_KIND_KNOB, "sustain", 0,10,0.01,0.01, 0,TRUE, TRUE,EVT_SUSTAIN_TIME,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, sustain) },
  { CONTROL_KIND_KNOB, "release", 0,5,0.01,0.01, 0,TRUE, TRUE,EVT_RELEASE_TIME,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, release) },
  { CONTROL_KIND_KNOB, "level", 0,1,0.01,0.01, 0,TRUE, TRUE,EVT_SUSTAIN_LEVEL,
    NULL,NULL, control_double_updater, (gpointer) offsetof(Data, sustain_level) },
  { CONTROL_KIND_USERDEF, "compound", 0,0,0,0, 0,FALSE, TRUE,0, init_all, done_all, refresh_all, NULL },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, TRUE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_ATTACK_TIME, "Attack Time", evt_attack_time_handler);
  gen_configure_event_input(k, EVT_DECAY_HALFTIME, "Decay Halftime", evt_decay_halftime_handler);
  gen_configure_event_input(k, EVT_SUSTAIN_TIME, "Sustain Time", evt_sustain_time_handler);
  gen_configure_event_input(k, EVT_RELEASE_TIME, "Release Time", evt_release_time_handler);
  gen_configure_event_input(k, EVT_SUSTAIN_LEVEL, "Sustain Level", evt_sustain_level_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_adsr(void) {
  setup_class();
}
