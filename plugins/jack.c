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

PRIVATE int instance_count = 0;
PRIVATE int jack_instance_count = 0;
//PRIVATE char device[256];

int rate = SAMPLE_RATE;				 /* stream rate */



PRIVATE void audio_play_fragment(Data *data, SAMPLE *left, SAMPLE *right, int length) {
  //OUTPUTSAMPLE *outbuf;
  //int buflen = length * sizeof(OUTPUTSAMPLE) * 2;
  int i,err;

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
  //jack_offset += length;

  //g_print( "writing %d frames\n", length );
}

PRIVATE void playport_play_fragment(OData *data, SAMPLE *samples, int length) {
  //OUTPUTSAMPLE *outbuf;
  //int buflen = length * sizeof(OUTPUTSAMPLE) * 2;
  int i,err;

  OUTPUTSAMPLE *out;
  SAMPLETIME offset = gen_get_sampletime() - jack_timestamp;

  if (length <= 0)
    return;

  out = jack_port_get_buffer( data->port, length );
  

  for (i = 0; i < length; i++) {
    out[i+offset] = (OUTPUTSAMPLE) samples[i];
  }
  //jack_offset += length;

  //g_print( "writing %d frames\n", length );
}

PRIVATE int process_callback( jack_nframes_t nframes, Data *data ) {
    //g_print( "hallo process %d frames:)\n", nframes );
    //
    //XXX: ha... ich habs... jack_offset wird zu jack_timestamp und dann
    //     kann ich immer gen_get_sampletime() - jack_timestamp rechnen und
    //     habe auch meinen offset. sehr gut.
    jack_timestamp = gen_get_sampletime();

    // XXX: ok... hier muss das ding stehen, was deltas zurueck gibt mit denen man dann
    //            mal eben den jack_offset hochzaehlen kann.
    //            Das abschicken des Realtime Events sollte noch schoener gekapselt sein,
    //            und irgendwie sollte ich noch eine bessere Loesung fuer dieses dilemma
    //            finden.
    
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

  if (instance_count > 0)
    /* Not allowed more than one of these things. */
    return 0;

  //instance_count++;
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

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data != NULL) {

    if( jack_instance_count == 1 )
	gen_deregister_clock(jack_clock);

    free(data);
  }

  instance_count--;
  jack_instance_count--;
}

PRIVATE void playport_destroy_instance(Generator *g) {
  Data *data = g->data;

  gen_deregister_realtime_fn(g, playport_realtime_handler);

  if (data != NULL) {

    if( jack_instance_count == 1 )
	gen_deregister_clock(jack_clock);

    free(data);
  }

  jack_instance_count--;
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

PRIVATE void setup_class(void) {
  GeneratorClass *k;


  k = gen_new_generatorclass("jack_out", FALSE, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Jack Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);

  k = gen_new_generatorclass("jack_port", FALSE, 0, 0,
			     playport_input_sigs, NULL, NULL,
			     playport_init_instance, playport_destroy_instance,
			     (AGenerator_pickle_t) playport_init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Jack Out Port",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_jack(void) {
  setup_class();
}



