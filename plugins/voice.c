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

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#ifdef HAVE_AUDIOFILE_H
#include <audiofile.h>
#endif

#define GENERATOR_CLASS_NAME	"voice"
#define GENERATOR_CLASS_PATH	"Sources/Sampled Voice"

typedef struct Data {
  char *filename;
  SAMPLE *sample;
  int len;
  gboolean store_sample;
  gboolean treat_as_raw;
} Data;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->filename = NULL;
  data->sample = NULL;
  data->len = 0;
  data->store_sample = TRUE;

#ifdef HAVE_AUDIOFILE_H
  data->treat_as_raw = FALSE;
#else
  data->treat_as_raw = TRUE;
#endif

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data->filename != NULL)
    free(data->filename);

  if (data->sample != NULL)
    free(data->sample);

  free(g->data);
}

PRIVATE gboolean try_load(Generator *g, const char *filename, gboolean verbose) {
  Data *data = g->data;
  gint16 *inbuf;
  int nsamples = 0;
  int i;
  gboolean success = FALSE;
  /* For RAW loading: */
  FILE *f = NULL;
  long pos;
  /* For audiofile loading: */
#ifdef HAVE_AUDIOFILE_H
  AFfilehandle affile = NULL;
  AFframecount framecount = 0;
  int sampleformat, samplewidth, channelcount;
#endif

  if (data->treat_as_raw) {
    f = fopen(filename, "rb");
    success = (f != NULL);
  } else {
#ifdef HAVE_AUDIOFILE_H
    affile = afOpenFile(filename, "rb", NULL);
    success = (affile != NULL);
#endif
  }

  if (!success) {
    if (verbose)
      popup_msgbox("Load Error", MSGBOX_OK, 0, MSGBOX_OK,
		   "Could not open audio file %s", filename);
    return FALSE;
  }

  if (data->treat_as_raw) {
    fseek(f, 0, SEEK_END);
    pos = ftell(f);
    fseek(f, 0, SEEK_SET);
    nsamples = pos / sizeof(gint16);
  } else {
#ifdef HAVE_AUDIOFILE_H
    channelcount = afGetChannels(affile, AF_DEFAULT_TRACK);
    afGetSampleFormat(affile, AF_DEFAULT_TRACK, &sampleformat, &samplewidth);
    framecount = afGetFrameCount(affile, AF_DEFAULT_TRACK);

    if (channelcount != 1 || samplewidth != 16) {
      if (verbose)
	popup_msgbox("File Format Problem", MSGBOX_OK, 0, MSGBOX_OK,
		     "The audio file must be 44.1kHz 16-bit mono.");
      afCloseFile(affile);
      return FALSE;
    }

    nsamples = framecount;
#endif
  }

  inbuf = malloc(sizeof(gint16) * nsamples);

  if (inbuf == NULL) {
    if (verbose)
      popup_msgbox("Memory Error", MSGBOX_OK, 0, MSGBOX_OK,
		   "Could not allocate enough memory to store the sample.");
    if (data->treat_as_raw) {
      fclose(f);
    } else {
#ifdef HAVE_AUDIOFILE_H
      afCloseFile(affile);
#endif
    }
    return FALSE;
  }

  if (data->treat_as_raw) {
    fread(inbuf, sizeof(gint16), nsamples, f);
    fclose(f);
  } else {
#ifdef HAVE_AUDIOFILE_H
    afReadFrames(affile, AF_DEFAULT_TRACK, inbuf, framecount);
    afCloseFile(affile);
#endif
  }

  if (data->sample != NULL)
    free(data->sample);

  data->len = nsamples;
  data->sample = malloc(sizeof(SAMPLE) * data->len);

  if (data->sample == NULL) {
    if (verbose)
      popup_msgbox("Memory Error", MSGBOX_OK, 0, MSGBOX_OK,
		   "Could not allocate enough memory to convert the sample.");
    free(inbuf);
    return FALSE;
  }

  for (i = 0; i < data->len; i++)
    data->sample[i] = inbuf[i] / 32768.0;

  free(inbuf);
  return TRUE;
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int i, len;
  gint16 *buf;
  gint32 binarylength;

  g->data = data;

  data->filename = objectstore_item_get_string(item, "voice_filename", NULL);
  data->len = len = objectstore_item_get_integer(item, "voice_length", 0);
  binarylength = objectstore_item_get_binary(item, "voice_sample", (void **) &buf);
  data->store_sample = objectstore_item_get_integer(item, "voice_store_sample", 1);
  data->treat_as_raw = objectstore_item_get_integer(item, "voice_bypass_libaudiofile", 0);
  data->sample = NULL;

#ifndef HAVE_AUDIOFILE_H
  if (!data->treat_as_raw) {
    popup_msgbox("Warning", MSGBOX_OK, 0, MSGBOX_OK,
		 "Treating sample files as RAW due to missing library \"audiofile\".");
    data->treat_as_raw = 1;
  }
#endif

  if (data->filename != NULL)
    data->filename = safe_string_dup(data->filename);

  if (binarylength != -1) {
    data->sample = safe_malloc(sizeof(SAMPLE) * len);
    for (i = 0; i < len; i++)
      data->sample[i] = ((gint16) g_ntohs(buf[i])) / 32768.0;
  } else if (data->filename != NULL) {
    try_load(g, data->filename, FALSE);
  }
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;

  objectstore_item_set_integer(item, "voice_bypass_libaudiofile", data->treat_as_raw);
  objectstore_item_set_integer(item, "voice_store_sample", data->store_sample);
  if (data->filename != NULL)
    objectstore_item_set_string(item, "voice_filename", data->filename);

  if (data->store_sample) {
    int bytelength = sizeof(gint16) * data->len;
    gint16 *buf = safe_malloc(bytelength);
    int i;

    objectstore_item_set_integer(item, "voice_length", data->len);

    for (i = 0; i < data->len; i++)
      buf[i] = g_htons(MAX(-32768, MIN(32767, 32768 * data->sample[i])));

    objectstore_item_set(item, "voice_sample",
			 objectstore_datum_new_binary(bytelength, (void *) buf));

    free(buf);
  }
}

PRIVATE SAMPLETIME output_range(Generator *g, OutputSignalDescriptor *sig) {
  return ((Data *) g->data)->len;
}

PRIVATE gboolean output_generator(Generator *g, OutputSignalDescriptor *sig,
				  SAMPLETIME offset, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int len, sil;

  if (data->len == 0 || offset >= data->len)
    return FALSE;

  len = MIN(MAX(data->len - offset, 0), buflen);
  if (len > 0)
    memcpy(buf, &data->sample[offset], len * sizeof(SAMPLE));

  sil = buflen - len;
  memset(&buf[len], 0, sil * sizeof(SAMPLE));
  return TRUE;
}

PRIVATE void evt_name_handler( Generator *g, AEvent *event ) {

    Data *data = g->data;
    if( event->kind != AE_STRING ) {
	g_warning( "not a string event when setting name !!!" );
	return;
    }

    if( try_load( g, event->d.string, FALSE ) ) {
    	    if( data->filename )
	    	g_free( data->filename );
	    data->filename = safe_string_dup( event->d.string );
    }
}

PRIVATE void load_new_sample(GtkWidget *widget, GtkWidget *fs) {
  Generator *g = gtk_object_get_data(GTK_OBJECT(fs), "Generator");
  GtkWidget *label = gtk_object_get_data(GTK_OBJECT(fs), "FilenameLabel");
  Data *data = g->data;
  const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  if (try_load(g, filename, TRUE)) {
    if (data->filename != NULL)
      free(data->filename);

    data->filename = safe_string_dup(filename);
    gtk_label_set_text(GTK_LABEL(label), data->filename);

    gtk_widget_destroy(fs);	/* %%% should this be gtk_widget_hide? uber-paranoia */
  }
}

PRIVATE void choose_clicked(GtkWidget *choose_button, Generator *g) {
  GtkWidget *fs = gtk_file_selection_new("Open Sample File");
  Data *data = g->data;

  gtk_object_set_data(GTK_OBJECT(fs), "Generator", g);
  gtk_object_set_data(GTK_OBJECT(fs), "FilenameLabel",
		      gtk_object_get_data(GTK_OBJECT(choose_button), "FilenameLabel"));

  if (data->filename)
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), data->filename);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		     GTK_SIGNAL_FUNC(load_new_sample), fs);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

  gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
  gtk_widget_show(fs);
}

PRIVATE void reload_clicked(GtkWidget *choose_button, Generator *g) {
  Data *data = g->data;

  if (data->filename != NULL)
    try_load(g, data->filename, TRUE);
}

PRIVATE void raw_toggled(GtkWidget *raw_toggle, Generator *g) {
  Data *data = g->data;
  data->treat_as_raw = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(raw_toggle));
}

PRIVATE void propgen(Component *c, Generator *g) {
  Data *data = g->data;
  GtkWidget *vb = gtk_vbox_new(FALSE, 5);
  GtkWidget *hb = gtk_hbox_new(TRUE, 5);
  GtkWidget *frame = gtk_frame_new("Sample File");
  GtkWidget *label = gtk_label_new(data->filename ? data->filename : "<none>");
  GtkWidget *reload_button = gtk_button_new_with_label("Reload");
  GtkWidget *choose_button = gtk_button_new_with_label("Choose File...");
  GtkWidget *save_toggle = gtk_check_button_new_with_label("Save sample with output");
  GtkWidget *raw_toggle = gtk_check_button_new_with_label("Use RAW format - bypass libaudiofile\n"
							  "(Assume 44.1kHz signed 16-bit mono)");

  gtk_container_add(GTK_CONTAINER(frame), vb);
  gtk_widget_show(vb);

  gtk_box_pack_start(GTK_BOX(vb), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  gtk_box_pack_start(GTK_BOX(vb), hb, TRUE, TRUE, 0);
  gtk_widget_show(hb);

  gtk_box_pack_start(GTK_BOX(vb), save_toggle, TRUE, TRUE, 0);
  gtk_widget_show(save_toggle);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_toggle), data->store_sample);

#ifdef HAVE_AUDIOFILE_H
  /* Don't ask unless we have audiofile. */
  gtk_box_pack_start(GTK_BOX(vb), raw_toggle, TRUE, TRUE, 0);
  gtk_widget_show(raw_toggle);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(raw_toggle), data->treat_as_raw);
#endif

  gtk_box_pack_start(GTK_BOX(hb), reload_button, TRUE, TRUE, 0);
  gtk_widget_show(reload_button);
  gtk_box_pack_start(GTK_BOX(hb), choose_button, TRUE, TRUE, 0);
  gtk_widget_show(choose_button);

  gtk_signal_connect(GTK_OBJECT(reload_button), "clicked",
		     GTK_SIGNAL_FUNC(reload_clicked), g);
  gtk_signal_connect(GTK_OBJECT(choose_button), "clicked",
		     GTK_SIGNAL_FUNC(choose_clicked), g);
  gtk_object_set_data(GTK_OBJECT(choose_button), "FilenameLabel", label);

  gtk_signal_connect(GTK_OBJECT(raw_toggle), "toggled",
		     GTK_SIGNAL_FUNC(raw_toggled), g);

  popup_dialog("Properties", MSGBOX_DISMISS, 0, MSGBOX_DISMISS, frame, NULL, 0);

  data->store_sample = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_toggle));
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_RANDOMACCESS, { NULL, { output_range, output_generator } } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 0,
					     NULL, output_sigs, NULL,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, 0, "Filename", evt_name_handler);

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, propgen);
}

PUBLIC void init_plugin_voice(void) {
  setup_class();
}
