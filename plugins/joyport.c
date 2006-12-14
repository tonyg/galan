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

#include <linux/joystick.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define GENERATOR_CLASS_NAME	"joyport"
#define GENERATOR_CLASS_PATH	"Controls/Joyport"



/* 
 * Here is the Data....
 *
 */

enum EVT_INPUTS { 
    NUM_EVENT_INPUTS = 0
};

enum EVT_OUTPUTS { 
    EVT_AXIS0 = 0,
    EVT_AXIS1,
    EVT_AXIS2,
    EVT_AXIS3,
    EVT_AXIS4,
    EVT_AXIS5,
    EVT_BTNDOWN,
    EVT_BTNUP,
    NUM_EVENT_OUTPUTS
};

typedef struct Data {
  char *dev_name;
  gint fd;  
  gint input_tag;
} Data;


/*
 * This is the input callback....
 * Seems to bo ok for now..
 *
 * now with the buttons its ok...
 * (??? Is sending the number ok with evtcomp)
 * 
 */


PRIVATE void input_callback( Generator *g, gint source, GdkInputCondition condition ) {

  struct js_event jsevent;
  AEvent event;

  read( source, &jsevent, sizeof( struct js_event ) );

  jsevent.type &= 0x7f;

  switch( jsevent.type ) {
      
      case JS_EVENT_AXIS:

	  if( jsevent.number < 6 ) {
	      gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
	      event.d.number = ((gdouble) jsevent.value) / 32767;
	      gen_send_events(g, jsevent.number, -1, &event);
	  }
	  
	  break;
	  

      case JS_EVENT_BUTTON:
	  
	  gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
	  event.d.number = ((gdouble) jsevent.number);
	  if( jsevent.value == 1 )
	      gen_send_events(g, EVT_BTNDOWN, -1, &event);
	  else
	      gen_send_events(g, EVT_BTNUP, -1, &event);

	  break;
    
      default:
	  //printf( "unknown event: type %d, num %d, val %d \n", jsevent.type, jsevent.number, jsevent.value  );
	  break;
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
  data->dev_name = safe_string_dup("/dev/input/js0");

  data->fd = open( data->dev_name, O_RDONLY );
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction)  input_callback, (gpointer) g ); 
  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gdk_input_remove( data->input_tag );
  close( data->fd );

  if( data->dev_name )
      free( data->dev_name );

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
  data->dev_name = safe_string_dup( objectstore_item_get_string(item, "dev_name", "/dev/input/js0" ) );

  data->fd = open( data->dev_name, O_RDONLY );
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction) input_callback, g ); 
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  if (data->dev_name != NULL)
    objectstore_item_set_string(item, "dev_name", data->dev_name);
}

/*
 * controls....
 *
 * Shall there ever be controls ???
 * i am a control :-)
 *
 */

PRIVATE ControlDescriptor controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
     { CONTROL_KIND_NONE, }
};

// Properties...

PRIVATE void propgen(Component *c, Generator *g) {
  Data *data = g->data;

  GtkWidget *hb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Joystick Device:");
  GtkWidget *text = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_entry_set_text(GTK_ENTRY(text), data->dev_name );

  popup_dialog("Properties", MSGBOX_DISMISS, 0, MSGBOX_DISMISS, hb, NULL, 0);

  if( data->dev_name )
      free( data->dev_name );

  data->dev_name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(text)));

  gdk_input_remove( data->input_tag );
  close( data->fd );

  data->fd = open( data->dev_name, O_RDONLY );
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction)  input_callback, (gpointer) g ); 
}

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

  gen_configure_event_output(k, EVT_AXIS0,   "Axxis0");
  gen_configure_event_output(k, EVT_AXIS1,   "Axxis1");
  gen_configure_event_output(k, EVT_AXIS2,   "Axxis2");
  gen_configure_event_output(k, EVT_AXIS3,   "Axxis3");
  gen_configure_event_output(k, EVT_AXIS4,   "Axxis4");
  gen_configure_event_output(k, EVT_AXIS5,   "Axxis5");
  gen_configure_event_output(k, EVT_BTNDOWN, "ButtonDown");
  gen_configure_event_output(k, EVT_BTNUP,   "ButtonUp");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, propgen);
}

PUBLIC void init_plugin_joyport(void) {
  setup_class();
}


