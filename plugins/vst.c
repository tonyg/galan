/* analyseplugin.c

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty. */

/*****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************/

#include "ladspa.h"

#include "ladspa-utils.h"

/*****************************************************************************/

/* includes for galan : */

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

#include "AEffect.h"

typedef AEffect *(*AEMainFunc)( audioMasterCallback cb );

//typedef struct VPluginData {
//    GList *insig, *outsig, *inevent, *outevent;
//} VPluginData;


typedef struct Data {

  AEMainFunc main_func;
  struct AEffect *aeff;
  //LADSPA_Handle instance_handle;
  //VPluginData *vpdat;
  float  *inevents;
  float  *outevents;
  float  *oldoutevents;
  float  **insignals;
  float  **outsignals;
  SAMPLETIME lastrun;
  
} Data;

// Globals... 

PRIVATE GHashTable *MainFuncIndex = NULL;
PRIVATE GHashTable *VPluginIndex = NULL;

long master_callback( AEffect *fx, long opcode, long index, long value, void *ptr, float opt ) {
	return 2;
}


PRIVATE void run_plugin( Generator *g, int buflen ) {
  Data *data = g->data;
  int i,j;

  for( i=0; i<data->aeff->numInputs; i++ ) {

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
      
  data->aeff->processReplacing( data->aeff, data->insignals, data->outsignals, buflen );
  data->lastrun = gen_get_sampletime();

#if 0
  for( i=0; i<outecount; i++ ) {

      //printf( "oev: %f", data->outevents[i] );
      if( data->oldoutevents[i] != data->outevents[i] ) {

	  // send event...

	  AEvent event;
	  
	  gen_init_aevent(&event, AE_NUMBER, NULL, 0, NULL, 0, gen_get_sampletime() );

#ifdef G_OS_WIN32
	      event.d.number = data->outevents[i];
#else
	  if( isnan(data->outevents[i]) || isinf( data->outevents[i]) ) {
	      printf( "nan ... \n" );
	      event.d.number = 0.0;
	  } else {
	      event.d.number = data->outevents[i];
	  }
#endif //G_OS_WIN32
	  
	  gen_send_events(g, i, -1, &event);

	  //printf( "sending ev: %f on %d \n", event.d.number, i );

	  // copy new -> old

	  data->oldoutevents[i] = data->outevents[i];
      }
  }
#endif
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {

    //Data *data = g->data;

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

#if 0
PRIVATE LADSPA_Data get_default( ControlDescriptor *control, LADSPA_PortRangeHintDescriptor hint ) {

    if( LADSPA_IS_HINT_HAS_DEFAULT( hint ) ) {

	if( LADSPA_IS_HINT_DEFAULT_MINIMUM( hint ) )
	    return control->min;
	if( LADSPA_IS_HINT_DEFAULT_LOW( hint ) )
	    return control->min * 0.75 + control->max * 0.25;
	if( LADSPA_IS_HINT_DEFAULT_MIDDLE( hint ) )
	    return control->min * 0.5 + control->max * 0.5;
	if( LADSPA_IS_HINT_DEFAULT_HIGH( hint ) )
	    return control->min * 0.25 + control->max * 0.75;
	if( LADSPA_IS_HINT_DEFAULT_MAXIMUM( hint ) )
	    return control->max;
	if( LADSPA_IS_HINT_DEFAULT_0( hint ) )
	    return 0.0;
	if( LADSPA_IS_HINT_DEFAULT_1( hint ) )
	    return 1.0;
	if( LADSPA_IS_HINT_DEFAULT_100( hint ) )
	    return 100.0;
	if( LADSPA_IS_HINT_DEFAULT_440( hint ) )
	    return 440.0;

	printf( "plugins says it has default... but i cant find out.\n" );
	return 0.0;

    } else 
	return 0.0;

}
#endif

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, inscount, outscount, i;


  

  
  g->data = data;



  data->main_func = g_hash_table_lookup( MainFuncIndex, g->klass->tag );
  //printf( "retrieved: %s %x\n", g->klass->name, data->ladspa_descriptor );
  data->aeff = data->main_func( &master_callback ); 
  //printf( "got instancehandle: %x\n", data->instance_handle );

  // Connect the ports ... pre: alloc memory for the controls...
  // connect wird erst vor dem laufen mit outpu_generator gemacht...

  inecount = data->aeff->numParams;
  inscount = data->aeff->numInputs;
  outscount = data->aeff->numOutputs;

  data->inevents = safe_malloc( sizeof( float ) * inecount  );
  //data->outevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  //data->oldoutevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  data->insignals = safe_malloc( sizeof( float * ) * inscount  );
  data->outsignals = safe_malloc( sizeof( float * ) * outscount  );

  for( i=0; i<inecount; i++ ) {
      data->inevents[i] = data->aeff->getParameter( data->aeff, i );
  }
  
  
//  for( i=0; i<outecount; i++ ) {
//      data->outevents[i] = 0.0;
//      data->oldoutevents[i] = 0.0;
//
//      data->ladspa_descriptor->connect_port(
//	      data->instance_handle,
//	      (int) g_list_nth_data( data->lpdat->outevent, i ),
//	      &(data->outevents[i]) );

//  }  

  for( i=0; i<outscount; i++ ) {
      data->outsignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  for( i=0; i<inscount; i++ ) {
      data->insignals[i] = safe_malloc( sizeof( float ) * MAXIMUM_REALTIME_STEP );
  }  

  data->aeff->dispatcher( data->aeff, effOpen, 0, 0, NULL, 0 );
  data->aeff->dispatcher( data->aeff, effSetSampleRate, 0, 0, NULL, (float) SAMPLE_RATE );
  data->aeff->dispatcher( data->aeff, effSetBlockSize, 0, MAXIMUM_REALTIME_STEP, NULL, 0 );
  data->aeff->dispatcher( data->aeff, effMainsChanged, 0, 1, NULL, 0 );

  data->lastrun = 0;
  
  if( outscount == 0 )
      gen_register_realtime_fn(g, realtime_handler);

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
    Data *data = g->data;
  gen_deregister_realtime_fn(g, realtime_handler);
  data->aeff->dispatcher( data->aeff, effClose, 0, 0, 0, 0 );
  // TODO: what a mess free it all ....
  free(g->data);
}

#if 0
PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, outecount, inscount, outscount, i;
  ObjectStoreDatum *inarray, *outarray;
  g->data = data;

  data->ladspa_descriptor = g_hash_table_lookup( DescriptorIndex, g->klass->tag );
  data->lpdat = g_hash_table_lookup( LPluginIndex, g->klass->tag );
  data->instance_handle = data->ladspa_descriptor->instantiate( data->ladspa_descriptor, SAMPLE_RATE ); 

  

  inarray = objectstore_item_get(item, "ladspa_inarray");
  outarray  = objectstore_item_get(item, "ladspa_oldoutarray");

  inecount = g_list_length( data->lpdat->inevent );
  outecount = g_list_length( data->lpdat->outevent );
  inscount = g_list_length( data->lpdat->insig );
  outscount = g_list_length( data->lpdat->outsig );

  data->inevents = safe_malloc( sizeof( LADSPA_Data ) * inecount  );
  data->outevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  data->oldoutevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  data->insignals = safe_malloc( sizeof( LADSPA_Data * ) * inscount  );
  data->outsignals = safe_malloc( sizeof( LADSPA_Data * ) * outscount  );

  for( i=0; i<inecount; i++ ) {
      
      ObjectStoreDatum *field = objectstore_datum_array_get(inarray, i);
      if( field )
	  data->inevents[i] = objectstore_datum_double_value( field );
      else
      {
	  unsigned long portindex = (unsigned long) g_list_nth_data( data->lpdat->inevent, i );
	  LADSPA_PortRangeHintDescriptor hint = data->ladspa_descriptor->PortRangeHints[portindex].HintDescriptor;
	  ControlDescriptor *control = &(g->klass->controls[i]);

	  data->inevents[i] = get_default( control, hint );
      }

      data->ladspa_descriptor->connect_port( 
	      data->instance_handle, 
	      (int) g_list_nth_data( data->lpdat->inevent, i ),
	      &(data->inevents[i]) );
  }
  for( i=0; i<outecount; i++ ) {
      ObjectStoreDatum *field = objectstore_datum_array_get(outarray, i);
      if( field )
	  data->oldoutevents[i] = objectstore_datum_double_value( field );
      else
	  data->oldoutevents[i] = 0;

      data->ladspa_descriptor->connect_port(
	      data->instance_handle,
	      (int) g_list_nth_data( data->lpdat->outevent, i ),
	      &(data->outevents[i]) );
  }
  for( i=0; i<outscount; i++ ) {
      data->outsignals[i] = safe_malloc( sizeof( LADSPA_Data ) * MAXIMUM_REALTIME_STEP );

      data->ladspa_descriptor->connect_port(
	      data->instance_handle,
	      (int) g_list_nth_data( data->lpdat->outsig, i ),
	      data->outsignals[i] );
  }  
  for( i=0; i<inscount; i++ ) {
      data->insignals[i] = safe_malloc( sizeof( LADSPA_Data ) * MAXIMUM_REALTIME_STEP );

      data->ladspa_descriptor->connect_port(
	      data->instance_handle,
	      (int) g_list_nth_data( data->lpdat->insig, i ),
	      data->insignals[i] );
  }  

  if( data->ladspa_descriptor->activate )
      data->ladspa_descriptor->activate( data->instance_handle ); 

  data->lastrun = 0;

  if( outscount == 0 )
      gen_register_realtime_fn(g, realtime_handler);
}

PRIVATE void pickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = g->data;
  int incount = g_list_length( data->lpdat->inevent );
  int outcount = g_list_length( data->lpdat->outevent );
  int i;

  //objectstore_item_set_integer(item, "TEMPLATE_dummy", data->dummy);
  ObjectStoreDatum *inarray = objectstore_datum_new_array( incount );
  ObjectStoreDatum *outarray = objectstore_datum_new_array( outcount );
  objectstore_item_set(item, "ladspa_inarray", inarray);
  objectstore_item_set(item, "ladspa_oldoutarray", outarray);

  for( i=0; i<incount; i++ )
      objectstore_datum_array_set(inarray, i, objectstore_datum_new_double( data->inevents[i] ) ); 
  for( i=0; i<outcount; i++ )
      objectstore_datum_array_set(outarray, i, objectstore_datum_new_double( data->oldoutevents[i] ) ); 

}
#endif


PRIVATE gboolean output_generator0(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[0][i];

  return TRUE;
}
PRIVATE gboolean output_generator1(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[1][i];

  return TRUE;
}
PRIVATE gboolean output_generator2(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[2][i];

  return TRUE;
}
PRIVATE gboolean output_generator3(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[3][i];

  return TRUE;
}
PRIVATE gboolean output_generator4(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[4][i];

  return TRUE;
}
PRIVATE gboolean output_generator5(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[5][i];

  return TRUE;
}
PRIVATE gboolean output_generator6(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[6][i];

  return TRUE;
}
PRIVATE gboolean output_generator7(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( data->lastrun != gen_get_sampletime() )
      run_plugin( g, buflen );

  for( i=0; i<buflen; i++ )
      buf[i] = data->outsignals[7][i];

  return TRUE;
}

PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
  /* handle incoming events on queue EVT_INPUT */
  Data *data = g->data;
  data->inevents[event->dst_q] = event->d.number;

  data->aeff->setParameter( data->aeff, event->dst_q, event->d.number );
  
  gen_update_controls( g, event->dst_q );
}





/*****************************************************************************/

PRIVATE AGenerator_t output_generators[] = { output_generator0, output_generator1, output_generator2, output_generator3,
				     output_generator4, output_generator5, output_generator6, output_generator7 };
PRIVATE int plugin_count=0;

PUBLIC void control_VST_Data_updater(Control *c) {
    Data *data=c->g->data;

    control_set_value(c, (data->inevents[(int) c->desc->refresh_data]));
}
				     
PRIVATE void setup_one_class( char *dllname ) {

    unsigned long i;
    //LPluginData *plugindata = safe_malloc( sizeof( LPluginData ) );
    void *dll_handle;
    AEMainFunc main_func;
    AEffect *tmp_effect;
    InputSignalDescriptor *inputdescr;
    OutputSignalDescriptor *outputdescr;
    ControlDescriptor *controls;
    char *generatorpath;

    GeneratorClass *k;
    
    char *basename = g_path_get_basename( dllname );
    basename[strlen(basename)-4] = 0;

    dll_handle = WineLoadLibrary( dllname );
    main_func = WineGetProcAddress( dll_handle, "main" );
    
    
    tmp_effect = main_func( &master_callback );
    
    


//    for( i=0; i<psDescriptor->PortCount; i++ ) {
//	
//	LADSPA_PortDescriptor PortDescriptor=psDescriptor->PortDescriptors[i];
//
//	if( LADSPA_IS_PORT_INPUT( PortDescriptor ) ) 
//
//	    if( LADSPA_IS_PORT_CONTROL(PortDescriptor) )
//		plugindata->inevent = g_list_append( plugindata->inevent, (gpointer) i );
//	    else
//		plugindata->insig = g_list_append( plugindata->insig, (gpointer) i );
//
//	else
//
//	    if( LADSPA_IS_PORT_CONTROL( PortDescriptor ) ) 
//		plugindata->outevent = g_list_append( plugindata->outevent, (gpointer) i );
//	    else
//		plugindata->outsig = g_list_append( plugindata->outsig, (gpointer) i );
//    } 

    // Add 1 for NULL termination
    inputdescr = safe_malloc( sizeof( InputSignalDescriptor ) * (tmp_effect->numInputs + 1) );
    outputdescr= safe_malloc( sizeof( OutputSignalDescriptor ) * (tmp_effect->numOutputs + 1) );
    controls = safe_malloc( sizeof( ControlDescriptor ) * (tmp_effect->numParams + 1) );


	
    //printf( "Input Signals:\n" );
    for( i=0; i<tmp_effect->numInputs; i++ )
    {
	inputdescr[i].name  = g_strdup_printf( "input_%d", (int)i );
	inputdescr[i].flags = SIG_FLAG_REALTIME;
    }
    inputdescr[i].name = NULL;
   
    //printf( "Output Signals:\n" );
    for( i=0; i<tmp_effect->numOutputs; i++ )
    {
	outputdescr[i].name  = g_strdup_printf( "output_%d", (int)i );
	outputdescr[i].flags = SIG_FLAG_REALTIME;
	outputdescr[i].d.realtime = output_generators[i];
    }
    outputdescr[i].name = NULL;

    for( i=0; i<tmp_effect->numParams; i++ )
    {
	controls[i].kind = CONTROL_KIND_KNOB;

	controls[i].name = safe_malloc( 30 );

	tmp_effect->dispatcher( tmp_effect, effGetParamName, i, 0, controls[i].name, 0 );

	controls[i].min = 0.0;
	
	controls[i].max = 1.0;

	controls[i].step = 0.01;
	controls[i].page = 0.01;

	controls[i].size = 0;
	controls[i].allow_direct_edit = TRUE;
	controls[i].is_dst_gen = TRUE;
	controls[i].queue_number = i;


	controls[i].initialize = NULL;
	controls[i].destroy = NULL;
	controls[i].refresh = control_VST_Data_updater;
	controls[i].refresh_data = (void *)i;
    }
    
    controls[i].kind = CONTROL_KIND_NONE;
   
    //printf( "Menu : %s\n", get_lrdf_menuname( psDescriptor->UniqueID ) );
    k = gen_new_generatorclass_with_different_tag( basename, 
				g_strdup_printf( "vst-%d", (int) tmp_effect->uniqueID ), FALSE,
				tmp_effect->numParams, 0,
				inputdescr, outputdescr, controls,
				init_instance, destroy_instance,
				init_instance, NULL);

    for( i=0; i<tmp_effect->numParams; i++ ) {

	gen_configure_event_input(k, i, controls[i].name, evt_input_handler);
    }

    //printf( "Output Events:\n" );
   
    
    //printf( "inserting: %s %x\n", psDescriptor->Label, psDescriptor );
    //XXX: Is it necessary to add both tags ? Should be handled by the generator resolution.
    
    g_hash_table_insert(MainFuncIndex, k->tag, main_func);

	
    generatorpath = g_strdup_printf( "VST%2d/%s", (plugin_count++) / 20, basename );



    gencomp_register_generatorclass(k, FALSE, generatorpath,
	    PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
	    NULL);
	
    free( generatorpath );
    
    //printf("Plugin Name: \"%s\"\n", psDescriptor->Name);
    //printf("Plugin Label: \"%s\"\n", psDescriptor->Label);
    //printf("Plugin Unique ID: %lu\n", psDescriptor->UniqueID);
    //printf("Maker: \"%s\"\n", psDescriptor->Maker);
    //printf("Copyright: \"%s\"\n", psDescriptor->Copyright);
}





PRIVATE void setup_all( void ) {

    //setup_one_class( "Z:/home/torbenh/xxx/vst/cesSynth1.dll" );
    
    char *vst_path = getenv( "GALAN_VST_DIR" );

    if( vst_path ) {
	GDir *dir = g_dir_open( vst_path, 0, NULL );
	const char *filename;

	while( filename = g_dir_read_name( dir ) ) {
	    char *winname;
	    if( strcmp(filename+(strlen(filename)-4), ".dll" ) )
		continue;

	    winname = g_strdup_printf( "Z:%s/%s", vst_path,filename );
	    
	    printf( "loading plugin %s\n", winname );
	    setup_one_class( winname );
	    g_free( winname );
	}


	g_dir_close( dir );
    } else {
	g_print( "GALAN_VST_DIR is no set !!!\n" );
    }
    
}

PRIVATE void setup_globals( void ) {

    SharedWineInit();
  MainFuncIndex = g_hash_table_new(g_str_hash, g_str_equal);
}


PUBLIC void init_plugin_vst(void) {
  setup_globals();
  setup_all();
}

/*****************************************************************************/

/* EOF */
