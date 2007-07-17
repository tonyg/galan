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

typedef struct Buffer {
    SAMPLETIME timestamp;
    SAMPLETIME len;
    SAMPLE *lbuffer, *rbuffer;
    char *new_name;
    gboolean atend;
} Buffer;

typedef struct Data {
  FILE *streamfd;
  OggVorbis_File vf;
  SAMPLE *lbuffer, *rbuffer;
  SAMPLE *lbuffer2, *rbuffer2;
  Buffer *buffers[2];
  Buffer *current_buf;
  SAMPLETIME len;
  SAMPLETIME curr_timestamp;
  gboolean bufferfull;
  gboolean playing;
  gboolean atend;
  gboolean thread_atend;
  gchar *curstreamname;

  gboolean done_thread;
  GThread *thread;
  GAsyncQueue *done, *free;
} Data;

//PRIVATE char curstreamname[259];

PRIVATE Buffer *buffer_new( int len ) {

    Buffer *retval = safe_malloc( sizeof( Buffer ) );

    retval->len = len;
    retval->lbuffer = safe_malloc( sizeof( SAMPLE ) * len );
    retval->rbuffer = safe_malloc( sizeof( SAMPLE ) * len );
    retval->new_name = NULL;
    retval->atend = FALSE;
    
    return retval;
}

PRIVATE void buffer_change_len( Buffer *buf, SAMPLETIME newlen ) {

    safe_free( buf->lbuffer );
    safe_free( buf->rbuffer );
    buf->len = newlen;
    buf->lbuffer = safe_malloc( sizeof( SAMPLE ) * newlen );
    buf->rbuffer = safe_malloc( sizeof( SAMPLE ) * newlen );
}

PRIVATE void buffer_free( Buffer *buf ) {
    safe_free( buf->lbuffer );
    safe_free( buf->rbuffer );
    if( buf->new_name )
	free( buf->new_name );

    safe_free( buf );
}

PRIVATE void buffer_clear( Buffer *buf ) {
    memset( buf->lbuffer, 0, sizeof( SAMPLE ) * buf->len );
    memset( buf->rbuffer, 0, sizeof( SAMPLE ) * buf->len  );
}

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

  //data->bufferfull = FALSE;
  data->thread_atend = FALSE;


  return TRUE;
}

PRIVATE void close_stream( Data *data ) {

    if( data->playing ) {
	ov_clear( &(data->vf) );
	data->playing = FALSE;
    }
}


PRIVATE void fill_buffer( Generator *g, Buffer *buf ) {
    int i,j,y, current_selection;
    int numBufSamples, readNSamples;
    gboolean end;
    
    OUTPUTSAMPLE buff[2048];
    Data *data = g->data;

    if( data->thread_atend )
	return;
    
    if( !data->playing )
	return;

    end = y = 0;

    do {
	buf->atend = FALSE;
	readNSamples = MIN(buf->len - y, 1024) * 2;
	i = ov_read( &(data->vf), (char *)buff, readNSamples * sizeof(OUTPUTSAMPLE), 0,2,1, &current_selection );

	if( i == 0 ) {
	    if( y == 0 ) {
		data->thread_atend = TRUE;
		buf->atend = TRUE;
	    } else {
		if( y < buf->len ) {
		    memset( &(buf->lbuffer[y]), 0, sizeof( SAMPLE ) * (buf->len - y ) );
		    memset( &(buf->rbuffer[y]), 0, sizeof( SAMPLE ) * (buf->len - y ) );
		    y = buf->len;
		}
		
	    }
	    end = TRUE;
	    
	    //ev->d.number = 1;
	    //gen_send_events(g, EVT_ATEND, -1, ev);
	}

	// Now i have buffsize Bytes which i have to put into my len array.
	
	numBufSamples = i/sizeof(OUTPUTSAMPLE)/2;

	if( (y + numBufSamples) <= buf->len ) {

	    for( j=0; j<numBufSamples; j++ ) {

		buf->lbuffer[y + j] = buff[j*2]   / 32768.0;
		buf->rbuffer[y + j] = buff[j*2+1] / 32768.0;
	    }
	    y += numBufSamples;
	}
	else {
	    printf( "soo... das ist erstmal voll hier...\n" );
	    end = TRUE;
	}
    } while( (!end) && (y < buf->len) );

    //data->bufferfull = TRUE;
}

/**
 * \brief file read and uncompress.
 *
 * \param Generator g
 *
 * expects initialised buffers and an open ov_stream.
 */

PRIVATE gpointer threado( Generator *g ) {

    Data *data = g->data;
    SAMPLETIME curr_playtime = 0;

    if( data->playing ) {
	fill_buffer( g, data->buffers[0] );
	curr_playtime += data->buffers[0]->len;
    } else 
	buffer_clear( data->buffers[0] );

    g_async_queue_push( data->done, data->buffers[0] );

    if( data->playing ) {
	fill_buffer( g, data->buffers[1] );
	curr_playtime += data->buffers[1]->len;
    } else 
	buffer_clear( data->buffers[1] );

    g_async_queue_push( data->done, data->buffers[1] );

    while( !data->done_thread ) {

	Buffer *buf = g_async_queue_pop( data->free );

	//g_print( "play_time = %d\n", curr_playtime );
	
	if( buf->new_name ) {

		close_stream( data );
		free( data->curstreamname );
		data->curstreamname = buf->new_name;
		buf->new_name = NULL;
		data->playing = open_stream( data );
	}


	if( buf->timestamp != curr_playtime ) {
	    //g_print( "skip\n" );
	    ov_time_seek_page( &(data->vf), ((gdouble)buf->timestamp)/44100 );
	    curr_playtime = buf->timestamp;
	}
	
	fill_buffer( g, buf );
	curr_playtime += buf->len;

	g_async_queue_push( data->done, buf );
    }

    return NULL;
}

PRIVATE gboolean init_instance(Generator *g) {

  Data *data = safe_malloc(sizeof(Data));
  
  g->data = data;

  data->len = SAMPLE_RATE;
  data->playing = FALSE;
  data->curstreamname = NULL;
  data->bufferfull = FALSE;
  data->done_thread = FALSE;
  data->curr_timestamp = 0;
  

  data->buffers[0] = buffer_new( data->len );
  data->buffers[1] = buffer_new( data->len );

  data->done = g_async_queue_new();
  data->free = g_async_queue_new();

  data->thread = g_thread_create( (GThreadFunc) threado, (gpointer) g, TRUE, NULL );
  

//  return init_common( data );
  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data != NULL) {

    data->done_thread = TRUE;
    g_thread_join( data->thread );

    close_stream( data );

    g_async_queue_unref( data->done );
    g_async_queue_unref( data->free );
    buffer_free( data->buffers[0] );
    buffer_free( data->buffers[1] );
    safe_free(data);
  }
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->len = objectstore_item_get_integer(item, "ogg_ra_len", 44100);
  data->curr_timestamp = objectstore_item_get_integer( item, "ogg_ra_curr_timestamp", 0 );
  data->playing = objectstore_item_get_integer( item, "ogg_ra_playing", FALSE );
  data->curstreamname = safe_string_dup( objectstore_item_get_string( item, "ogg_ra_curstreamname", NULL ) );
  data->done_thread = FALSE;

  data->buffers[0] = buffer_new( data->len );
  data->buffers[1] = buffer_new( data->len );

  data->done = g_async_queue_new();
  data->free = g_async_queue_new();

  data->bufferfull = FALSE;
  
  if( data->playing )
      data->playing = open_stream( data );

  data->thread = g_thread_create( (GThreadFunc) threado, (gpointer) g, TRUE, NULL );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  objectstore_item_set_integer(item, "ogg_ra_len", data->len);
  objectstore_item_set_integer( item, "ogg_ra_curr_timestamp", data->curr_timestamp );
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

  if( !data->bufferfull ) {
      data->current_buf = g_async_queue_try_pop( data->done );
      if( !data->current_buf ) {
	  return FALSE;
      } else {
	  data->bufferfull = TRUE;
	  if( data->current_buf->atend ) {
	      data->atend = TRUE;
	      return FALSE;
	  }
      }
  }
  
  if (!data->current_buf || data->current_buf->len == 0 || offset >= data->current_buf->len || !data->bufferfull )
    return FALSE;

  len = MIN(MAX(data->current_buf->len - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->current_buf->lbuffer[offset], len * sizeof(SAMPLE));

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

  if( !data->bufferfull ) {
      data->current_buf = g_async_queue_try_pop( data->done );
      if( !data->current_buf ) {
	  return FALSE;
      } else {
	  data->bufferfull = TRUE;
	  if( data->current_buf->atend ) {
	      data->atend = TRUE;
	      return FALSE;
	  }
      }
  }
  
  if (!data->current_buf || data->current_buf->len == 0 || offset >= data->current_buf->len || !data->bufferfull )
    return FALSE;

  len = MIN(MAX(data->current_buf->len - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->current_buf->rbuffer[offset], len * sizeof(SAMPLE));

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
    
    Data *data = g->data;

    if( data->atend )
	return;
    
//    if( !data->playing )
//	return;

    if( data->bufferfull ) {

	data->current_buf->timestamp = (data->curr_timestamp += data->len);
	//g_print( "push.timestamp = %d\n", data->current_buf->timestamp );
	g_async_queue_push( data->free, data->current_buf );
    }
	//g_print( "buffer empty\n" );

    data->current_buf = g_async_queue_try_pop( data->done );

    if( data->current_buf ) {

	data->bufferfull = TRUE;
	if( data->current_buf->atend )
	    data->atend = TRUE;

    } else {

	data->bufferfull = FALSE;

    }

}

PRIVATE void evt_bufflen_handler (Generator *g, AEvent *event) {

    Data *data = g->data;

    // own both buffers to stop reader thread.
    // the data->current_buffer is already owned by the main thread

    if( !data->bufferfull ) {
	data->current_buf = g_async_queue_pop( data->done );
    }
	
    Buffer *tmp = g_async_queue_pop( data->done );

    //g_async_queue_pop( data->done );
    

    data->len = event->d.number * SAMPLE_RATE;

    buffer_change_len( data->buffers[0], data->len );
    buffer_change_len( data->buffers[1], data->len );
    data->bufferfull = FALSE;
    data->current_buf->timestamp = (data->curr_timestamp += data->len);
    g_async_queue_push( data->free, data->current_buf );
    tmp->timestamp = (data->curr_timestamp += data->len);
    g_async_queue_push( data->free, tmp );
}

PRIVATE void evt_seek_handler (Generator *g, AEvent *event) {

    Data *data = g->data;

    if( !data->playing )
	return;

    data->curr_timestamp = event->d.number * SAMPLE_RATE;

    data->atend = FALSE;
    data->thread_atend = FALSE;
}

PRIVATE void evt_name_handler( Generator *g, AEvent *event ) {

    Data *data = g->data;
    if( event->kind != AE_STRING ) {
	g_warning( "not a string event when setting name !!!" );
	return;
    }
    g_print( "changing name to: %s\n", event->d.string );

    // FIXME: better use a name_change_pending flag.
    // and use switch buffer to change the name.
    if( !data->bufferfull ) {
	data->current_buf = g_async_queue_pop( data->done );
	data->bufferfull = TRUE;
    }

    data->current_buf->new_name = safe_string_dup( event->d.string );
    data->curr_timestamp = 0;
	
    data->atend = FALSE;
    data->thread_atend = FALSE;
    //data->playing = TRUE;
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

    if (!g_thread_supported ()) g_thread_init (NULL);
    
    setup_class();
}

