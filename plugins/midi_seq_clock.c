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

#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"midiseqclock"
#define GENERATOR_CLASS_PATH	"Misc/MIDI Sequencer Clock"

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
    NUM_EVENT_OUTPUTS
};

typedef struct Data {
  gint fd;  
  gint input_tag;
  SAMPLETIME miditime_offset;
  SAMPLETIME gentime_offset;
  SAMPLETIME last_timestamp;
  gint midibytestocome, midibufpos;
  unsigned char midibuffer[MIDI_BUFSIZE];
  SAMPLETIME buffer_timestamp;
  unsigned char laststatus;
} Data;


/*
 * This is the input callback....
 * Seems to bo ok for now..
 */

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

PRIVATE void execute_midi_command( Generator *g ) {
    
    AEvent event;
    Data *data = g->data;
    int channel = data->laststatus & 0x0f;
    
    g_print( "Executing MIDI Command %d...\n" , (int) data->laststatus );
    switch( data->laststatus & 0xf0 ) {
	case 0x90:
	    {
		// now the note is in data->midibuffer[0] and the velocity is in [1] 
		
		//g_print( "lasttimestamp=%d, gen=%d, diff=%d (%fsec)\n", data->last_timestamp, gen_get_sampletime(), data->last_timestamp - gen_get_sampletime(), ((gdouble)(data->last_timestamp - gen_get_sampletime())) / SAMPLE_RATE );
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );

		event.d.number = channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
		event.d.number = data->midibuffer[0];
		gen_send_events(g, EVT_NOTE, -1, &event);

		event.d.number = data->midibuffer[1];
		gen_send_events(g, EVT_VELOCITY, -1, &event);
	    
		break;
		
	    }
	case 0xc0:
	    {
		gen_init_aevent( &event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );

		event.d.number = channel;
		gen_send_events(g, EVT_CHANNEL, -1, &event);

		//g_print( "(%x,%x)\n", data->midibuffer[0], data->midibuffer[1] );
		event.d.number = data->midibuffer[0];
		gen_send_events(g, EVT_PROGAMCHANGE, -1, &event);
	    }
	    
    }
	
}


PRIVATE void input_callback( Generator *g, gint source, GdkInputCondition condition ) {

    Data *data = g->data;
    unsigned char midievent[4];
    AEvent event;
    int l,i;

    l = read( source, &midievent, 4 );

    for( i=0; i<l; i+=4 ) {
	//g_print( "%d midiev: %d, %x\n", i, (int) midievent[i], *((int *) &midievent[i]) & 0xffffff00 );
	switch( midievent[i] ) {
	    case SEQ_WAIT:
		//g_print( "TMR_WAIT_ABS: %d\n",  *((int *) &midievent[i]) >> 8 );
		if( data->miditime_offset == -1 ) {
		    data->miditime_offset = (*((int *) midievent) >> 8)  * SAMPLE_RATE / 100;
		    data->gentime_offset = gen_get_sampletime();
		}
		data->last_timestamp = (*((int *) &midievent[i]) >> 8) * SAMPLE_RATE / 100
		    - data->miditime_offset + data->gentime_offset;

		break;
	    case SEQ_MIDIPUTC:
		switch( midievent[1] & 0xf0 ) {
		    case 0xf0:
			switch( midievent[1] ) {
			    case 0xf8:
				gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
				event.d.number = 1;
				gen_send_events(g, EVT_CLOCK, -1, &event);
				break;

			    case 0xfa:
				gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, data->last_timestamp );
				event.d.number = 1;
				gen_send_events(g, EVT_START, -1, &event);
				break;
			}
			break;

		    case 0xe0:
		    case 0xd0:
		    case 0xc0:
		    case 0xb0:
		    case 0xa0:
		    case 0x80:
		    case 0x90:
			data->laststatus = midievent[1];
			data->midibytestocome = get_bytes_to_come( data->laststatus );
			//g_print( "playevent here\n" );
			data->midibufpos = 0;
			
			break;

		    default:
			//g_print( "in default... %d ls=%x\n", data->midibytestocome, (int) data->laststatus );
			if( data->midibytestocome ) {
			    
			    data->midibuffer[data->midibufpos++] = midievent[1];

			    if( (data->midibytestocome--) == 1 ) {

				// Now there is a midicommand in data->midibuffer
				// status byte is data->laststatus

				execute_midi_command( g );
			    }

			} else {

			    // running command.. set bytes_to_come to 
			    data->midibytestocome = get_bytes_to_come( data->laststatus ) - 1;
			    data->midibufpos = 0;

			    // Store this byte...
			    data->midibuffer[data->midibufpos++] = midievent[1];
			}
			    


		}
		break;
	}
    }
}


/*
 * Constuctor and Destructor 
 *
 * I have to add a global-property to get /dev/input/js0 from config.
 * Of course make it configurable in plugin-properties.
 *
 * then i need asserts... But hey it works now :-)
 * Instance can be added even if configured for js1 and only js0 
 * available (??? confusing having non working component  )
 * 
 */

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->fd = open( "/dev/sequencer", O_RDWR );
  if( data->fd > 0 ) {
      free( data );
      return 0;
  }
  data->miditime_offset = -1;
  data->last_timestamp = gen_get_sampletime();
  data->midibytestocome = 0;
  data->laststatus = 0;

//  ioctl( data->fd, SNDCTL_SEQ_ACTSENSE_ENABLE, 0 );
//  ioctl( data->fd, SNDCTL_SEQ_TIMING_ENABLE, 0 );
//  ioctl( data->fd, SNDCTL_SEQ_RT_ENABLE, 0 );
      
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction)  input_callback, (gpointer) g ); 
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gdk_input_remove( data->input_tag );
  close( data->fd );

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
  g->data = data;

  data->fd = open( "/dev/sequencer", O_RDONLY | O_NONBLOCK );
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction) input_callback, g ); 

  data->miditime_offset = -1;
  data->last_timestamp = gen_get_sampletime();
  data->midibytestocome = 0;
  data->laststatus = 0;

}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  //Data *data = g->data;
}


PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
     { CONTROL_KIND_NONE, }
};


/*
 * setup Class
 *
 * much TODO here....
 *
 * check how many buttons and axes we have...
 * can i change this dynamicly ???
 * 
 */

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_output(k, EVT_CLOCK,   "Clock");
  gen_configure_event_output(k, EVT_START,   "Start");
  gen_configure_event_output(k, EVT_CHANNEL,   "Channel");
  gen_configure_event_output(k, EVT_NOTE,   "Note");
  gen_configure_event_output(k, EVT_VELOCITY,   "Velocity");
  gen_configure_event_output(k, EVT_PROGAMCHANGE,   "Program");
  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_midi_seq_clock(void) {
  setup_class();
}


