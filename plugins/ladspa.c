/* analyseplugin.c

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty. */

/*****************************************************************************/

#include <dlfcn.h>
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
} LPluginData;


typedef struct Data {
  LADSPA_Descriptor *ladspa_descriptor;
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

PRIVATE GHashTable *DescriptorIndex = NULL;
PRIVATE GHashTable *LPluginIndex = NULL;




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

	  if( isnan(data->outevents[i]) || isinf( data->outevents[i]) ) {
	      printf( "nan ... \n" );
	      event.d.number = 0.0;
	  } else {
	      event.d.number = data->outevents[i];
	  }
	  
	  gen_send_events(g, i, -1, &event);

	  printf( "sending ev: %f on %d \n", event.d.number, i );

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
	    return control->min;
	    return control->min * 0.5 + control->max * 0.5;
	if( LADSPA_IS_HINT_DEFAULT_HIGH( hint ) )
	    return control->min;
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
	    return 440;


	return 0.0;
    } else 
	return 0.0;

}

PRIVATE gboolean init_instance(Generator *g) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, outecount, inscount, outscount, i;
  GList *portX;
  g->data = data;

  data->ladspa_descriptor = g_hash_table_lookup( DescriptorIndex, g->klass->name );
  //printf( "retrieved: %s %x\n", g->klass->name, data->ladspa_descriptor );
  data->lpdat = g_hash_table_lookup( LPluginIndex, g->klass->name );
  data->instance_handle = data->ladspa_descriptor->instantiate( data->ladspa_descriptor, SAMPLE_RATE ); 
  //printf( "got instancehandle: %x\n", data->instance_handle );

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
  gen_deregister_realtime_fn(g, realtime_handler);
  // TODO: what a mess free it all ....
  free(g->data);
}

PRIVATE void unpickle_instance(Generator *g, ObjectStoreItem *item, ObjectStore *db) {
  Data *data = safe_malloc(sizeof(Data));
  int inecount, outecount, inscount, outscount, i;
  ObjectStoreDatum *inarray, *outarray;
  g->data = data;

  data->ladspa_descriptor = g_hash_table_lookup( DescriptorIndex, g->klass->name );
  data->lpdat = g_hash_table_lookup( LPluginIndex, g->klass->name );
  data->instance_handle = data->ladspa_descriptor->instantiate( data->ladspa_descriptor, SAMPLE_RATE ); 
  
  if( data->ladspa_descriptor->activate )
      data->ladspa_descriptor->activate( data->instance_handle ); 
  
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
      data->inevents[i] =
	objectstore_datum_double_value(
	  objectstore_datum_array_get(inarray, i)
	);
      data->ladspa_descriptor->connect_port( 
	      data->instance_handle, 
	      (int) g_list_nth_data( data->lpdat->inevent, i ),
	      &(data->inevents[i]) );
  }
  for( i=0; i<outecount; i++ ) {
      data->oldoutevents[i] =
	objectstore_datum_double_value(
	  objectstore_datum_array_get(outarray, i)
	);
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

  data->lastrun = 0;

  if( outscount == 0 )
      gen_register_realtime_fn(g, realtime_handler);

  //data->dummy = objectstore_item_get_integer(item, "TEMPLATE_dummy", 123456);
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

}





/*****************************************************************************/

int mainii(const int iArgc, const char ** ppcArgv) {
//  
//  const char * pcPluginName = NULL;
//  const char * pcPluginLabel = NULL;
//  int bVerbose = 1;
//
//  /* Check for a flag, but only at the start. Cannot get use getopt()
//     as it gets thoroughly confused when faced with negative numbers
//     on the command line. */
//  switch (iArgc) {
//  case 2:
//    if (strcmp(ppcArgv[1], "-h") != 0) {
//      pcPluginName = ppcArgv[1];
//      pcPluginLabel = NULL;
//    }
//    break;
//  case 3:
//    if (strcmp(ppcArgv[1], "-l") == 0) {
//      pcPluginName = ppcArgv[2];
//      pcPluginLabel = NULL;
//      bVerbose = 0;
//    }
//    else {
//      pcPluginName = ppcArgv[1];
//      pcPluginLabel = ppcArgv[2];
//    }
//    break;
//  case 4:
//    if (strcmp(ppcArgv[1], "-l") == 0) {
//      pcPluginName = ppcArgv[2];
//      pcPluginLabel = ppcArgv[3];
//      bVerbose = 0;
//    }
//    break;
//  }
//
//  if (!pcPluginName) {
//    fprintf(stderr, 
//	    "Usage:\tanalyseplugin [flags] <LADSPA plugin file name> "
//	    "[<plugin label>].\n"
//	    "Flags:"
//	    "\t-l\tProduce a summary list rather than a verbose report.\n");
//    return(1);
//  }
//
//  return analysePlugin(pcPluginName, pcPluginLabel, bVerbose);
 return 0;
}
/*
PRIVATE void setup_class(void) {
  GeneratorClass *k = gen_new_generatorclass(GENERATOR_CLASS_NAME, FALSE,
					     NUM_EVENT_INPUTS, NUM_EVENT_OUTPUTS,
					     input_sigs, output_sigs, controls,
					     init_instance, destroy_instance,
					     unpickle_instance, pickle_instance);

  gen_configure_event_input(k, EVT_INPUT, "Input", evt_input_handler);
  gen_configure_event_output(k, EVT_OUTPUT, "Output");

  gencomp_register_generatorclass(k, FALSE, GENERATOR_CLASS_PATH,
				  PIXMAPDIRIFY(GENERATOR_CLASS_PIXMAP),
				  NULL);
}
*/

AGenerator_t output_generators[] = { output_generator0, output_generator1, output_generator2, output_generator3,
				     output_generator4, output_generator5, output_generator6, output_generator7 };
PRIVATE int plugin_count=0;

PUBLIC void control_LADSPA_Data_updater(Control *c) {
    Data *data=c->g->data;

    control_set_value(c, (data->inevents[(int) c->desc->refresh_data]));
}
				     
PRIVATE void setup_one_class( const LADSPA_Descriptor *psDescriptor ) {

    unsigned long i;
    LPluginData *plugindata = safe_malloc( sizeof( LPluginData ) );
    GList *portX;
    InputSignalDescriptor *inputdescr;
    OutputSignalDescriptor *outputdescr;
    ControlDescriptor *controls;
    char *generatorpath;

    GeneratorClass *k;
    
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
	inputdescr[i].name  = psDescriptor->PortNames[portindex];
	inputdescr[i].flags = SIG_FLAG_REALTIME;
    }
    inputdescr[i].name = NULL;
   
    //printf( "Output Signals:\n" );
    for( i=0,portX = plugindata->outsig; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	//printf( "Name: %s\n", psDescriptor->PortNames[portindex] );
	outputdescr[i].name  = psDescriptor->PortNames[portindex];
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

	controls[i].name = psDescriptor->PortNames[portindex];

	if( LADSPA_IS_HINT_BOUNDED_BELOW( psDescriptor->PortRangeHints[portindex].HintDescriptor ) )
	    controls[i].min = psDescriptor->PortRangeHints[portindex].LowerBound;
	else
	    controls[i].min = 0.0;
	
	if( LADSPA_IS_HINT_BOUNDED_ABOVE( psDescriptor->PortRangeHints[portindex].HintDescriptor ) )
	    controls[i].max = psDescriptor->PortRangeHints[portindex].UpperBound;
	else
	    controls[i].max = 0.0;

	if( LADSPA_IS_HINT_SAMPLE_RATE( psDescriptor->PortRangeHints[portindex].HintDescriptor ) ) {
	    controls[i].min *= SAMPLE_RATE;
	    controls[i].max *= SAMPLE_RATE;
	}
	if( LADSPA_IS_HINT_INTEGER( psDescriptor->PortRangeHints[portindex].HintDescriptor ) ) {
	    controls[i].step = 1.0;
	    controls[i].page = 1.0;
	} else {
	    controls[i].step = 0.01;
	    controls[i].page = 0.01;
	}

	controls[i].size = 0;
	controls[i].allow_direct_edit = TRUE;
	controls[i].is_dst_gen = TRUE;
	controls[i].queue_number = i;

	// { CONTROL_KIND_KNOB, "attack", 0,5,0.01,0.01, 0,TRUE, TRUE,EVT_ATTACK_TIME,
	//   NULL,NULL, control_double_updater, (gpointer) offsetof(Data, attack) },


	controls[i].initialize = NULL;
	controls[i].destroy = NULL;
	controls[i].refresh = control_LADSPA_Data_updater;
	controls[i].refresh_data = (void *)i;
    }
    
    controls[i].kind = CONTROL_KIND_NONE;
   
    k = gen_new_generatorclass( psDescriptor->Label, FALSE,
				g_list_length( plugindata->inevent ), g_list_length( plugindata->outevent ),
				inputdescr, outputdescr, controls,
				init_instance, destroy_instance,
				unpickle_instance, pickle_instance);

    for( i=0,portX = plugindata->inevent; portX != NULL; i++,portX = g_list_next( portX ) ) {
	unsigned long portindex = (unsigned long) portX->data;

	gen_configure_event_input(k, i, psDescriptor->PortNames[portindex], evt_input_handler);
    }

    //printf( "Output Events:\n" );
    for( i=0,portX = plugindata->outevent; portX != NULL; i++,portX = g_list_next( portX ) )
    {
	unsigned long portindex = (unsigned long) portX->data;

	gen_configure_event_output(k, i, psDescriptor->PortNames[portindex] );
    }
   
   
    
    //printf( "inserting: %s %x\n", psDescriptor->Label, psDescriptor );
    g_hash_table_insert(DescriptorIndex, (char *)psDescriptor->Label, (LADSPA_Descriptor *) psDescriptor);
    g_hash_table_insert(LPluginIndex, (char *)psDescriptor->Label, plugindata);

	
    generatorpath = safe_malloc( strlen("LADSPA00/")+strlen( psDescriptor->Name )+1 ); 
    sprintf( generatorpath, "LADSPA%2d/%s", (plugin_count++) / 20, psDescriptor->Name );
    //strcpy( generatorpath, "LADSPA/" );
    //strcat( generatorpath, psDescriptor->Name );


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


typedef struct ladspa_plugin {
    char *pluginname;
} ladspa_plugin;

PRIVATE void init_one_plugin( const char *filename, void *handle, LADSPA_Descriptor_Function fDescriptorFunction) {

  unsigned long lPluginIndex;
  //unsigned long lLength;
  const LADSPA_Descriptor *psDescriptor;


  if (!fDescriptorFunction) {
    const char * pcError = dlerror();
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

    setup_one_class( psDescriptor );
  }
}

//PRIVATE void load_plugin( char *filename ) {
//
//  LADSPA_Descriptor_Function pfDescriptorFunction;
//  const LADSPA_Descriptor * psDescriptor;
//  unsigned long lPluginIndex;
//  //unsigned long lPortIndex;
//  //unsigned long lSpaceIndex;
//  //unsigned long lSpacePadding1;
//  //unsigned long lSpacePadding2;
//  //unsigned long lLength;
//  void * pvPluginHandle;
//  //LADSPA_PortRangeHintDescriptor iHintDescriptor;
//  //LADSPA_Data fBound;
//
//  pvPluginHandle = loadLADSPAPluginLibrary( filename );
//
//  dlerror();
//  pfDescriptorFunction 
//    = (LADSPA_Descriptor_Function)dlsym(pvPluginHandle, "ladspa_descriptor");
//  if (!pfDescriptorFunction) {
//    const char * pcError = dlerror();
//    if (pcError) 
//      fprintf(stderr,
//	      "Unable to find ladspa_descriptor() function in plugin file "
//	      "\"%s\": %s.\n"
//	      "Are you sure this is a LADSPA plugin file?\n", 
//	      filename,
//	      pcError);
//    return;
//  }
//
//  for (lPluginIndex = 0;; lPluginIndex++) {
//    psDescriptor = pfDescriptorFunction(lPluginIndex);
//    if (!psDescriptor)
//      break;
//
//    setup_one_class( psDescriptor );
//  }
//}


PRIVATE void setup_all( void ) {
    LADSPAPluginSearch(init_one_plugin);
}

PRIVATE void setup_globals( void ) {

  DescriptorIndex = g_hash_table_new(g_str_hash, g_str_equal);
  LPluginIndex = g_hash_table_new(g_str_hash, g_str_equal);
}


PUBLIC void init_plugin_ladspa(void) {
  setup_globals();
  setup_all();
  //setup_class();
}

/*****************************************************************************/

/* EOF */