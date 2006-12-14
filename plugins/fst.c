/* analyseplugin.c

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty. */

/* now its a galan plugin... and its even a vst adaptor now. */
/* copyright torben hohn 
 * release under GPL.
 */

/*****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <signal.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkevents.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gmodule.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"


/* On Win32, these headers seem to need to follow glib.h */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define GENERATOR_CLASS_NAME	"vsttest1"
#define GENERATOR_CLASS_PATH	"Misc/VST"
#define GENERATOR_CLASS_PIXMAP	"template.xpm"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_INPUT		0
#define NUM_EVENT_INPUTS	1

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1

#include <fst.h>
#include <vst/aeffectx.h>
typedef AEffect *(*AEMainFunc)( audioMasterCallback cb );


gboolean adopted_rt_thread = FALSE;


typedef struct Data {

  FST *aeff;
  float  *inevents;
  float  **insignals;
  float  **outsignals;
  SAMPLETIME lastrun;
  int paramoffset;
  void *chunk;
  
} Data;

typedef struct EditorData {
	int mapped;
} EditorData;

typedef struct PluginInfo {
    FSTHandle *handle;
    char *dllname;
} PluginInfo;

// Globals... 

PRIVATE GHashTable *MainFuncIndex = NULL;

PRIVATE void handle_automation( AEffect *plugin, long index, float value ) {
    Generator *g;
    Data *data;

    g_return_if_fail( plugin );
    
    g = plugin->user;
    g_return_if_fail( g );

    data = g->data;
    g_return_if_fail( data );

    //printf( "pre inevents setting\n" );
    data->inevents[index] = value;
    //printf( "survived\n" );
    gen_update_controls( g, -1 );
}


PRIVATE long master_callback( void *fx, long opcode, long index, long value, void *ptr, float opt ) {

    //printf( "master callback: op=%ld, index = %ld, value = %ld opt=%f\n", opcode, index, value, opt );
    switch( opcode ) {
	case audioMasterAutomate: 
	    handle_automation( fx, index, opt );
	    return 1;
	case audioMasterVersion: return 2;
	case audioMasterGetBlockSize: return MAXIMUM_REALTIME_STEP;
	case audioMasterGetSampleRate: return SAMPLE_RATE;
	case audioMasterWantMidi: return 1;
	default: return 0;
    }
    return 1;
}

PRIVATE gboolean configure_handler (GtkWidget* widget, GdkEventConfigure* ev, GtkSocket *socket) {
	XEvent event;
	gint x, y;

	//printf( "configure event\n" );
	g_return_val_if_fail (socket->plug_window != NULL, FALSE);
	event.xconfigure.type = ConfigureNotify;

	event.xconfigure.event = GDK_WINDOW_XWINDOW (socket->plug_window);
	event.xconfigure.window = GDK_WINDOW_XWINDOW (socket->plug_window);

	gdk_error_trap_push ();
	gdk_window_get_origin (socket->plug_window, &x, &y);
	gdk_error_trap_pop ();

	event.xconfigure.x = x;
	event.xconfigure.y = y;
	event.xconfigure.width = GTK_WIDGET(socket)->allocation.width;
	event.xconfigure.height = GTK_WIDGET(socket)->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
		    GDK_WINDOW_XWINDOW (socket->plug_window),
		    False, StructureNotifyMask, &event);
	gdk_display_sync (gtk_widget_get_display (GTK_WIDGET (socket)));
	gdk_error_trap_pop ();

	return FALSE;
}

PRIVATE void my_fst_signal_handler (int sig, siginfo_t* info, void* context) {
	printf ("fst signal handler %d", sig );
	exit (1);
}

PRIVATE void run_plugin( Generator *g, int buflen ) {
  Data *data = g->data;
  int i,j;

//  if( ! adopted_rt_thread ) {
//      printf( "adopting... %d\n" , fst_adopt_thread() );
//      adopted_rt_thread = TRUE;
//  }
  for( i=0; i<data->aeff->plugin->numInputs; i++ ) {

//#if SAMPLE==double
//      SAMPLE buf[MAXIMUM_REALTIME_STEP];
//
//      if( gen_read_realtime_input( g, i, -1, buf, buflen ) ) {
//
//	  for( j=0; j<buflen; j++ )
//	      data->insignals[i][j] = buf[j];
//#else
//
      if( gen_read_realtime_input( g, i, -1, data->insignals[i], buflen ) ) {

//#endif

      } else {

	  for( j=0; j<buflen; j++ )
	      data->insignals[i][j] = 0.0;
      }

  }
      
  if( data->aeff->plugin->flags & effFlagsCanReplacing ) {
      for( i=0; i<data->aeff->plugin->numOutputs; i++ ) {
	      memset( data->outsignals[i], 0, sizeof(float) * buflen );
      }
      data->aeff->plugin->processReplacing( data->aeff->plugin, data->insignals, data->outsignals, buflen );
  } else {

      for( i=0; i<data->aeff->plugin->numOutputs; i++ ) {
	      memset( data->outsignals[i], 0, sizeof(float) * buflen );
      }
      data->aeff->plugin->process( data->aeff->plugin, data->insignals, data->outsignals, buflen );
  }

  data->lastrun = gen_get_sampletime();

}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {

    switch (event->kind)
    {
	case AE_REALTIME:
	    {
		run_plugin( g, event->d.integer );
		break;
	    }

	default:
	    g_warning("vst module doesn't care for events of kind %d.", event->kind);
	    break;
    }
}


PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, inscount, outscount, i;

  PluginInfo *pluginfo;

  
  g->data = data;



  pluginfo = g_hash_table_lookup( MainFuncIndex, g->klass->tag );
  if( !pluginfo->handle )
      pluginfo->handle = fst_load( pluginfo->dllname );
  
  data->aeff = fst_instantiate( pluginfo->handle, (audioMasterCallback) master_callback, g ); 

  inecount = data->aeff->plugin->numParams;
  inscount = data->aeff->plugin->numInputs;
  outscount = data->aeff->plugin->numOutputs;

  data->inevents = safe_malloc( sizeof( float ) * inecount  );
  data->insignals = safe_malloc( sizeof( float * ) * inscount  );
  data->outsignals = safe_malloc( sizeof( float * ) * outscount  );

  for( i=0; i<inecount; i++ ) {
      data->inevents[i] = data->aeff->plugin->getParameter( data->aeff->plugin, i );
  }

  for( i=0; i<outscount; i++ ) {
      data->outsignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  for( i=0; i<inscount; i++ ) {
      data->insignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  data->aeff->plugin->dispatcher( data->aeff->plugin, effOpen, 0, 0, NULL, 0 );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effSetSampleRate, 0, 0, NULL, (float) SAMPLE_RATE );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effSetBlockSize, 0, MAXIMUM_REALTIME_STEP, NULL, 0 );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effMainsChanged, 0, 1, NULL, 0 );
  

  data->lastrun = 0;
  
  if( outscount == 0 )
      gen_register_realtime_fn(g, realtime_handler);

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
  gen_deregister_realtime_fn(g, realtime_handler);
  fst_close( data->aeff );
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, inscount, outscount, i;
  ObjectStoreDatum *inarray;
  void *chunk;
  int bytelen;


  PluginInfo *pluginfo;

  g->data = data;

  pluginfo = g_hash_table_lookup( MainFuncIndex, g->klass->tag );
  if( !pluginfo->handle )
      pluginfo->handle = fst_load( pluginfo->dllname );
  
  data->aeff = fst_instantiate( pluginfo->handle, (audioMasterCallback) master_callback, g ); 

  inecount = data->aeff->plugin->numParams;
  inscount = data->aeff->plugin->numInputs;
  outscount = data->aeff->plugin->numOutputs;
  //printf( "survived instantiate np=%d\n", inecount );

  data->inevents = safe_malloc( sizeof( float ) * inecount  );
  data->insignals = safe_malloc( sizeof( float * ) * inscount  );
  data->outsignals = safe_malloc( sizeof( float * ) * outscount  );

  inarray = objectstore_item_get(item, "param_array");

  if( inarray ) {

      for( i=0; i<inecount; i++ ) {

	  ObjectStoreDatum *field = objectstore_datum_array_get(inarray, i);
	  if( field ) {
	      data->inevents[i] = objectstore_datum_double_value( field );
	      if( data->inevents[i] >= 0 || data->inevents[i] <= 1 )
		  data->aeff->plugin->setParameter( data->aeff->plugin, i, data->inevents[i] );
	  } else {
	      data->inevents[i] = data->aeff->plugin->getParameter( data->aeff->plugin, i );
	  }
      }
  }

  for( i=0; i<outscount; i++ ) {
      data->outsignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  for( i=0; i<inscount; i++ ) {
      data->insignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  //printf( "survived parameters\n" );

  data->aeff->plugin->dispatcher( data->aeff->plugin, effOpen, 0, 0, NULL, 0 );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effSetSampleRate, 0, 0, NULL, (float) SAMPLE_RATE );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effSetBlockSize, 0, MAXIMUM_REALTIME_STEP, NULL, 0 );
  data->aeff->plugin->dispatcher( data->aeff->plugin, effMainsChanged, 0, 1, NULL, 0 );

  //printf( "running\n" );
  bytelen = objectstore_item_get_integer( item, "chunk_bytelen", 0 );

  if( bytelen != 0 ) {
      int len = objectstore_item_get_binary( item, "chunk_data", &chunk );
      if( len == bytelen ) {
	  
	  printf( "Setting chunk...\n" );

	  data->chunk = safe_malloc( len );
	  memcpy( data->chunk, chunk, len );
	  data->aeff->plugin->dispatcher( data->aeff->plugin, effSetChunk, 0, bytelen, data->chunk, 0 );
	  printf( "survived...\n" );
      } else {
	  printf( "something is wring :(\n" );
      }
  }

  data->lastrun = 0;

  if( outscount == 0 )
      gen_register_realtime_fn(g, realtime_handler);

  printf( "returning\n" );
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  int incount = data->aeff->plugin->numParams;
  int i, bytelen;
  void *chunk;

  //objectstore_item_set_integer(item, "TEMPLATE_dummy", data->dummy);
  ObjectStoreDatum *inarray = objectstore_datum_new_array( incount );
  objectstore_item_set(item, "param_array", inarray);

  for( i=0; i<incount; i++ )
      objectstore_datum_array_set(inarray, i, objectstore_datum_new_double( data->inevents[i] ) ); 

  bytelen = data->aeff->plugin->dispatcher( data->aeff->plugin, effGetChunk, 0, 0, &chunk, 0 );
  objectstore_item_set_integer(item, "chunk_bytelen", bytelen);
  if( bytelen ) {
      objectstore_item_set(item, "chunk_data",
	      objectstore_datum_new_binary(bytelen, chunk));
  }
}

#define outgen( n ) PRIVATE gboolean output_generator##n(Generator *g, SAMPLE *buf, int buflen) { \
  Data *data = g->data; \
  int i; \
\
  if( data->lastrun != gen_get_sampletime() )\
      run_plugin( g, buflen );\
\
  for( i=0; i<buflen; i++ )\
      buf[i] = data->outsignals[n][i];\
\
  return TRUE;\
}

outgen(0);
outgen(1);
outgen(2);
outgen(3);
outgen(4);
outgen(5);
outgen(6);
outgen(7);
outgen(8);
outgen(9);
outgen(10);
outgen(11);
outgen(12);


PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  data->inevents[event->dst_q] = event->d.number;

  data->aeff->plugin->setParameter( data->aeff->plugin, event->dst_q, event->d.number );
  
  gen_update_controls( g, -1 );
}

PRIVATE void evt_midi_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  struct VstMidiEvent pevent;
  struct VstEvents events;
  int i;

  if( event->kind != AE_MIDIEVENT )
      return;


	//printf("len: %d cmd: 0x%x note: %d vel: %d\n", event->d.midiev.len, event->d.midiev.midistring[0], event->d.midiev.midistring[1], event->d.midiev.midistring[2] );
	
  pevent.type = kVstMidiType;
  pevent.byteSize = 24;
  pevent.deltaFrames = 0;
  pevent.flags = 0;
  pevent.detune = 0;
  pevent.noteLength = 0;
  pevent.noteOffset = 0;
  pevent.reserved1 = 0;
  pevent.reserved2 = 0;
  pevent.noteOffVelocity = 0;

  for( i=0; i<event->d.midiev.len; i++ )
      pevent.midiData[i] = event->d.midiev.midistring[i];

  for( ; i<4; i++ )
      pevent.midiData[i] = 0;
      
  events.numEvents = 1;
  events.reserved = 0;
  events.events[0] = &pevent;
  
  data->aeff->plugin->dispatcher (data->aeff->plugin, effProcessEvents, 0, 0, &events, 0.0f);
  
  //gen_update_controls( g, -1 );
}


/*****************************************************************************/

PRIVATE void editor_move_handler( Control *c ) {
    configure_handler( NULL, NULL, c->widget );
}

PRIVATE gboolean sock_map( GtkWidget *widget, Control *c ) {
    Data *data = c->g->data;
    EditorData *wdata = c->data;

    if( !wdata->mapped ) {
	wdata->mapped = TRUE;

	if( fst_run_editor( data->aeff ) ) {
	    printf( "Failed to create Editor\n" );
	}

	//printf( "sock map XID = %d\n", fst_get_XID( data->aeff ) );
	gtk_socket_add_id( GTK_SOCKET(c->widget), fst_get_XID( data->aeff ) );
	gtk_widget_show( c->widget );
	gtk_widget_set_usize( c->widget, data->aeff->width, data->aeff->height );
	gtk_signal_connect( GTK_OBJECT(control_panel), "configure_event", (GtkSignalFunc)configure_handler, c->widget );
	c->move_callback = editor_move_handler;
    }
    else
    {
	gtk_widget_unmap( widget );
	gtk_widget_map( widget );
//	sock_unmap( widget, c );
//	sock_map( widget, c );
    }

    return TRUE;
}

PRIVATE gboolean sock_unmap( GtkWidget *widget, Control *c ) {
    Data *data = c->g->data;
    EditorData *wdata = c->data;

    if( wdata->mapped ) {
	wdata->mapped = FALSE;
	//printf( "sock unmap XID = %d\n", fst_get_XID( data->aeff ) );
	gtk_signal_disconnect_by_data( GTK_OBJECT( control_panel ), c->widget );
	c->move_callback = NULL;
	fst_destroy_editor( data->aeff );
    }

    return TRUE;
}


PRIVATE void init_editor( Control *control ) {


  EditorData *wdata = safe_malloc( sizeof( EditorData ) );
  control->data = wdata;

  control->widget = gtk_socket_new();

  
  gtk_signal_connect( GTK_OBJECT(control->widget), "map", (GtkSignalFunc)sock_map, control );
  gtk_signal_connect( GTK_OBJECT(control->widget), "unmap", (GtkSignalFunc)sock_unmap, control );
}

PRIVATE void done_editor(Control *c) {
    Data *data = c->g->data;
    EditorData *wdata = c->data;


  if( wdata->mapped ) {
      wdata->mapped = FALSE;
      c->move_callback = NULL;
      gtk_signal_disconnect_by_data( GTK_OBJECT( control_panel ), c->widget );
      fst_destroy_editor( data->aeff );
  }
}


/*****************************************************************************/

PRIVATE AGenerator_t output_generators[13] = { 
    output_generator0,
    output_generator1,
    output_generator2,
    output_generator3,
    output_generator4,
    output_generator5,
    output_generator6,
    output_generator7,
    output_generator8,
    output_generator9,
    output_generator10,
    output_generator11,
    output_generator12
};

PRIVATE int plugin_count=0;

PUBLIC void control_VST_Data_updater(Control *c) {
    Data *data=c->g->data;

    control_set_value(c, (data->inevents[(int) c->desc->refresh_data]));
}
				     
PRIVATE void setup_one_class( char *dllname ) {

    unsigned long i,j;
    FSTInfo *fstinfo;
    InputSignalDescriptor *inputdescr;
    OutputSignalDescriptor *outputdescr;
    ControlDescriptor *controls;
    char *generatorpath;

    GeneratorClass *k;
    PluginInfo *pluginfo;
    
    char *basename = g_path_get_basename( dllname );
    basename[strlen(basename)-4] = 0;

    fstinfo = fst_get_info( dllname );
    if( !fstinfo ) return;



    // Add 1 for NULL termination
    inputdescr = safe_malloc( sizeof( InputSignalDescriptor ) * (fstinfo->numInputs + 1) );
    outputdescr= safe_malloc( sizeof( OutputSignalDescriptor ) * (fstinfo->numOutputs + 1) );

    // Add 1 for NULL termination and add 1 for Editor
    controls = safe_malloc( sizeof( ControlDescriptor ) * (fstinfo->numParams + 2) );


	
    for( i=0; i<fstinfo->numInputs; i++ )
    {
	inputdescr[i].name  = g_strdup_printf( "input_%d", (int)i );
	inputdescr[i].flags = SIG_FLAG_REALTIME;
    }
    inputdescr[i].name = NULL;
   
    for( i=0; i<fstinfo->numOutputs; i++ )
    {
	outputdescr[i].name  = g_strdup_printf( "output_%d", (int)i );
	outputdescr[i].flags = SIG_FLAG_REALTIME;
	outputdescr[i].d.realtime = output_generators[i];
    }
    outputdescr[i].name = NULL;

    i=0;
    if( fstinfo->hasEditor) {

	controls[i].kind = CONTROL_KIND_USERDEF;
	controls[i].name = "Editor";


	controls[i].min = 0.0;
	
	controls[i].max = 1.0;

	controls[i].step = 0.01;
	controls[i].page = 0.01;

	controls[i].size = 0;
	controls[i].allow_direct_edit = TRUE;
	controls[i].is_dst_gen = TRUE;
	controls[i].queue_number = i;


	controls[i].initialize = init_editor;
	controls[i].destroy = done_editor;
	controls[i].refresh = NULL;
	controls[i].refresh_data = (void *)i;
	i++;
    }
    for( j=0 ; j<fstinfo->numParams; i++, j++ )
    {
	controls[i].kind = CONTROL_KIND_KNOB;

	controls[i].name = safe_string_dup( fstinfo->ParamNames[j] );
	controls[i].min = 0.0;
	
	controls[i].max = 1.0;

	controls[i].step = 0.01;
	controls[i].page = 0.01;

	controls[i].size = 0;
	controls[i].allow_direct_edit = TRUE;
	controls[i].is_dst_gen = TRUE;
	controls[i].queue_number = j;


	controls[i].initialize = NULL;
	controls[i].destroy = NULL;
	controls[i].refresh = control_VST_Data_updater;
	controls[i].refresh_data = (void *)j;
    }
    
    controls[i].kind = CONTROL_KIND_NONE;
   
    k = gen_new_generatorclass_with_different_tag( basename, 
				g_strdup_printf( "vst-%d", (int) fstinfo->UniqueID ), FALSE,
				fstinfo->numParams + (fstinfo->wantMidi ? 1:0), 0,
				inputdescr, outputdescr, controls,
				init_instance, destroy_instance,
				unpickle_instance, pickle_instance);

    for( i=0; i<fstinfo->numParams; i++ ) {

	gen_configure_event_input(k, i, fstinfo->ParamNames[i], evt_input_handler);
    }
    if( fstinfo->wantMidi ) {
	gen_configure_event_input(k, i, "MidiPort", evt_midi_handler);
    }


    pluginfo = safe_malloc( sizeof( PluginInfo ) );
    pluginfo->handle = NULL;
    pluginfo->dllname = safe_string_dup( dllname );
    
    g_hash_table_insert(MainFuncIndex, k->tag, pluginfo);

	
    generatorpath = g_strdup_printf( "VST%2d/%s", (plugin_count++) / 20, basename );



    gencomp_register_generatorclass(k, FALSE, generatorpath,
	    PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
	    NULL);
	
    free( generatorpath );
    
    fst_free_info( fstinfo );
}


PRIVATE void setup_all( void ) {

    char *vst_path = getenv( "GALAN_VST_DIR" );

    if( vst_path ) {
	GDir *dir = g_dir_open( vst_path, 0, NULL );
	const char *filename;

	while( (filename = g_dir_read_name( dir )) ) {
	    char *fullname;
	    if( strcmp(filename+(strlen(filename)-4), ".dll" ) )
		continue;

	    fullname = g_strdup_printf( "%s/%s", vst_path, filename );
	    
	    setup_one_class( fullname );
	    g_free( fullname );
	}


	g_dir_close( dir );
    } else {
	g_print( "GALAN_VST_DIR is no set !!!\n" );
    }
}

PRIVATE gboolean setup_globals( void ) {

    //if (fst_init (NULL))
    if (fst_init (my_fst_signal_handler))
	return TRUE;
    
    MainFuncIndex = g_hash_table_new(g_str_hash, g_str_equal);
    return FALSE;
}


PUBLIC void init_plugin_fst(void) {
  if(  setup_globals() ) 
      return;

  setup_all();
}

/*****************************************************************************/

/* EOF */
