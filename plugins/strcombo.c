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

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"strcombo"
#define GENERATOR_CLASS_PATH	"Controls/String Combobox"

#define EVT_VALUE 0
#define EVT_OUTPUT 0

typedef struct Data {
    GList    *list;
} Data;

typedef struct ControlData {
} ControlData;

PRIVATE int init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));

  data->list = NULL;

  g->data = data;
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
  
    g_list_free( data->list );
    free(data);
}


PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = safe_malloc(sizeof(Data));

  ObjectStoreDatum *array = objectstore_item_get(item, "strings");
  data->list = NULL;

  if (array != NULL) {
      int i;
      int len = objectstore_datum_array_length(array);
      for (i = 0; i < len; i++) {
	  ObjectStoreDatum *datum = objectstore_datum_array_get(array, i);

	  data->list = g_list_append(data->list,
		  (gpointer) objectstore_datum_string_value(datum));
      }
  }

  g->data = data;
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {

  Data *data = g->data;
  int len = g_list_length( data->list );
  int i;
  GList *curr;
  
  ObjectStoreDatum *array = objectstore_datum_new_array(len);

  objectstore_item_set(item, "strings", array);

  for( i=0, curr=data->list; i<len; i++, curr=g_list_next(curr) )
      objectstore_datum_array_set(array, i, objectstore_datum_new_string((char *) curr->data));
}


PRIVATE void entry_activated( GtkEntry *entry, Control *control ) {

    Generator *g = control->g;
    AEvent event;
    gchar *str = gtk_editable_get_chars( GTK_EDITABLE( GTK_COMBO(control->widget)->entry ), 0, -1 );


    gen_init_aevent(&event, AE_STRING, NULL, 0, NULL, 0, gen_get_sampletime() );
    event.d.string = safe_string_dup( str );
    g_free( str );
	    
    gen_send_events(g, EVT_OUTPUT, -1, &event);
}



PRIVATE void init_combo( Control *control ) {

    GtkCombo *cb;

    cb = GTK_COMBO( gtk_combo_new() );
    g_assert( cb != NULL );

    gtk_combo_disable_activate( cb );
    if( ((Data *)control->g->data)->list != NULL )
	gtk_combo_set_popdown_strings( cb, ((Data *)control->g->data)->list );

    gtk_signal_connect( GTK_OBJECT( cb->entry ), "activate", GTK_SIGNAL_FUNC(entry_activated), control );

    control->widget = GTK_WIDGET(cb);
}

PRIVATE void done_combo(Control *control) {
}

PRIVATE void refresh_combo(Control *control) {
    Data *data = control->g->data;

    if( ((Data *)control->g->data)->list != NULL )
	gtk_combo_set_popdown_strings( GTK_COMBO(control->widget), data->list );

}


PRIVATE void evt_value_handler(Generator *g, AEvent *event) {

  Data *data = g->data;

  if( event->kind == AE_STRING ) {

      gchar *str = safe_string_dup( event->d.string );
      data->list = g_list_append( data->list, str );
  }
      
  gen_update_controls(g, -1);
}

PRIVATE ControlDescriptor controls[] = {
  { CONTROL_KIND_USERDEF, "combo", 0,0,0,0, 0,FALSE, 0,0, init_combo, done_combo, refresh_combo },
  { CONTROL_KIND_NONE, }
};



PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE, 1, 1,
					     NULL, NULL, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_VALUE, "Value", evt_value_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_strcombo(void) {
  setup_class();
}
