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

#define SIG_LEFT_CHANNEL	0
#define SIG_RIGHT_CHANNEL	1


typedef float OUTPUTSAMPLE;

PRIVATE AClock *jack_clock = NULL;
PRIVATE SAMPLETIME jack_timestamp = 0;
PRIVATE jack_client_t *jack_client = NULL;

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

typedef struct TransportData {
  gdouble freq;
  gint period;
} TransportData;

PRIVATE int instance_count = 0;
PRIVATE int jack_instance_count = 0;

int rate = SAMPLE_RATE;				 /* stream rate */

GList *transport_clocks = NULL;

PRIVATE void transport_frame_event( Generator *g, SAMPLETIME frame, SAMPLETIME numframes );

PRIVATE void audio_play_fragment(Data *data, SAMPLE *left, SAMPLE *right, int length) {
  int i;

  OUTPUTSAMPLE *lout, *rout;
  SAMPLETIME offset = gen_get_sampletime() - jack_timestamp;

  if (length <= 0)
    return;

  lout = jack_port_get_buffer( data->port_l, length );
  rout = jack_port_get_buffer( data->port_r, length );
  

  for (i = 0; i < length; i++) {
    lout[i+offset] = (OUTPUTSAMPLE) left[i];
    rout[i+offset] = (OUTPUTSAMPLE) right[i];
  }
}

PRIVATE void playport_play_fragment(OData *data, SAMPLE *samples, int length) {
  int i;

  OUTPUTSAMPLE *out;
  SAMPLETIME offset = gen_get_sampletime() - jack_timestamp;

  if (length <= 0)
    return;

  out = jack_port_get_buffer( data->port, length );
  

  for (i = 0; i < length; i++) {
    out[i+offset] = (OUTPUTSAMPLE) samples[i];
  }
}

PRIVATE int process_callback( jack_nframes_t nframes, Data *data ) {

    jack_timestamp = gen_get_sampletime();

    if( transport_clocks ) {
	jack_transport_info_t trans_info;
	GList *l;

	jack_get_transport_info( jack_client, &trans_info );

	if( trans_info.valid & JackTransportState && trans_info.valid & JackTransportPosition && trans_info.transport_state == JackTransportRolling ) {
	    
	    for( l = transport_clocks; l; l = g_list_next( l ) ) {
		Generator *g = l->data;
		transport_frame_event( g, trans_info.frame, nframes );
	    }
	}
	else
	    if( trans_info.valid & JackTransportState && trans_info.transport_state == JackTransportRolling )
		printf( "Invalid Frame :(\n" );

    }
    gen_clock_mainloop_have_remaining( nframes );

    return 0;
}

void
jack_shutdown (void *arg)
{
	g_print( "jack exited :(\n" );
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  //Data *d = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      jack_deactivate( jack_client );
      //snd_async_del_handler(d->async_handler);
      //d->async_handler = NULL;
      break;

    case CLOCK_ENABLE:
      jack_set_process_callback( jack_client, process_callback, NULL ); 
      jack_on_shutdown (jack_client, jack_shutdown, 0);

      g_print( "pre activate\n" );
      jack_activate( jack_client );
      g_print( "post activate\n" );
      //if (jack_connect (jack_client, jack_port_name (d->port_r), "alsa_pcm:playback_2")) {
	//  g_print ("cannot connect output ports\n");
      //}
      //if (jack_connect (d->jack_client, jack_port_name (d->port_l), "alsa_pcm:playback_1")) {
//	  g_print ("cannot connect output ports\n");
//      }
//      g_print ("srcport = %s\n", jack_port_name (d->port_r) );

	      
      break;

    default:
      g_message("Unreachable code reached (jack_output)... reason = %d", reason);
      break;
  }
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
      //SAMPLE *l_buf, *r_buf;
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      //l_buf = safe_malloc(bufbytes);
      //r_buf = safe_malloc(bufbytes);

      if (!gen_read_realtime_input(g, SIG_LEFT_CHANNEL, -1, data->l_buf, event->d.integer))
	memset(data->l_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT_CHANNEL, -1, data->r_buf, event->d.integer))
	memset(data->r_buf, 0, bufbytes);

      audio_play_fragment(data, data->l_buf, data->r_buf, event->d.integer);
      //free(l_buf);
      //free(r_buf);

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
      g_warning("oss_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  Data *data;
  //int err;

  if (instance_count > 0)
    /* Not allowed more than one of these things. */
    return 0;

  instance_count++;
  jack_instance_count++;

  data = safe_malloc(sizeof(Data));

  data->l_buf = safe_malloc( sizeof(SAMPLE) * 4096 );
  data->r_buf = safe_malloc( sizeof(SAMPLE) * 4096 );
  //jack_timestamp = ;

  if( jack_client == NULL )
      jack_client = jack_client_new( "galan" );
  

  if (jack_client == NULL) {
      if( data->l_buf )
	  free( data->l_buf );

      if( data->r_buf )
	  free( data->r_buf );

    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open Jack Client");
    return 0;
  }

  data->port_l = jack_port_register (jack_client, "std_left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  data->port_r = jack_port_register (jack_client, "std_right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  if( jack_clock == NULL ) {
      jack_clock = gen_register_clock(g, "Jack Clock", clock_handler);
      gen_select_clock(jack_clock);	/* a not unreasonable assumption? */
  }

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
  //jack_timestamp = ;

  if( jack_client == NULL )
      jack_client = jack_client_new( "galan" );
  

  if (jack_client == NULL) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open Jack Client");
    return 0;
  }

  data->port = jack_port_register (jack_client, g->name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  if( jack_clock == NULL ) {
      jack_clock = gen_register_clock(g, "Jack Clock", clock_handler);
      gen_select_clock(jack_clock);	/* a not unreasonable assumption? */
  }

  g->data = data;

  gen_register_realtime_fn(g, playport_realtime_handler);

  return 1;
}

PRIVATE int recport_init_instance(Generator *g) {
    OData *data;

    jack_instance_count++;

    data = safe_malloc(sizeof(OData));

    if( jack_client == NULL )
	jack_client = jack_client_new( "galan" );


    if (jack_client == NULL) {
	free(data);
	popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		"Could not open Jack Client");
	return 0;
    }

    data->port = jack_port_register (jack_client, g->name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    if( jack_clock == NULL ) {
	jack_clock = gen_register_clock(g, "Jack Clock", clock_handler);
	gen_select_clock(jack_clock);	/* a not unreasonable assumption? */
    }

    g->data = data;

    return 1;
}


PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;

    gen_deregister_realtime_fn(g, realtime_handler);

    if (data != NULL) {
	if( data->l_buf )
	    free( data->l_buf );

	if( data->r_buf )
	    free( data->r_buf );

	jack_port_unregister( jack_client, data->port_l );
	jack_port_unregister( jack_client, data->port_r );

	if( jack_instance_count == 1 ) {
	    gen_deregister_clock(jack_clock);
	    jack_client_close( jack_client );
	}

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

	jack_port_unregister( jack_client, data->port );
	if( jack_instance_count == 1 ) {
	    gen_deregister_clock(jack_clock);
	    jack_client_close( jack_client );
	}

	free(data);
    }

    jack_instance_count--;
}

PRIVATE void recport_destroy_instance(Generator *g) {
  OData *data = g->data;

  if (data != NULL) {

    jack_port_unregister( jack_client, data->port );
    if( jack_instance_count == 1 ) {
	gen_deregister_clock(jack_clock);
	jack_client_close( jack_client );
    }

    free(data);
  }

  jack_instance_count--;
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
    OData *data = g->data;
    int i;

    OUTPUTSAMPLE *in;
    SAMPLETIME offset = gen_get_sampletime() - jack_timestamp;


    in = jack_port_get_buffer( data->port, buflen );


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

PRIVATE gboolean transport_init_instance(Generator *g) {
  TransportData *data = safe_malloc(sizeof(TransportData));
  g->data = data;

  data->period = 0;

  transport_clocks = g_list_append( transport_clocks, g );
  return TRUE;
}

PRIVATE void transport_destroy_instance(Generator *g) {
    transport_clocks = g_list_remove( transport_clocks, g );
  free(g->data);
}

PRIVATE void transport_unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  TransportData *data = safe_malloc(sizeof(TransportData));
  g->data = data;

  data->period = objectstore_item_get_integer(item, "transport_period", 0);

  if (data->period != 0) {
    data->freq = (gdouble) SAMPLE_RATE / data->period;
  }
  transport_clocks = g_list_append( transport_clocks, g );
}

PRIVATE void transport_pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  TransportData *data = g->data;
  objectstore_item_set_integer(item, "transport_period", data->period);
}

PRIVATE void transport_evt_freq_handler(Generator *g, AEvent *event) {
  TransportData *data = g->data;

  data->freq = event->d.number;

  if (event->d.number < 0.0001)
    data->period = 0;
  else
    data->period = SAMPLE_RATE / event->d.number;
}

PRIVATE void transport_frame_event( Generator *g, SAMPLETIME frame, SAMPLETIME numframes ) {
    TransportData *data = g->data;
    AEvent e;
    SAMPLETIME t,i;

    if( data->period != 0 ) {

	for( t=(frame-1)/data->period+1, i=t*data->period-frame; i<numframes; t++, i+=data->period ) {
	    gen_init_aevent(&e, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() + i);
	    e.d.number = t;
	    gen_send_events(g, 0, -1, &e);
	}
    }

}

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

  gencomp_register_generatorclass(k, FALSE, "Outputs/Jack In Port",
				  NULL,
				  NULL);

  k = gen_new_generatorclass("jack_transport", FALSE, 1, 1,
			     NULL, NULL, NULL,
			     transport_init_instance, transport_destroy_instance,
			     (AGenerator_pickle_t) transport_unpickle_instance, transport_pickle_instance);

  gen_configure_event_input(k, 0, "Freq", transport_evt_freq_handler );
  gen_configure_event_output(k, 0, "Position");

  gencomp_register_generatorclass(k, FALSE, "Misc/Jack Transport Clock",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_jack(void) {
  setup_class();
}



