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
#include "galan_lash.h"

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

  int seq_port;
  void (*exec_ev)( Generator *g, snd_seq_event_t *ev );
  snd_seq_event_t *rdev;

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

			if( lash_enabled( galan_lash_get_client() ) )
			    lash_alsa_client_id(galan_lash_get_client(), (unsigned char)snd_seq_client_id(seq_client));

    //printf( "queue: %d\n", seq_queue );
    return TRUE;

}

PRIVATE void execute_event_old( Generator *g, snd_seq_event_t *ev ) {
    
    AEvent event;
    
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
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.number = ev->data.note.channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		event.d.number = ev->data.note.note;

		if( ev->data.note.velocity != 0 )
		    gen_send_events(g, EVT_NOTE, -1, &event);
		else
		    gen_send_events(g, EVT_NOTEOFF, -1, &event);

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
	
}

PRIVATE void execute_event_new( Generator *g, snd_seq_event_t *ev ) {
    
    AEvent event;
    
    switch( ev->type ) {
	case SND_SEQ_EVENT_CLOCK:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 1;
		event.d.midiev.midistring[0] = 0xf8;  // FIXME
		gen_send_events(g, 0, -1, &event);
		break;
	    }
	case SND_SEQ_EVENT_START:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 1;
		event.d.midiev.midistring[0] = 0xfa;  // FIXME
		gen_send_events(g, 0, -1, &event);
		break;
	    }
	case SND_SEQ_EVENT_NOTEON:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 3;
		event.d.midiev.midistring[0] = 0x90 | ev->data.note.channel;

		event.d.midiev.midistring[1] = ev->data.note.note;
		event.d.midiev.midistring[2] = ev->data.note.velocity;

		gen_send_events(g, 0, -1, &event);
	    
		break;
	    }
	case SND_SEQ_EVENT_NOTEOFF:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 2;
		event.d.midiev.midistring[0] = 0x80 | ev->data.note.channel;

		event.d.midiev.midistring[1] = ev->data.note.note;

		gen_send_events(g, 0, -1, &event);
	    
		break;
	    }
	case SND_SEQ_EVENT_PGMCHANGE:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 2;
		event.d.midiev.midistring[0] = 0xc0 | ev->data.control.channel;

		event.d.midiev.midistring[1] = ev->data.control.value;

		gen_send_events(g, 0, -1, &event);
	    
		break;
	    }
	case SND_SEQ_EVENT_CONTROLLER:
	    {
		gen_init_aevent( &event, AE_MIDIEVENT, NULL, 0, NULL, 0, gen_get_sampletime() );

		event.d.midiev.len = 3;
		event.d.midiev.midistring[0] = 0xb0 | ev->data.control.channel;

		event.d.midiev.midistring[1] = ev->data.control.param;
		event.d.midiev.midistring[2] = ev->data.control.value;

		gen_send_events(g, 0, -1, &event);
	    
		break;
	    }
	default:
	    break;

    }
	
}
PRIVATE void input_callback( Generator *gold, gint source, GdkInputCondition condition ) {

    Data *data;
    int port;
    Generator *g;
    snd_seq_event_t *ev;

    snd_seq_event_input(seq_client, &ev);

    port = ev->dest.port;

    g = g_hash_table_lookup( clients, &port );
    RETURN_UNLESS( g != NULL );
    data=g->data;
    data->exec_ev( g, ev );

    snd_seq_free_event( ev );
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

  return 1;
}




PRIVATE int init_instance_new(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int ret;
  g->data = data;

  /*
   * open a port on alsa_seq.
   * how do i update the name of the port when generator is renamed ?
   */

  data->exec_ev = execute_event_new;

    ret = snd_seq_create_simple_port(seq_client, 
				     g->name,
				     SND_SEQ_PORT_CAP_WRITE |
				     SND_SEQ_PORT_CAP_SUBS_WRITE,
				     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				     SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;
    data->rdev = NULL;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	return 0;
    }

    g_hash_table_insert( clients, &data->seq_port, g );

  return 1;
}

PRIVATE int init_instance_out(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int ret;
  g->data = data;

  /*
   * open a port on alsa_seq.
   * how do i update the name of the port when generator is renamed ?
   */

  data->exec_ev = execute_event_new;

    ret = snd_seq_create_simple_port(seq_client, 
				     g->name,
				     SND_SEQ_PORT_CAP_READ |
				     SND_SEQ_PORT_CAP_SUBS_READ,
				     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				     SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;
    snd_midi_event_new( 10, &data->rdev );
    snd_midi_event_init( data->rdev );
    snd_midi_event_no_status( data->rdev, 1 );

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	return 0;
    }

    g_hash_table_insert( clients, &data->seq_port, g );

  return 1;
}

PRIVATE int init_instance_old(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int ret;
  g->data = data;

  /*
   * open a port on alsa_seq.
   * how do i update the name of the port when generator is renamed ?
   */

  data->exec_ev = execute_event_old;

    ret = snd_seq_create_simple_port(seq_client, 
				     g->name,
				     SND_SEQ_PORT_CAP_WRITE |
				     SND_SEQ_PORT_CAP_SUBS_WRITE,
				     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				     SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;
    data->rdev = NULL;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	return 0;
    }

    g_hash_table_insert( clients, &data->seq_port, g );

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  g_hash_table_remove( clients, &data->seq_port );

  snd_seq_delete_port( seq_client, data->seq_port );
  if( data->rdev )
      snd_midi_event_free( data->rdev );

  free(g->data);
}


/*
 * pickle and unpickle 
 * 
 * are straight forward....
 * 
 */

PRIVATE void unpickle_instance_old(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
    Data *data = safe_malloc(sizeof(Data));
    int ret;

    g->data = data;

    data->exec_ev = execute_event_old;
    ret = snd_seq_create_simple_port(seq_client, 
	    g->name,
	    SND_SEQ_PORT_CAP_WRITE |
	    SND_SEQ_PORT_CAP_SUBS_WRITE,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	    SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;
    data->rdev = NULL;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	// TODO: make this safe even if port cannot be established
	//       could be in the generator which can have a null generatorclass 
	//       which can be setup by the unpickler....
	return;
    }

    g_hash_table_insert( clients, &data->seq_port, g );
}

PRIVATE void unpickle_instance_new(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
    Data *data = safe_malloc(sizeof(Data));
    int ret;

    g->data = data;

    data->exec_ev = execute_event_new;
    ret = snd_seq_create_simple_port(seq_client, 
	    g->name,
	    SND_SEQ_PORT_CAP_WRITE |
	    SND_SEQ_PORT_CAP_SUBS_WRITE,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	    SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;
    data->rdev = NULL;

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	// TODO: make this safe even if port cannot be established
	//       could be in the generator which can have a null generatorclass 
	//       which can be setup by the unpickler....
	return;
    }

    g_hash_table_insert( clients, &data->seq_port, g );
}

PRIVATE void unpickle_instance_out(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
    Data *data = safe_malloc(sizeof(Data));
    int ret;

    g->data = data;

    data->exec_ev = execute_event_new;
    ret = snd_seq_create_simple_port(seq_client, 
	    g->name,
	    SND_SEQ_PORT_CAP_READ |
	    SND_SEQ_PORT_CAP_SUBS_READ,
	    SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	    SND_SEQ_PORT_TYPE_APPLICATION );
    data->seq_port = ret;

    snd_midi_event_new( 10, &data->rdev );
    snd_midi_event_init( data->rdev );
    snd_midi_event_no_status( data->rdev, 1 );

    if ( ret < 0 ){
	printf( "snd_seq_create_simple_port(read) error\n");
	// TODO: make this safe even if port cannot be established
	//       could be in the generator which can have a null generatorclass 
	//       which can be setup by the unpickler....
	return;
    }

    g_hash_table_insert( clients, &data->seq_port, g );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
}


PRIVATE void evt_midi_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  //int i;

  if( event->kind != AE_MIDIEVENT )
      return;

  snd_seq_event_t seqev;

  snd_seq_ev_clear( &seqev );

  snd_midi_event_encode( data->rdev, event->d.midiev.midistring, event->d.midiev.len, &seqev );
  
  snd_seq_ev_set_subs( &seqev );
  snd_seq_ev_set_source( &seqev, data->seq_port );

  snd_seq_event_output_direct( seq_client, &seqev );
}


/**
 * \brief Setup the class of the midi_in Generator
 */

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, NULL,
					     init_instance_old, destroy_instance,
					     unpickle_instance_old, pickle_instance);

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

  k = gen_new_generatorclass("alsa_seq_in", FALSE, 0, 1,
					     NULL, NULL, NULL,
					     init_instance_new, destroy_instance,
					     unpickle_instance_new, pickle_instance);

  gen_configure_event_output(k, 0,   "Midi Out");
  gencomp_register_generatorclass(k, FALSE, "Misc/Alsa Seq In", NULL, NULL);

  k = gen_new_generatorclass("alsa_seq_out", FALSE, 1, 0,
					     NULL, NULL, NULL,
					     init_instance_out, destroy_instance,
					     unpickle_instance_out, pickle_instance);

  gen_configure_event_input(k, 0,   "Midi In", evt_midi_handler );
  gencomp_register_generatorclass(k, FALSE, "Misc/Alsa Seq Out", NULL, NULL);
}

PUBLIC void init_plugin_alsa_midi(void) {
    if( !open_alsa_seq() )
	return;
    if( !setup_input_handler() )
	return;

  clients=g_hash_table_new( g_int_hash, g_int_equal );
    
  setup_class();
}


