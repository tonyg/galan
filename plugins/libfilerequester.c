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


#define GENERATOR_CLASS_NAME	"filereq"
#define GENERATOR_CLASS_PATH	"Controls/File Requester"


#define EVT_REQUEST		0
#define EVT_FILENAME		0

typedef struct Data {
  char *filename;

  GThread *thread;
  GAsyncQueue *req;
} Data;

PRIVATE void access_output_file(GtkWidget *widget, GtkWidget *fs) {
    
    AEvent event;
    Generator *g = gtk_object_get_data(GTK_OBJECT(fs), "Generator");
    Data *data = g->data;
    const char *filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

    if (data->filename != NULL)
	free(data->filename);
    data->filename = safe_string_dup(filename);

    gtk_widget_destroy(fs);	/* %%% should this be gtk_widget_hide? uber-paranoia */

    gen_init_aevent(&event, AE_STRING, NULL, 0, NULL, 0, gen_get_sampletime() );
    event.d.string = safe_string_dup( data->filename );
    gen_send_events(g, EVT_FILENAME, -1, &event);
}

PRIVATE gpointer req_thread( Generator *g ) {

    Data *data = g->data;
    GtkWidget *fs;

    while( 1 ) {
	gpointer ptr = g_async_queue_pop( data->req );
	if( ptr == NULL )
	    break;
	else {

	    gdk_threads_enter();
	    
	    fs = gtk_file_selection_new("Select File");

	    gtk_object_set_data(GTK_OBJECT(fs), "Generator", g);
	    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		    GTK_SIGNAL_FUNC(access_output_file), fs);
	    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
		    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

	    if (data->filename != NULL)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), data->filename);

	    gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
	    gtk_widget_show(fs);

	    gdk_threads_leave();
	}
	
    }
    return NULL;
}



PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  g->data = data;

  data->filename = NULL;

  data->req = g_async_queue_new();
  data->thread = g_thread_create( (GThreadFunc) req_thread, (gpointer) g, TRUE, NULL );

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data->filename != NULL)
    free(data->filename);

  g_async_queue_push( data->req, NULL );

  free(g->data);
}


PRIVATE void evt_request_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  g_async_queue_push( data->req, (gpointer) 1 );
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { NULL, }
};

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { NULL, }
};

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_BUTTON, "req", 0,0,0,0, 0,FALSE, TRUE,EVT_REQUEST, NULL,NULL,NULL },
  { CONTROL_KIND_NONE, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     (AGenerator_pickle_t) init_instance, NULL);

  gen_configure_event_input(k, EVT_REQUEST, "Req", evt_request_handler);
  gen_configure_event_output(k, EVT_FILENAME, "Filename");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_filerequester(void) {
  setup_class();
}
