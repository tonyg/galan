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

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#include <unistd.h>

#include <sndfile.h>

#define GENERATOR_CLASS_NAME	"pcm_out"
#define GENERATOR_CLASS_PATH	"Outputs/SndFile Recorder (PCM, Stereo)"

#define SIG_LEFT		0
#define SIG_RIGHT		1

#define EVT_RECORD		0
#define EVT_NAME		1

typedef struct Data {
  char *filename;
  SNDFILE *output;
  SF_INFO setup;
  guint frames_recorded;
} Data;


PRIVATE int output_fragment(SNDFILE *f, SAMPLE *l_buf, SAMPLE *r_buf, int length) {
  SAMPLE *outbuf;
  int buflen = length * sizeof(SAMPLE) * 2;
  int i;

  if (length <= 0)
    return 0;

  outbuf = g_alloca(buflen);

  for (i = 0; i < length; i++) {
    outbuf[i<<1]       = l_buf[i];
    outbuf[(i<<1) + 1] = r_buf[i];
  }

//  i = sf_writef_double(f, outbuf, length);
  i = sf_writef_float(f, outbuf, length);
  return i;
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
      SAMPLE *l_buf, *r_buf;
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      l_buf = g_alloca(bufbytes);
      r_buf = g_alloca(bufbytes);

      if (!gen_read_realtime_input(g, SIG_LEFT, -1, l_buf, event->d.integer))
	memset(l_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT, -1, r_buf, event->d.integer))
	memset(r_buf, 0, bufbytes);

      if (data->output != NULL) {
	int written = output_fragment(data->output, l_buf, r_buf, event->d.integer);
	if (written > 0)
	  data->frames_recorded += written;
      }

      break;
    }

    default:
      g_warning("pcm_out module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->filename = NULL;
  data->output = NULL;
  data->setup.samplerate = SAMPLE_RATE;
  data->setup.channels = 2;
  data->setup.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  data->frames_recorded = 0;

  //afInitFileFormat(data->setup, AF_FILE_WAVE);
  //afInitChannels(data->setup, AF_DEFAULT_TRACK, 2);
  //afInitSampleFormat(data->setup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);

  gen_register_realtime_fn(g, realtime_handler);

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data->filename != NULL)
    free(data->filename);

  if (data->output != NULL)
    sf_close(data->output);

  free(g->data);
}

PRIVATE void access_output_file(GtkWidget *widget, GtkWidget *fs) {
  Generator *g = gtk_object_get_data(GTK_OBJECT(fs), "Generator");
  Data *data = g->data;
  const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));
  FILE *f;

  f = fopen(filename, "rb");

  if (f != NULL) {
    fclose(f);
    if (popup_msgbox("Confirm Overwrite", MSGBOX_YES | MSGBOX_NO, 0, MSGBOX_NO,
		     "The file named %s exists.\nDo you want to overwrite it?",
		     filename) != MSGBOX_YES) {
      return;
    }
  }

  if (data->filename != NULL)
    free(data->filename);
  data->filename = safe_string_dup(filename);

  data->output = sf_open( filename, SFM_WRITE, &data->setup );
  data->frames_recorded = 0;

  if (data->output == NULL) {
    popup_msgbox("Could Not Create File", MSGBOX_OK, 0, MSGBOX_OK,
		 "Could not create output file %s.\n"
		 "Recording cancelled.",
		 filename);
    return;
  }

  sf_command( data->output, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE );

  gtk_widget_destroy(fs);	/* %%% should this be gtk_widget_hide? uber-paranoia */
}

PRIVATE void evt_name_handler( Generator *g, AEvent *event ) {

    Data *data = g->data;
    if( event->kind != AE_STRING ) {
	g_warning( "not a string event when setting name !!!" );
	return;
    }

    	    if( data->filename )
	    	g_free( data->filename );
	    data->filename = safe_string_dup( event->d.string );
}

PRIVATE void evt_record_handler(Generator *g, AEvent *event) {
    gboolean start = (event->d.number > 0.5);
    Data *data = g->data;
    FILE *f;

    if (start) {

	f = fopen(data->filename, "rb");

	data->output = sf_open( data->filename, SFM_WRITE, &data->setup );
	data->frames_recorded = 0;

	//    GtkWidget *fs = gtk_file_selection_new("Select Output WAV File");

	//    gtk_object_set_data(GTK_OBJECT(fs), "Generator", g);
	//    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
	//		       GTK_SIGNAL_FUNC(access_output_file), fs);
	//    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
	//			      GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

	//    if (data->filename != NULL)
	//      gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), data->filename);

	//    gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
	//    gtk_widget_show(fs);
    } else {
	if (data->output != NULL) {
	    sf_close(data->output);
	    data->output = NULL;
	}
    }

	//   popup_msgbox("Recording Complete", MSGBOX_OK, 0, MSGBOX_OK,
	//		 "Recorded %g seconds (%d frames) of data to file\n"
	//		 "%s",
	//		 (float) data->frames_recorded / (SAMPLE_RATE), data->frames_recorded,
	//		 data->filename ? data->filename : "<anonymous>");
	//  }
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Left", SIG_FLAG_REALTIME },
  { "Right", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_TOGGLE, "record", 0,0,0,0, 0,FALSE, TRUE,EVT_RECORD, NULL,NULL,NULL },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, TRUE, 2, 0,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     (AGenerator_pickle_t) init_instance, NULL);

  gen_configure_event_input(k, EVT_RECORD, "Record", evt_record_handler);
  gen_configure_event_input(k, EVT_NAME, "NAME", evt_name_handler);

  gencomp_register_generatorclass(k, TRUE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_sndfile_out(void) {
  setup_class();
}



