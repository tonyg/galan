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

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

enum Outputs {
    
    SIG_LEFT = 0,
    SIG_RIGHT,
    NUM_OUTPUTS
};

enum OutpuEvents {
    EVT_ATEND = 0,
    NUM_EVENT_OUTPUTS
};

typedef signed short OUTPUTSAMPLE;

typedef struct Data {
  FILE *streamfd;
  OggVorbis_File vf;
  SAMPLE *lbuffer, *rbuffer;
  SAMPLETIME len;
  gboolean bufferfull;
  gboolean playing;
  gboolean atend;
  gchar *curstreamname;
} Data;

//PRIVATE char curstreamname[259];

PRIVATE gboolean open_stream( Data *data ) {

  //data->lbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
  //data->rbuffer = safe_malloc( data->len * sizeof(SAMPLE) );

  data->streamfd = fopen( data->curstreamname, "rb" );

  if(data->streamfd == NULL) {

    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open file, %s.", data->curstreamname);
    return FALSE;
  }

  if(ov_open(data->streamfd, &(data->vf), NULL, 0) < 0) {

      fclose( data->streamfd );

      popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
	    "Not an ogg stream: %s.", data->curstreamname);

      return FALSE;
  }

  data->bufferfull = FALSE;
  data->atend = FALSE;


  return TRUE;
}

PRIVATE void close_stream( Data *data ) {

    if( data->playing ) {
	ov_clear( &(data->vf) );
	data->playing = FALSE;
    }
}

PRIVATE gboolean init_instance(Generator *g) {

  Data *data = safe_malloc(sizeof(Data));
  
  data->len = 44100;
  data->playing = FALSE;
  data->curstreamname = NULL;
  data->bufferfull = FALSE;
  data->lbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
  data->rbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
  
  g->data = data;

//  return init_common( data );
  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data != NULL) {
    close_stream( data );

    free(data->lbuffer);
    free(data->rbuffer);
    free(data);
  }
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->len = objectstore_item_get_integer(item, "ogg_ra_len", 44100);
  data->playing = objectstore_item_get_integer( item, "ogg_ra_playing", FALSE );
  data->curstreamname = safe_string_dup( objectstore_item_get_string( item, "ogg_ra_curstreamname", NULL ) );
  data->lbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
  data->rbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
  if( data->playing )
      data->playing = open_stream( data );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "ogg_ra_len", data->len);
  objectstore_item_set_integer(item, "ogg_ra_playing", data->playing );
  objectstore_item_set_string (item, "ogg_ra_curstreamname", data->curstreamname );
}

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  return ((Data *) g->data)->len;
}

PRIVATE gboolean leftoutput_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int len, sil;

  if( !data->playing )
      return FALSE;

  if( data->atend )
      return FALSE;

  if (data->len == 0 || offset >= data->len || !data->bufferfull )
    return FALSE;

  len = MIN(MAX(data->len - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->lbuffer[offset], len * sizeof(SAMPLE));

  sil = buflen - len;
  memset(&buf[len], 0, sil * sizeof(SAMPLE));
  return TRUE;
}

PRIVATE gboolean rightoutput_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int len, sil;

  if( !data->playing )
      return FALSE;

  if( data->atend )
      return FALSE;

  if (data->len == 0 || offset >= data->len || !data->bufferfull )
    return FALSE;

  len = MIN(MAX(data->len - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->rbuffer[offset], len * sizeof(SAMPLE));

  sil = buflen - len;
  memset(&buf[len], 0, sil * sizeof(SAMPLE));
  return TRUE;
}

PRIVATE void evt_fillbuffer( Generator *g, AEvent *ev ) {

    /*
     * evt_fillbuffer
     *
     * FIXME: does not check for number of channels
     *
     *
     * 
     */



    
    int i,j,y, current_selection;
    int numBufSamples, readNSamples;
    gboolean end;
    
    OUTPUTSAMPLE buff[2048];
    Data *data = g->data;

    if( data->atend )
	return;
    
    if( !data->playing )
	return;

    end = y = 0;

    do {
	readNSamples = MIN(data->len - y, 1024) * 2;
	i = ov_read( &(data->vf), (char *)buff, readNSamples * sizeof(OUTPUTSAMPLE), 0,2,1, &current_selection );

	if( i == 0 ) {
	    data->atend = TRUE;
	    end = TRUE;
	    
	    ev->d.number = 1;
	    gen_send_events(g, EVT_ATEND, -1, ev);
	}

	// Now i have buffsize Bytes which i have to put into my len array.
	
	numBufSamples = i/sizeof(OUTPUTSAMPLE)/2;

	if( (y + numBufSamples) <= data->len ) {

	    for( j=0; j<numBufSamples; j++ ) {

		data->lbuffer[y + j] = buff[j*2]   / 32768.0;
		data->rbuffer[y + j] = buff[j*2+1] / 32768.0;
	    }
	    y += numBufSamples;
	}
	else {
	    printf( "soo... das ist erstmal voll hier...\n" );
	    end = TRUE;
	}
    } while( (!end) && (y < data->len) );

    data->bufferfull = TRUE;
}

PRIVATE void evt_bufflen_handler (Generator *g, AEvent *event) {

    Data *data = g->data;

    data->len = event->d.number * SAMPLE_RATE;
    free( data->lbuffer );
    free( data->rbuffer );
    data->lbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
    data->rbuffer = safe_malloc( data->len * sizeof(SAMPLE) );
    data->bufferfull = FALSE;
}

PRIVATE void evt_seek_handler (Generator *g, AEvent *event) {

    Data *data = g->data;

    if( !data->playing )
	return;

    data->atend = FALSE;

    ov_time_seek_page( &(data->vf), event->d.number );
}

PRIVATE void evt_name_handler( Generator *g, AEvent *event ) {

    Data *data = g->data;
    if( event->kind != AE_STRING ) {
	g_warning( "not a string event when setting name !!!" );
	return;
    }

    if( data->playing ) {
	if( strcmp( data->curstreamname, event->d.string ) ) {

	    close_stream( data );
	    data->curstreamname = safe_string_dup( event->d.string );
	    data->playing = open_stream( data );
	}
    } else {
	data->curstreamname = safe_string_dup( event->d.string );
	data->playing = open_stream( data );
    }
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Left", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, leftoutput_generator } } },
  { "Right", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, rightoutput_generator } } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k;

  //char *name = prefs_get_item("input_ogg_file");
  //sprintf(curstreamname, "%s", name ? name : "/data/mp3/vera/perlon2000-14Tracks/track01.ogg");

  //prefs_register_option("input_ogg_file", "/data/mp3/vera/perlon2000-14Tracks/track01.ogg");
  //prefs_register_option("input_ogg_file", "/data/mp3/vera/perlon2000-14Tracks/track02.ogg");
  //prefs_register_option("input_ogg_file", "/data/mp3/vera/perlon2000-14Tracks/track03.ogg");
  //prefs_register_option("input_ogg_file", "/home/torben/bla.ogg");


  k = gen_new_generatorclass("ogg_ra", FALSE, 4, NUM_EVENT_OUTPUTS,
			     NULL, output_sigs, NULL,
			     init_instance, destroy_instance,
			     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, 0, "Fill Buffer", evt_fillbuffer);
  gen_configure_event_input(k, 1, "Buffer Length", evt_bufflen_handler);
  gen_configure_event_input(k, 2, "Seek", evt_seek_handler);
  gen_configure_event_input(k, 3, "Streamname", evt_name_handler);

  gen_configure_event_output( k, EVT_ATEND, "AtEnd" );

  gencomp_register_generatorclass(k, FALSE, "Sources/OGG Input RA",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_ogg_ra(void) {
  setup_class();
}

