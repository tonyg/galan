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

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

/* %%% Win32: dirent.h seems to conflict with glib-1.3, so ignore dirent.h */
#ifndef NATIVE_WIN32
#include <dirent.h>
#endif

/* On Win32, these headers seem to need to follow glib.h */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define GENERATOR_CLASS_NAME	"ladspatest1"
#define GENERATOR_CLASS_PATH	"Misc/LADSPA"
#define GENERATOR_CLASS_PIXMAP	"template.xpm"

#define SIG_INPUT		0
#define SIG_OUTPUT		0

#define EVT_INPUT		0
#define NUM_EVENT_INPUTS	1

#define EVT_OUTPUT		0
#define NUM_EVENT_OUTPUTS	1



typedef struct LPluginData {
    GList *insig, *outsig, *inevent, *outevent;

    char *filename;
    unsigned long   index;

    GModule *module;
    const LADSPA_Descriptor *ladspa_descriptor;
    unsigned int refcount;
	
} LPluginData;

typedef struct LModuleData {
    GModule *module;
    unsigned int refcount;
} LModuleData;

typedef struct Data {
  const LADSPA_Descriptor *ladspa_descriptor;
  LADSPA_Handle instance_handle;
  LPluginData *lpdat;
  LADSPA_Data  *inevents;
  LADSPA_Data  *outevents;
  LADSPA_Data  *oldoutevents;
  LADSPA_Data  **insignals;
  LADSPA_Data  **outsignals;
  SAMPLETIME lastrun;
  
} Data;

// Globals... 

//PRIVATE GHashTable *DescriptorIndex = NULL;
PRIVATE GHashTable *LPluginIndex = NULL;
PRIVATE GHashTable *LModuleIndex = NULL;

#ifdef HAVE_LRDF
PRIVATE GRelation *PathIndex = NULL;
#endif


PRIVATE GModule *lplugindata_get_module( LPluginData *lpdat ) {

    LModuleData *moddata = g_hash_table_lookup( LModuleIndex, lpdat->filename );
    if( !moddata ) {
	moddata = safe_malloc( sizeof( LModuleData ) );
	g_hash_table_insert( LModuleIndex, lpdat->filename, moddata );
	moddata->refcount = 0;
	moddata->module =  NULL;
    }

    if( moddata->module ) {
	moddata->refcount++;
	return moddata->module;
    } else {
	moddata->module = g_module_open (lpdat->filename, G_MODULE_BIND_LAZY );
	moddata->refcount++;
	return moddata->module;
    }
}

PRIVATE void lplugindata_release_module( LPluginData *lpdat ) {

    LModuleData *moddata = g_hash_table_lookup( LModuleIndex, lpdat->filename );

    if( !moddata ) return;

    moddata->refcount--;
    if( moddata->refcount > 0 ) return;
    g_module_close( moddata->module );
    moddata->module = NULL;
}

PRIVATE const LADSPA_Descriptor *lplugindata_get_descriptor( LPluginData *lpdat ) {

    if( lpdat->ladspa_descriptor ) {
	lpdat->refcount++;
	return lpdat->ladspa_descriptor;
    }
    else
    {
	GModule * psPluginHandle;
	LADSPA_Descriptor_Function fDescriptorFunction;


	psPluginHandle = lplugindata_get_module( lpdat ); // g_module_open (lpdat->filename, G_MODULE_BIND_LAZY );
	if( !psPluginHandle ) {
	    return NULL;
	}

	if( !g_module_symbol(psPluginHandle, "ladspa_descriptor", &fDescriptorFunction ) ) {
	    lplugindata_release_module( lpdat );
	    return NULL;
	}

	if( !fDescriptorFunction ) {
	    lplugindata_release_module( lpdat );
	    return NULL;
	}

	lpdat->refcount = 0;
	lpdat->ladspa_descriptor = fDescriptorFunction( lpdat->index );
	lpdat->refcount++;
	return lpdat->ladspa_descriptor;
    }
}

PRIVATE void lplugindata_release_descriptor( LPluginData *lpdat ) {

    lpdat->refcount--;

    if( lpdat->refcount > 0 ) return;

    // try to unload...

    lpdat->ladspa_descriptor = NULL;
    lplugindata_release_module( lpdat );
}



//-------------------------------------------------------------------------
//
//   generator methods and some helpers.....
//

PRIVATE void run_plugin( Generator *g, int buflen ) {
  Data *data = g->data;
  int incount = g_list_length( data->lpdat->insig );
  int outecount = g_list_length( data->lpdat->outevent );
  int i,j;

  for( i=0; i<incount; i++ ) {

      SAMPLE buf[MAXIMUM_REALTIME_STEP];

      if( gen_read_realtime_input( g, i, -1, buf, buflen ) ) {

	  for( j=0; j<buflen; j++ )
	      data->insignals[i][j] = buf[j];

      } else {

	  for( j=0; j<buflen; j++ )
	      data->insignals[i][j] = 0.0;
      }

  }
      
  data->ladspa_descriptor->run( data->instance_handle, buflen );
  data->lastrun = gen_get_sampletime();

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
	    g_warning("ladspa module doesn't care for events of kind %d.", event->kind);
	    break;
    }
}

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

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, outecount, inscount, outscount, i;
  GList *portX;
  g->data = data;

  //XXX: may be empty.
  //data->ladspa_descriptor = g_hash_table_lookup( DescriptorIndex, g->klass->tag );

  data->lpdat = g_hash_table_lookup( LPluginIndex, g->klass->tag );

  data->ladspa_descriptor = lplugindata_get_descriptor( data->lpdat );

  data->instance_handle = data->ladspa_descriptor->instantiate( data->ladspa_descriptor, SAMPLE_RATE ); 

  // Connect the ports ... pre: alloc memory for the controls...
  // connect wird erst vor dem laufen mit outpu_generator gemacht...

  inecount = g_list_length( data->lpdat->inevent );
  outecount = g_list_length( data->lpdat->outevent );
  inscount = g_list_length( data->lpdat->insig );
  outscount = g_list_length( data->lpdat->outsig );

  data->inevents = safe_malloc( sizeof( LADSPA_Data ) * inecount  );
  data->outevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  data->oldoutevents = safe_malloc( sizeof( LADSPA_Data ) * outecount  );
  data->insignals = safe_malloc( sizeof( LADSPA_Data * ) * inscount  );
  data->outsignals = safe_malloc( sizeof( LADSPA_Data * ) * outscount  );

  for( i=0,portX = data->lpdat->inevent; portX != NULL; i++,portX = g_list_next( portX ) ) {
      unsigned long portindex = (unsigned long) portX->data;
      LADSPA_PortRangeHintDescriptor hint = data->ladspa_descriptor->PortRangeHints[portindex].HintDescriptor;
      ControlDescriptor *control = &(g->klass->controls[i]);

      data->inevents[i] = get_default( control, hint );
      //printf( "default %d = %f\n", i, data->inevents[i] );
      data->ladspa_descriptor->connect_port( 
	      data->instance_handle, 
	      (int) g_list_nth_data( data->lpdat->inevent, i ),
	      &(data->inevents[i]) );
  }
  
  for( i=0; i<outecount; i++ ) {
      data->outevents[i] = 0.0;
      data->oldoutevents[i] = 0.0;

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

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {

    Data *data = g->data;
    int inscount = g_list_length( data->lpdat->insig );
    int outscount = g_list_length( data->lpdat->outsig );
    int i;

    gen_deregister_realtime_fn(g, realtime_handler);
    if( data->ladspa_descriptor->deactivate )
	data->ladspa_descriptor->deactivate( data->instance_handle ); 

    for( i=0; i<outscount; i++ ) free( data->outsignals[i]);
    for( i=0; i<inscount; i++ ) free( data->insignals[i]);
    free( data->inevents );
    free( data->outevents );
    free( data->oldoutevents);
    free( data->insignals);
    free( data->outsignals);

    if( data->ladspa_descriptor->cleanup )
	data->ladspa_descriptor->cleanup( data->instance_handle ); 

    // TODO: destroy LADSPA instance;
    lplugindata_release_descriptor( data->lpdat );

    free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, outecount, inscount, outscount, i;
  ObjectStoreDatum *inarray, *outarray;
  g->data = data;

  //TODO: Maybe empty... load module...
  //data->ladspa_descriptor = g_hash_table_lookup( DescriptorIndex, g->klass->tag );

  data->lpdat = g_hash_table_lookup( LPluginIndex, g->klass->tag );
  data->ladspa_descriptor = lplugindata_get_descriptor( data->lpdat );

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

  gen_update_controls( g, event->dst_q );
}





/*****************************************************************************/

PRIVATE AGenerator_t output_generators[] = { output_generator0, output_generator1, output_generator2, output_generator3,
				     output_generator4, output_generator5, output_generator6, output_generator7 };
PRIVATE int plugin_count=0;

PRIVATE void control_LADSPA_Data_updater(Control *c) {
    Data *data=c->g->data;

    control_set_value(c, (data->inevents[(int) c->desc->refresh_data]));
}
				     
void string_search_and_replace( char **string, char search, char *replace ) {

    char *src = *string;
    GString *retval = g_string_new( "" );

    while( *src ) {

	if( *src == search )
	    g_string_append( retval, replace );
	else
	    g_string_append_len( retval, src, 1 );

	src++ ;
    }

    free( *string );
    *string = safe_string_dup( retval->str );
    g_string_free( retval, TRUE );
}

PRIVATE void setup_one_class( const LADSPA_Descriptor *psDescriptor, const char *filename, unsigned long plugindex ) {

    unsigned long i;
    LPluginData *plugindata = safe_malloc( sizeof( LPluginData ) );
    GList *portX;
    InputSignalDescriptor *inputdescr;
    OutputSignalDescriptor *outputdescr;
    ControlDescriptor *controls;
    char *generatorpath;

    GeneratorClass *k, *v;
    
    plugindata->insig = NULL;
    plugindata->outsig = NULL;
    plugindata->inevent = NULL;
    plugindata->outevent = NULL;

    for( i=0; i<psDescriptor->PortCount; i++ ) {
	
	LADSPA_PortDescriptor PortDescriptor=psDescriptor->PortDescriptors[i];

	if( LADSPA_IS_PORT_INPUT( PortDescriptor ) ) 

	    if( LADSPA_IS_PORT_CONTROL(PortDescriptor) )
		plugindata->inevent = g_list_append( plugindata->inevent, (gpointer) i );
	    else
		plugindata->insig = g_list_append( plugindata->insig, (gpointer) i );

	else

	    if( LADSPA_IS_PORT_CONTROL( PortDescriptor ) ) 
		plugindata->outevent = g_list_append( plugindata->outevent, (gpointer) i );
	    else
		plugindata->outsig = g_list_append( plugindata->outsig, (gpointer) i );
    } 

    inputdescr = safe_malloc( sizeof( InputSignalDescriptor ) * (g_list_length( plugindata->insig ) + 1) );
    outputdescr= safe_malloc( sizeof( OutputSignalDescriptor ) * (g_list_length( plugindata->outsig ) + 1) );
    controls = safe_malloc( sizeof( ControlDescriptor ) * (g_list_length( plugindata->inevent ) + 1) );


	
    //printf( "Input Signals:\n" );
    for( i=0,portX = plugindata->insig; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	//printf( "Name: %s\n", psDescriptor->PortNames[portindex] );
	inputdescr[i].name  = safe_string_dup( psDescriptor->PortNames[portindex] );
	inputdescr[i].flags = SIG_FLAG_REALTIME;
    }
    inputdescr[i].name = NULL;
   
    //printf( "Output Signals:\n" );
    for( i=0,portX = plugindata->outsig; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	//printf( "Name: %s\n", psDescriptor->PortNames[portindex] );
	outputdescr[i].name  = safe_string_dup( psDescriptor->PortNames[portindex] );
	outputdescr[i].flags = SIG_FLAG_REALTIME;
	outputdescr[i].d.realtime = output_generators[i];
    }
    outputdescr[i].name = NULL;

    for( i=0,portX = plugindata->inevent; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	if( LADSPA_IS_HINT_TOGGLED( psDescriptor->PortRangeHints[portindex].HintDescriptor ) )
	    controls[i].kind = CONTROL_KIND_TOGGLE;
	else
	    controls[i].kind = CONTROL_KIND_KNOB;

	controls[i].name = safe_string_dup( psDescriptor->PortNames[portindex] );
	string_search_and_replace( &( controls[i].name ), '/', "\\/" );
	

	if( LADSPA_IS_HINT_BOUNDED_BELOW( psDescriptor->PortRangeHints[portindex].HintDescriptor ) )
	    controls[i].min = psDescriptor->PortRangeHints[portindex].LowerBound;
	else
	    controls[i].min = 0.0;
	
	if( LADSPA_IS_HINT_BOUNDED_ABOVE( psDescriptor->PortRangeHints[portindex].HintDescriptor ) )
	    controls[i].max = psDescriptor->PortRangeHints[portindex].UpperBound;
	else
	    controls[i].max = 100.0;

	if( LADSPA_IS_HINT_SAMPLE_RATE( psDescriptor->PortRangeHints[portindex].HintDescriptor ) ) {
	    controls[i].min *= SAMPLE_RATE;
	    controls[i].max *= SAMPLE_RATE;
	}
	if( LADSPA_IS_HINT_INTEGER( psDescriptor->PortRangeHints[portindex].HintDescriptor ) ) {
	    controls[i].step = 1.0;
	    controls[i].page = 1.0;
	} else {
	    controls[i].page = controls[i].step = (controls[i].max - controls[i].min) / 200;
	}

	controls[i].size = 0;
	controls[i].allow_direct_edit = TRUE;
	controls[i].is_dst_gen = TRUE;
	controls[i].queue_number = i;


	controls[i].initialize = NULL;
	controls[i].destroy = NULL;
	controls[i].refresh = control_LADSPA_Data_updater;
	controls[i].refresh_data = (void *)i;
    }
    
    controls[i].kind = CONTROL_KIND_NONE;
   
    //printf( "Menu : %s\n", get_lrdf_menuname( psDescriptor->UniqueID ) );
    k = gen_new_generatorclass_with_different_tag( psDescriptor->Label, 
				g_strdup_printf( "ladspa-%d", (int) psDescriptor->UniqueID ), FALSE,
				g_list_length( plugindata->inevent ), g_list_length( plugindata->outevent ),
				inputdescr, outputdescr, controls,
				init_instance, destroy_instance,
				unpickle_instance, pickle_instance);

    v = gen_new_generatorclass( psDescriptor->Label, FALSE,
				g_list_length( plugindata->inevent ), g_list_length( plugindata->outevent ),
				inputdescr, outputdescr, controls,
				init_instance, destroy_instance,
				unpickle_instance, pickle_instance);

    for( i=0,portX = plugindata->inevent; portX != NULL; i++,portX = g_list_next( portX ) ) {
	unsigned long portindex = (unsigned long) portX->data;

	gen_configure_event_input(k, i, psDescriptor->PortNames[portindex], evt_input_handler);
	gen_configure_event_input(v, i, psDescriptor->PortNames[portindex], evt_input_handler);
    }

    //printf( "Output Events:\n" );
    for( i=0,portX = plugindata->outevent; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	gen_configure_event_output(k, i, psDescriptor->PortNames[portindex] );
	gen_configure_event_output(v, i, psDescriptor->PortNames[portindex] );
    }
   
   
    
    //printf( "inserting: %s %x\n", psDescriptor->Label, psDescriptor );
    //XXX: Is it necessary to add both tags ? Should be handled by the generator resolution.
    //
    // TODO: 
    //
    //FIXME:

    plugindata->filename = safe_string_dup( filename );
    plugindata->index = plugindex;
    plugindata->ladspa_descriptor = NULL;
    plugindata->refcount = 0;
    
    //g_hash_table_insert(DescriptorIndex, k->tag, (LADSPA_Descriptor *) psDescriptor);
    //g_hash_table_insert(DescriptorIndex, v->tag, (LADSPA_Descriptor *) psDescriptor);
    g_hash_table_insert(LPluginIndex, k->tag, plugindata);
    g_hash_table_insert(LPluginIndex, v->tag, plugindata);

	
    char *psdesc_copy = safe_string_dup( psDescriptor->Name );
    string_search_and_replace( &psdesc_copy, '/', "\\/" );
			
#ifdef HAVE_LRDF
    {
	guint uid = psDescriptor->UniqueID;
	//printf( "uid=%d\n", uid );
	GTuples *paths = g_relation_select( PathIndex, &uid, 0 );
	if( paths == NULL || paths->len == 0 ) {
	    generatorpath = g_strdup_printf( "LADSPA%2d/%s", (plugin_count++) / 20, psdesc_copy );


	    gencomp_register_generatorclass(k, FALSE, generatorpath,
		    NULL, NULL);

	    g_free( generatorpath );
	} else {
	    int i;
	    for( i=0; i<paths->len; i++ ) {
		char *genname = g_strdup_printf( "%s/%s", (char *) g_tuples_index( paths, i, 1 ), psdesc_copy );
		gencomp_register_generatorclass(k, FALSE, genname,
			NULL, NULL);
		g_free( genname );
	    }
	}	

    }
#else
    generatorpath = g_strdup_printf( "LADSPA%2d/%s", (plugin_count++) / 20, psdesc_copy );

    gencomp_register_generatorclass(k, FALSE, generatorpath,
	    NULL,
	    NULL);

    g_free( generatorpath );
#endif
    free( psdesc_copy );
    
    //printf("Plugin Name: \"%s\"\n", psDescriptor->Name);
    //printf("Plugin Label: \"%s\"\n", psDescriptor->Label);
    //printf("Plugin Unique ID: %lu\n", psDescriptor->UniqueID);
    //printf("Maker: \"%s\"\n", psDescriptor->Maker);
    //printf("Copyright: \"%s\"\n", psDescriptor->Copyright);
}


typedef struct ladspa_plugin {
    char *pluginname;
} ladspa_plugin;

PRIVATE void init_one_plugin( const char *filename, void *handle, LADSPA_Descriptor_Function fDescriptorFunction) {

  unsigned long lPluginIndex;
  //unsigned long lLength;
  const LADSPA_Descriptor *psDescriptor;


  if (!fDescriptorFunction) {
    const char * pcError = g_module_error();
    if (pcError) 
      fprintf(stderr,
	      "Unable to find ladspa_descriptor() function in plugin file "
	      "\"%s\": %s.\n"
	      "Are you sure this is a LADSPA plugin file?\n", 
	      filename,
	      pcError);
    return;
  }

  for (lPluginIndex = 0;; lPluginIndex++) {
    psDescriptor = fDescriptorFunction(lPluginIndex);
    if (!psDescriptor)
      break;

    setup_one_class( psDescriptor, filename, lPluginIndex );
  }
  g_module_close( (GModule *) handle );
}

#ifdef HAVE_LRDF


void decend(char *uri, char *base)
{
	lrdf_uris *uris;
	unsigned int i;
	char *newbase;
	char *label;

	uris = lrdf_get_instances(uri);

	if (uris != NULL) {
		for (i = 0; i < uris->count; i++) {
			guint *uid = safe_malloc( sizeof( guint ) );
			char *basedup = safe_string_dup( base );

			*uid = lrdf_get_uid( uris->items[i] );
			

			//printf("%s/[%d]\n", base, *uid );
			
			g_relation_insert( PathIndex, uid, basedup ); 
			//printf( "selct: %d\n", g_relation_count( PathIndex, uid, 0 ) );
		}
		lrdf_free_uris(uris);
	}

	uris = lrdf_get_subclasses(uri);

	if (uris != NULL) {
		for (i = 0; i < uris->count; i++) {
			label = lrdf_get_label(uris->items[i]);

			// TODO: escape label
			string_search_and_replace( &label, '/', "|" );



			newbase = malloc(strlen(base) + strlen(label) + 2);
			sprintf(newbase, "%s/%s", base, label);
			//printf("%s\n", newbase);
			decend(uris->items[i], newbase);
			free(newbase);
		}
		lrdf_free_uris(uris);
	}
}

//
// taken from jack rack
// get the path stuff done....
//

static void
load_dir_uris ( const char * dir)
{
  DIR * dir_stream;
  struct dirent * dir_entry;
  char * file_name;
  int err;
  size_t dirlen;
  char * extension;
  
  dir_stream = opendir (dir);
  if (!dir_stream)
    return;
  
  dirlen = strlen (dir);
  
  while ( (dir_entry = readdir (dir_stream)) )
    {
      /* check if it's a .rdf or .rdfs */
      extension = strrchr (dir_entry->d_name, '.');
      if (!extension)
        continue;
      if (strcmp (extension, ".rdf") != 0 &&
          strcmp (extension, ".rdfs") != 0)
        continue;
  
      file_name = g_malloc (dirlen + 1 + strlen (dir_entry->d_name) + 1 + 7);
    
      strcpy (file_name, "file://");
      strcpy (file_name + 7, dir);
      if ((file_name + 7)[dirlen - 1] == '/')
        strcpy (file_name + 7 + dirlen, dir_entry->d_name);
      else
        {
          (file_name + 7)[dirlen] = '/';
          strcpy (file_name + 7 + dirlen + 1, dir_entry->d_name);
        }
    
       lrdf_read_file( file_name );
       g_free( file_name );
    }
  
  err = closedir (dir_stream);
  if (err)
    fprintf (stderr, "error closing directory what the xxxx\n" );
}

static void
plugin_mgr_load_path_uris (void)
{
  char * lrdf_path, * dir;
  
  lrdf_path = g_strdup (getenv ("LADSPA_RDF_PATH"));
  if (!lrdf_path)
    lrdf_path = g_strdup ("/usr/local/share/ladspa/rdf:/usr/share/ladspa/rdf");
  
  dir = strtok (lrdf_path, ":");
  do
    load_dir_uris ( dir);
  while ((dir = strtok (NULL, ":")));

  g_free (lrdf_path);
}

PRIVATE void setup_lrdf( void ) {

// const char *files[] = {"file:/home/torbenh/test/liblrdf-0.2.2/examples/ladspa.rdfs",
// 			"file:/usr/share/ladspa/rdf/swh-plugins.rdf" , NULL };

 lrdf_init( );
 //lrdf_read_files( files );
 plugin_mgr_load_path_uris();

 PathIndex = g_relation_new(2);
 g_relation_index(PathIndex, 0, g_int_hash, g_int_equal);
 decend(LADSPA_BASE "Plugin", "LADSPA");
}
#endif // HAVE_LRDF

PRIVATE void setup_all( void ) {

#ifdef HAVE_LRDF  
  setup_lrdf();
#endif
    LADSPAPluginSearch(init_one_plugin);
}

PRIVATE void setup_globals( void ) {

  //DescriptorIndex = g_hash_table_new(g_str_hash, g_str_equal);
  LPluginIndex = g_hash_table_new(g_str_hash, g_str_equal);
  LModuleIndex = g_hash_table_new(g_str_hash, g_str_equal);
}


PUBLIC void init_plugin_ladspa(void) {
  setup_globals();
  setup_all();
}

/*****************************************************************************/

/* EOF */
