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

#define GENERATOR_CLASS_NAME	"midiclock"
#define GENERATOR_CLASS_PATH	"Misc/MIDI Clock"



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
    NUM_EVENT_OUTPUTS
};

typedef struct Data {
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

  unsigned char midibyte;
  AEvent event;

  read( source, &midibyte, 1 );

  if( midibyte == 0xf8 ) {

      gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
      event.d.number = 1;
      gen_send_events(g, EVT_CLOCK, -1, &event);
  }
  if( midibyte == 0xfa ) {

      gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );
      event.d.number = 1;
      gen_send_events(g, EVT_START, -1, &event);
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

  data->fd = open( "/dev/midi00", O_RDONLY | O_NONBLOCK );
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

  data->fd = open( "/dev/midi00", O_RDONLY );
  data->input_tag = gdk_input_add( data->fd, GDK_INPUT_READ, (GdkInputFunction) input_callback, g ); 
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  //Data *data = g->data;
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
  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH, NULL, NULL);
}

PUBLIC void init_plugin_midiclock(void) {
  setup_class();
}


