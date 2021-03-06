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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"
#include "prefs.h"

#include <jack/jack.h>
#ifdef HAVE_JACKMIDI_H
#include <jack/midiport.h>
#endif

#include <galan_jack.h>
#include <galan_lash.h>

#define SIG_LEFT_CHANNEL	0
#define SIG_RIGHT_CHANNEL	1


typedef float OUTPUTSAMPLE;


typedef struct Data {
  //jack_client_t *jack_client;
  jack_port_t *port_l, *port_r;
  SAMPLE *l_buf, *r_buf;
  //SAMPLETIME offset;
  //AClock *clock;
} Data;

typedef struct OData {
    jack_port_t *port;
    SAMPLE *buf;
} OData;

typedef struct MidiInData {
    jack_port_t *port;
    SAMPLETIME lastprocess_stamp;
    jack_nframes_t nframes;
    
} MidiInData;

enum EVT_OUTPUTS { 
    EVT_CLOCK = 0,
    EVT_START,
    EVT_CHANNEL,
    EVT_NOTE,
    EVT_VELOCITY,
    EVT_PROGAMCHANGE,
    EVT_CTRLPARAM,
    EVT_CTRLVALUE,
    EVT_NOTEOFF,
    NUM_EVENT_OUTPUTS
};

typedef struct TransportData {
  gdouble freq;
  gdouble ticks_per_beat;
  gdouble period;
  gdouble last_bpm;
} TransportData;

PRIVATE int instance_count = 0;
PRIVATE int jack_instance_count = 0;

PRIVATE void transport_frame_event( Generator *g, SAMPLETIME frame, SAMPLETIME numframes, gdouble bpm );



PRIVATE void audio_play_fragment(Data *data, SAMPLE *left, SAMPLE *right, int length) {
  int i;

  OUTPUTSAMPLE *lout, *rout;
  SAMPLETIME offset = gen_get_sampletime() - galan_jack_get_timestamp();

  if (length <= 0)
    return;

  if( (data->port_l==NULL) || (data->port_r==NULL) )
      return;

  lout = jack_port_get_buffer( data->port_l, length+offset );
  rout = jack_port_get_buffer( data->port_r, length+offset );
  

  for (i = 0; i < length; i++) {
    lout[i+offset] = (OUTPUTSAMPLE) left[i];
    rout[i+offset] = (OUTPUTSAMPLE) right[i];
  }
}

PRIVATE void playport_play_fragment(OData *data, SAMPLE *samples, int length) {
  int i;

  OUTPUTSAMPLE *out;
  SAMPLETIME offset = gen_get_sampletime() - galan_jack_get_timestamp();

  if (length <= 0)
    return;

  if( data->port == NULL )
      return;

  out = jack_port_get_buffer( data->port, length+offset );
  

  for (i = 0; i < length; i++) {
    out[i+offset] = (OUTPUTSAMPLE) samples[i];
  }
}


PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
      //SAMPLE *l_buf, *r_buf;
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      if (!gen_read_realtime_input(g, SIG_LEFT_CHANNEL, -1, data->l_buf, event->d.integer))
	memset(data->l_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT_CHANNEL, -1, data->r_buf, event->d.integer))
	memset(data->r_buf, 0, bufbytes);

      audio_play_fragment(data, data->l_buf, data->r_buf, event->d.integer);

      break;
    }

    default:
      g_warning("oss_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE void playport_realtime_handler(Generator *g, AEvent *event) {
  OData *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
			  
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      if (!gen_read_realtime_input(g, SIG_LEFT_CHANNEL, -1, data->buf, event->d.integer))
	memset(data->buf, 0, bufbytes);

      playport_play_fragment(data, data->buf, event->d.integer);

      break;
    }

    default:
      g_warning("jack_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  Data *data;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  jack_instance_count++;

  data = safe_malloc(sizeof(Data));

  data->l_buf = safe_malloc( sizeof(SAMPLE) * 4096 );
  data->r_buf = safe_malloc( sizeof(SAMPLE) * 4096 );

  data->port_l = jack_port_register (galan_jack_get_client(), "std_left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  data->port_r = jack_port_register (galan_jack_get_client(), "std_right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  if( (data->port_l == NULL) || (data->port_r == NULL) )
	  return 0;

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);

  return 1;
}

PRIVATE int playport_init_instance(Generator *g) {
  OData *data;
  //int err;

  jack_instance_count++;

  data = safe_malloc(sizeof(OData));

  data->buf = safe_malloc( sizeof(SAMPLE) * 4096 );

  

  data->port = jack_port_register (galan_jack_get_client(), g->name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  g->data = data;

  gen_register_realtime_fn(g, playport_realtime_handler);

  return 1;
}

PRIVATE int recport_init_instance(Generator *g) {
    OData *data;

    jack_instance_count++;

    data = safe_malloc(sizeof(OData));


    data->port = jack_port_register (galan_jack_get_client(), g->name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    g->data = data;

    return 1;
}


PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;

    gen_deregister_realtime_fn(g, realtime_handler);

    if (data != NULL)
    {
	if( data->l_buf )
	    free( data->l_buf );
	if( data->r_buf )
	    free( data->r_buf );

	if( data->port_l )
	    jack_port_unregister( galan_jack_get_client(), data->port_l );
	if( data->port_r )
	    jack_port_unregister( galan_jack_get_client(), data->port_r );

	free(data);
    }

    instance_count--;
    jack_instance_count--;
}

PRIVATE void playport_destroy_instance(Generator *g) {
    OData *data = g->data;

    gen_deregister_realtime_fn(g, playport_realtime_handler);

    if (data != NULL) {

	if( data->buf )
	    free( data->buf );

	if( data->port )
	    jack_port_unregister( galan_jack_get_client(), data->port );

	free(data);
    }

    jack_instance_count--;
}

PRIVATE void recport_destroy_instance(Generator *g) {
  OData *data = g->data;

  if (data != NULL) {

      if( data->port )
	  jack_port_unregister( galan_jack_get_client(), data->port );
      free(data);
  }

  jack_instance_count--;
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
    OData *data = g->data;
    int i;

    OUTPUTSAMPLE *in;
    SAMPLETIME offset = gen_get_sampletime() - galan_jack_get_timestamp();

    if( data->port == NULL )
	return FALSE;

    in = jack_port_get_buffer( data->port, buflen+offset );


    for (i = 0; i < buflen; i++)
	buf[i] = in[i+offset];

    return TRUE;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Left Channel", SIG_FLAG_REALTIME },
  { "Right Channel", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE InputSignalDescriptor playport_input_sigs[] = {
  { "Output", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Input", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};


// Transport

PRIVATE gboolean transport_init_instance(Generator *g) {
  TransportData *data = safe_malloc(sizeof(TransportData));
  g->data = data;

  data->period = 0.0;
  data->ticks_per_beat = 4.0;
  data->last_bpm = 0;

  galan_jack_register_transport_clock( g, transport_frame_event );
  return TRUE;
}

PRIVATE void transport_destroy_instance(Generator *g) {
  galan_jack_deregister_transport_clock( g, transport_frame_event );
  free(g->data);
}

PRIVATE void transport_unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  TransportData *data = safe_malloc(sizeof(TransportData));
  g->data = data;
  int ticks_per_beat_int = objectstore_item_get_integer(item, "ticks_per_beat", 4);

  data->period = objectstore_item_get_integer(item, "transport_period", 0);
  data->ticks_per_beat = objectstore_item_get_double(item, "ticks_per_beat_double", (double) ticks_per_beat_int );
  data->last_bpm = 0;

  if (data->period != 0) {
    data->freq = (gdouble) SAMPLE_RATE / data->period;
  }
  galan_jack_register_transport_clock( g, transport_frame_event );
}

PRIVATE void transport_pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  TransportData *data = g->data;
  objectstore_item_set_integer(item, "transport_period", data->period);
  objectstore_item_set_double(item, "ticks_per_beat_double", data->ticks_per_beat);
}

PRIVATE void transport_evt_freq_handler(Generator *g, AEvent *event) {
  TransportData *data = g->data;

  data->freq = event->d.number;

  if (event->d.number < 0.0001)
    data->period = 0;
  else
    data->period = SAMPLE_RATE / event->d.number;

  gen_update_controls(g, 0);
}

PRIVATE void transport_evt_tpb_handler(Generator *g, AEvent *event) {
  TransportData *data = g->data;

  data->ticks_per_beat = (event->d.number);
  
  gen_update_controls(g, 1);
  
}

PRIVATE void transport_evt_emit_bpm_handler(Generator *g, AEvent *event) {
  TransportData *data = g->data;

  if( data->last_bpm != 0 ) {
      gen_init_aevent( event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
      event->d.number = data->last_bpm;
      gen_send_events(g, 1, -1, event);
  }
}

PRIVATE void transport_frame_event( Generator *g, SAMPLETIME frame, SAMPLETIME numframes, gdouble bpm ) {
    TransportData *data = g->data;
    AEvent e;
    SAMPLETIME t,i;

    if( data->ticks_per_beat == 0 )
	data->period = 0;

    if( (bpm != 0.0) && (data->ticks_per_beat != 0) ) {
	data->period = ((gdouble)(SAMPLE_RATE)) * 60.0 / (gdouble) data->ticks_per_beat / bpm;
    }

    if( data->period != 0 ) {

	// ok...
	t = (int) floor( ((double)(frame)) / data->period);
	i = (SAMPLETIME) ( ((double)t)*data->period ) - frame;
	//for( t=(frame-1)/data->period+1, i=t*data->period-frame; i<numframes; t++, i+=data->period ) {
	for( ; i<numframes; t++, i+= data->period) {
	    if( i>=0 ) {
		gen_init_aevent(&e, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() + i);
		e.d.number = t;
		gen_send_events(g, 0, -1, &e);
	    }
	}
    }
    if( data->last_bpm != bpm ) {
	data->last_bpm = bpm;
	transport_evt_emit_bpm_handler( g, &e );
    }
}

// Midi_in
#ifdef HAVE_JACKMIDI_H
PRIVATE void midiinport_jackprocess_handler(Generator *g, jack_nframes_t nframes) {
    MidiInData *data = g->data;
    void * input_buf;
    jack_midi_event_t input_event;
    jack_nframes_t input_event_count;
    jack_nframes_t i;
    AEvent ev;

    data->nframes = nframes;
    if( data->port == NULL )
	return;

    input_buf = jack_port_get_buffer(data->port, nframes);
    input_event_count = jack_midi_get_event_count(input_buf);

  /* iterate over all incoming JACK MIDI events */
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i);

    ev.time = input_event.time + gen_get_sampletime();
    ev.kind = AE_MIDIEVENT;
    ev.d.midiev.len = input_event.size;
    memcpy( &(ev.d.midiev.midistring), input_event.buffer,  input_event.size);

    
    /* normalise note events if needed */
    if ((ev.d.midiev.len == 3) && ((ev.d.midiev.midistring[0] & 0xF0) == 0x90) &&
        (ev.d.midiev.midistring[2] == 0))
    {
      ev.d.midiev.midistring[0] = 0x80 | (ev.d.midiev.midistring[0] & 0x0F);
    }

    gen_send_events(g, 0, -1, &ev);
  }

}

PRIVATE int midiinport_init_instance(Generator *g) {
    MidiInData *data;

    jack_instance_count++;

    data = safe_malloc(sizeof(MidiInData));


    data->port = jack_port_register (galan_jack_get_client(), g->name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

    g->data = data;

    //gen_register_realtime_fn(g, midiinport_realtime_handler);
    galan_jack_register_process_handler( g, midiinport_jackprocess_handler );
    return 1;
}

PRIVATE void midiinport_destroy_instance(Generator *g) {
    MidiInData *data = g->data;

    galan_jack_deregister_process_handler(g, midiinport_jackprocess_handler);
    if (data != NULL) {

	if( data->port )
	    jack_port_unregister( galan_jack_get_client(), data->port );
	free(data);
    }

    jack_instance_count--;
}

PRIVATE void midioutport_jackprocess_handler( Generator *g, jack_nframes_t nframes ) {
    MidiInData *data = g->data;

    void * output_buf;
    if( data->port == NULL )
	return;

    output_buf = jack_port_get_buffer(data->port, nframes);

    jack_midi_clear_buffer( output_buf );
    data->lastprocess_stamp = gen_get_sampletime();
    data->nframes = nframes;
}

PRIVATE void midioutport_midievent_handler(Generator *g, AEvent *event) {
    MidiInData *data = g->data;

    void * output_buf;
    if( data->port == NULL )
	return;

    output_buf = jack_port_get_buffer(data->port, data->nframes);

    jack_midi_event_write( output_buf,
	    event->time - data->lastprocess_stamp, 
	    event->d.midiev.midistring, 
	    event->d.midiev.len			    );
}

PRIVATE int midioutport_init_instance(Generator *g) {
    MidiInData *data;

    jack_instance_count++;

    data = safe_malloc(sizeof(MidiInData));

    data->port = jack_port_register (galan_jack_get_client(), g->name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

    g->data = data;

    //gen_register_realtime_fn(g, midiinport_realtime_handler);
    galan_jack_register_process_handler( g, midioutport_jackprocess_handler );

    return 1;
}

PRIVATE void midioutport_destroy_instance(Generator *g) {
    MidiInData *data = g->data;

    //gen_deregister_realtime_fn(g, midioutport_realtime_handler);
    galan_jack_deregister_process_handler( g, midioutport_jackprocess_handler );
    if (data != NULL) {

	if( data->port )
	    jack_port_unregister( galan_jack_get_client(), data->port );
	free(data);
    }

    jack_instance_count--;
}
#endif

// General

PRIVATE ControlDescriptor transport_controls[] = {
  { CONTROL_KIND_KNOB, "rate", 0,500,1,1, 0,TRUE, TRUE, 0,
    NULL,NULL, control_double_updater, (gpointer) offsetof(TransportData, freq) },
  { CONTROL_KIND_KNOB, "tpb", 0,32,1,1, 0,TRUE, TRUE, 1,
    NULL,NULL, control_double_updater, (gpointer) offsetof(TransportData, ticks_per_beat) },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k;


  k = gen_new_generatorclass("jack_out", FALSE, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Jack Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);

  k = gen_new_generatorclass("jack_outport", FALSE, 0, 0,
			     playport_input_sigs, NULL, NULL,
			     playport_init_instance, playport_destroy_instance,
			     (AGenerator_pickle_t) playport_init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Jack Out Port",
				  NULL,
				  NULL);

  k = gen_new_generatorclass("jack_inport", FALSE, 0, 0,
			     NULL, output_sigs, NULL,
			     recport_init_instance, recport_destroy_instance,
			     (AGenerator_pickle_t) recport_init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Sources/Jack In Port",
				  NULL,
				  NULL);

#ifdef HAVE_JACKMIDI_H
  k = gen_new_generatorclass("jack_midiin", FALSE, 0, 1,
			     NULL, NULL, NULL,
			     midiinport_init_instance, midiinport_destroy_instance,
			     (AGenerator_pickle_t) midiinport_init_instance, NULL);

  gen_configure_event_output(k, 0,   "midi");
  gencomp_register_generatorclass(k, FALSE, "Midi Events/Jack Midi In",
				  NULL,
				  NULL);

  k = gen_new_generatorclass("jack_midiout", FALSE, 1, 0,
			     NULL, NULL, NULL,
			     midioutport_init_instance, midioutport_destroy_instance,
			     (AGenerator_pickle_t) midioutport_init_instance, NULL);

  gen_configure_event_input(k, 0,   "midi", midioutport_midievent_handler);
  gencomp_register_generatorclass(k, FALSE, "Midi Events/Jack Midi Out",
				  NULL,
				  NULL);
#endif

  k = gen_new_generatorclass("jack_transport", FALSE, 3, 2,
			     NULL, NULL, transport_controls,
			     transport_init_instance, transport_destroy_instance,
			     (AGenerator_pickle_t) transport_unpickle_instance, transport_pickle_instance);


  gen_configure_event_input(k, 0, "Freq", transport_evt_freq_handler );
  gen_configure_event_input(k, 1, "TpB", transport_evt_tpb_handler );
  gen_configure_event_input(k, 2, "Emit Bpm", transport_evt_emit_bpm_handler );
  gen_configure_event_output(k, 0, "Position");
  gen_configure_event_output(k, 1, "bpm");

  gencomp_register_generatorclass(k, FALSE, "Misc/Jack Transport Clock",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin(void) {
  setup_class();
}



