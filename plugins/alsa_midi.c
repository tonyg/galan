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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//#include <sys/ioctl.h>
//#include <sys/soundcard.h>
#include <alsa/asoundlib.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"alsa_midi_in"
#define GENERATOR_CLASS_PATH	"Misc/ALSA MIDI In"

#define MIDI_BUFSIZE 8

/* 
 * Here is the Data....
 *
 */

enum EVT_INPUTS { 
    NUM_EVENT_INPUTS = 0
};

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

typedef struct Data {
/*  gint fd;  
  gint input_tag;
  SAMPLETIME miditime_offset;
  SAMPLETIME gentime_offset;
  SAMPLETIME last_timestamp;
  gint midibytestocome, midibufpos;
  unsigned char midibuffer[MIDI_BUFSIZE];
  SAMPLETIME buffer_timestamp;
  unsigned char laststatus;
  */

  int seq_port;

} Data;

/**
 * \brief the alsa_seq client
 *
 * This is global. The Ports will be local to the Generator.
 */

PRIVATE snd_seq_t *seq_client;

/**
 * \brief global hash Portid -> Generator
 *
 * The Generator s have to register themselves on this hash.
 */

PRIVATE GHashTable *clients;

PRIVATE int seq_queue;
PRIVATE int seq_count;
PRIVATE int seq_input_tag;
PRIVATE struct pollfd *seq_ufds;

/**
 * \brief open alsa_seq input port
 *
 * This function opens the alsa_sequencer.
 * it should probably determine the filedescriptor we will be polling.
 */

PRIVATE gboolean open_alsa_seq( void ) {

    int ret;

    /* open the sequencer client */
    ret = snd_seq_open(&seq_client, "default",  SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);

    RETURN_VAL_UNLESS( ret >= 0, FALSE );
  
    /* set our clients name */
    snd_seq_set_client_name(seq_client, "galan");
  
    /* set up our clients queue */
    seq_queue = snd_seq_alloc_queue( seq_client );
    //printf( "queue: %d\n", seq_queue );
    return TRUE;

}

PRIVATE void execute_event( snd_seq_event_t *ev ) {
    
    AEvent event;
    //Data *data = g->data;
    //int channel = data->laststatus & 0x0f;

    int port = ev->dest.port;
    Generator *g = g_hash_table_lookup( clients, &port );
    RETURN_UNLESS( g != NULL );
    
    switch( ev->type ) {
	case SND_SEQ_EVENT_CLOCK:
	    {
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.number = 1;
		gen_send_events(g, EVT_CLOCK, -1, &event);
		break;
	    }
	case SND_SEQ_EVENT_START:
	    {
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.number = 1;
		gen_send_events(g, EVT_START, -1, &event);
		break;
	    }
	case SND_SEQ_EVENT_NOTEON:
	    {
		//gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, ev->timestamp );
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.number = ev->data.note.channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		event.d.number = ev->data.note.note;
		gen_send_events(g, EVT_NOTE, -1, &event);

		event.d.number = ev->data.note.velocity;
		gen_send_events(g, EVT_VELOCITY, -1, &event);
	    
		break;
		
	    }
	case SND_SEQ_EVENT_NOTEOFF:
	    {
		//gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, ev->timestamp );
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.number = ev->data.note.channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		event.d.number = ev->data.note.note;
		gen_send_events(g, EVT_NOTEOFF, -1, &event);

		event.d.number = ev->data.note.velocity;
		gen_send_events(g, EVT_VELOCITY, -1, &event);
	    
		break;
		
	    }
	case SND_SEQ_EVENT_PGMCHANGE:
	    {
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
		//gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );

		event.d.number = ev->data.control.channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
		event.d.number = ev->data.control.value;
		gen_send_events(g, EVT_PROGAMCHANGE, -1, &event);
	    }
	case SND_SEQ_EVENT_CONTROLLER:
	    {
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
		//gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );

		event.d.number = ev->data.control.channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
		event.d.number = ev->data.control.param;
		gen_send_events(g, EVT_CTRLPARAM, -1, &event);

		event.d.number = ev->data.control.value;
		gen_send_events(g, EVT_CTRLVALUE, -1, &event);
	    }
	default:
	    break;

    }
//    g_print( "Executing MIDI Command %d...\n" , (int) data->laststatus );
//    switch( data->laststatus & 0xf0 ) {
//	case 0x90:
//	    {
//		// now the note is in data->midibuffer[0] and the velocity is in [1] 
//		
//		//g_print( "lasttimestamp=%d, gen=%d, diff=%d (%fsec)\n", data->last_timestamp, gen_get_sampletime(), data->last_timestamp - gen_get_sampletime(), ((gdouble)(data->last_timestamp - gen_get_sampletime())) / SAMPLE_RATE );
//		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
//
//		event.d.number = channel;
//		gen_send_events(g, EVT_CHANNEL, -1, &event);
//
//		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
//		event.d.number = data->midibuffer[0];
//		gen_send_events(g, EVT_NOTE, -1, &event);
//
//		event.d.number = data->midibuffer[1];
//		gen_send_events(g, EVT_VELOCITY, -1, &event);
//	    
//		break;
//		
//	    }
//	case 0xc0:
//	    {
//		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
//
//		event.d.number = channel;
//		gen_send_events(g, EVT_CHANNEL, -1, &event);
//
//		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
//		event.d.number = data->midibuffer[0];
//		gen_send_events(g, EVT_PROGAMCHANGE, -1, &event);
//	    }
//	    
//    }
	
}

PRIVATE void input_callback( Generator *g, gint source, GdkInputCondition condition ) {

    //Data *data = g->data;
    //unsigned char midievent[4];
    //AEvent event;
    //int l,i;

    snd_seq_event_t *ev;
    //snd_midi_event_t *midi_ev;

    //g_print( "input here :)\n" );

 
  
    /* alsa midi parser */
    //snd_midi_event_new( 10, &midi_ev );

    /* temp for midi data */
    //unsigned char buffer[3];
    
    snd_seq_event_input(seq_client, &ev);

    //g_print( "event type: %d timestamp %d:%d\n", ev->type, ev->time.time.tv_sec, ev->time.time.tv_nsec );

    execute_event( ev );

    //snd_midi_event_decode( midi_ev,
//			   buffer,
//			   3,
//			   ev ); 
    
//    a_in->setTimeStamp( ev->time.tick );
//    a_in->setStatus( buffer[0] );
//    a_in->setData( buffer[1], buffer[2] );

    snd_seq_free_event( ev );
//    snd_midi_event_free( midi_ev );


//    l = read( source, &midievent, 4 );
//
//    for( i=0; i<l; i+=4 ) {
//	//g_print( "%d midiev: %d, %x\n", i, (int) midievent[i], *((int *) &midievent[i]) & 0xffffff00 );
//	switch( midievent[i] ) {
//	    case SEQ_WAIT:
//		//g_print( "TMR_WAIT_ABS: %d\n",  *((int *) &midievent[i]) >> 8 );
//		if( data->miditime_offset == -1 ) {
//		    data->miditime_offset = (*((int *) midievent) >> 8)  * SAMPLE_RATE / 100;
//		    data->gentime_offset = gen_get_sampletime();
//		}
//		data->last_timestamp = (*((int *) &midievent[i]) >> 8) * SAMPLE_RATE / 100
//		    - data->miditime_offset + data->gentime_offset;
//
//		break;
//	    case SEQ_MIDIPUTC:
//		switch( midievent[1] & 0xf0 ) {
//		    case 0xf0:
//			switch( midievent[1] ) {
//			    case 0xf8:
//				gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
//				event.d.number = 1;
//				gen_send_events(g, EVT_CLOCK, -1, &event);
//				break;
//
//			    case 0xfa:
//				gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
//				event.d.number = 1;
//				gen_send_events(g, EVT_START, -1, &event);
//				break;
//			}
//			break;
//
//		    case 0xe0:
//		    case 0xd0:
//		    case 0xc0:
//		    case 0xb0:
//		    case 0xa0:
//		    case 0x80:
//		    case 0x90:
//			data->laststatus = midievent[1];
//			data->midibytestocome = get_bytes_to_come( data->laststatus );
//			//g_print( "playevent here\n" );
//			data->midibufpos = 0;
//			
//			break;
//
//		    default:
//			//g_print( "in default... %d ls=%x\n", data->midibytestocome, (int) data->laststatus );
//			if( data->midibytestocome ) {
//			    
//			    data->midibuffer[data->midibufpos++] = midievent[1];
//
//			    if( (data->midibytestocome--) == 1 ) {
//
//				// Now there is a midicommand in data->midibuffer
//				// status byte is data->laststatus
//
//				execute_midi_command( g );
//			    }
//
//			} else {
//
//			    // running command.. set bytes_to_come to 
//			    data->midibytestocome = get_bytes_to_come( data->laststatus ) - 1;
//			    data->midibufpos = 0;
//
//			    // Store this byte...
//			    data->midibuffer[data->midibufpos++] = midievent[1];
//			}
//			    
//
//
//		}
//		break;
//	}
}

PRIVATE int setup_input_handler( void ) {

  int err;

  seq_count = snd_seq_poll_descriptors_count (seq_client, POLLIN);
  if (seq_count <= 0) {
      printf("Invalid poll descriptors count\n");
      return 0;
  }
  g_print( "poll count = %d\n", seq_count );

  seq_ufds = malloc(sizeof(struct pollfd) * seq_count);
  if (seq_ufds == NULL) {
      printf("No enough memory\n");
      return 0;
  }
  if ((err=snd_seq_poll_descriptors(seq_client, seq_ufds, seq_count, POLLIN)) < 0) {
      printf("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
      return 0;
  }

  seq_input_tag = gdk_input_add(seq_ufds[0].fd, GDK_INPUT_READ,
					(GdkInputFunction) input_callback, NULL);
  //seq_clock = gen_register_clock(g, "ALSA Output Clock", clock_handler);


  //gen_register_realtime_fn(g, realtime_handler);
  //gen_select_clock(data->clock);	/* a not unreasonable assumption? */

  return 1;
}

PRIVATE int get_bytes_to_come( unsigned char midistatus ) {

    switch( midistatus & 0xf0 ) {

	case 0x80:
	case 0x90:
	case 0xa0:
	case 0xb0:
	case 0xe0:
	    return 2;
	case 0xc0:
	case 0xd0:
	    return 1;
	    
	default:
	    return 0;
    }
}

/*
 * This is the input callback....
 * Seems to bo ok for now..
 */




PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int ret;
  g->data = data;

  /*
   * open a port on alsa_seq.
   * how do i update the name of the port when generator is renamed ?
   */


    ret = snd_seq_create_simple_port(seq_client, 
				     g->name,
				     SND_SEQ_PORT_CAP_WRITE |
				     SND_SEQ_PORT_CAP_SUBS_WRITE,
				     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				     SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	return 0;
    }

    g_hash_table_insert( clients, &data->seq_port, g );

//  data->fd = open( "/dev/sequencer", O_RDWR | O_NONBLOCK );
//  if( data->fd == -1 ) {
//      free( data );
//      return 0;
//  }
//  data->miditime_offset = -1;
//  data->last_timestamp = gen_get_sampletime();
//  data->midibytestocome = 0;
//  data->laststatus = 0;

//  ioctl( data->fd, SNDCTL_SEQ_ACTSENSE_ENABLE, 0 );
//  ioctl( data->fd, SNDCTL_SEQ_TIMING_ENABLE, 0 );
//  ioctl( data->fd, SNDCTL_SEQ_RT_ENABLE, 0 );
      
  //data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction)  input_callback, (gpointer) g ); 
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  g_hash_table_remove( clients, &data->seq_port );

  snd_seq_delete_port( seq_client, data->seq_port );

  free(g->data);
}


/*
 * pickle and unpickle 
 * 
 * are straight forward....
 * 
 */

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
    Data *data = safe_malloc(sizeof(Data));
    int ret;

    g->data = data;

    ret = snd_seq_create_simple_port(seq_client, 
	    g->name,
	    SND_SEQ_PORT_CAP_WRITE |
	    SND_SEQ_PORT_CAP_SUBS_WRITE,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	    SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	// TODO: make this safe even if port cannot be established
	//       could be in the generator which can have a null generatorclass 
	//       which can be setup by the unpickler....
	return;
    }

    g_hash_table_insert( clients, &data->seq_port, g );

    //  data->fd = open( "/dev/sequencer", O_RDWR | O_NONBLOCK );
    //  if( data->fd == -1 ) {
    //      free( data );
    //      return 0;
    //  }
    //  data->miditime_offset = -1;
    //  data->last_timestamp = gen_get_sampletime();
    //  data->midibytestocome = 0;
    //  data->laststatus = 0;

    //  ioctl( data->fd, SNDCTL_SEQ_ACTSENSE_ENABLE, 0 );
    //  ioctl( data->fd, SNDCTL_SEQ_TIMING_ENABLE, 0 );
    //  ioctl( data->fd, SNDCTL_SEQ_RT_ENABLE, 0 );

    //data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction)  input_callback, (gpointer) g ); 
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  //Data *data = g->data;
}


PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
     { CONTROL_KIND_NONE, }
};


/**
 * \brief Setup the class of the midi_in Generator
 */

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_output(k, EVT_CLOCK,   "Clock");
  gen_configure_event_output(k, EVT_START,   "Start");
  gen_configure_event_output(k, EVT_CHANNEL,   "Channel");
  gen_configure_event_output(k, EVT_NOTE,   "NoteOn");
  gen_configure_event_output(k, EVT_VELOCITY,   "Velocity");
  gen_configure_event_output(k, EVT_PROGAMCHANGE,   "Program");
  gen_configure_event_output(k, EVT_CTRLPARAM,   "Control Param");
  gen_configure_event_output(k, EVT_CTRLVALUE,   "Control Value" );
  gen_configure_event_output(k, EVT_NOTEOFF,   "NoteOff");
  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_alsa_midi(void) {
    if( !open_alsa_seq() )
	return;
    if( !setup_input_handler() )
	return;

  clients=g_hash_table_new( g_int_hash, g_int_equal );
    
  setup_class();
}


