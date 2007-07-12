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
#include "control.h"
#include "msgbox.h"

#include "galan_jack.h"

/**
 * \brief This is a hash mapping from string -> GeneratorClass
 *
 * This is used when a Generator is unpickled to find the GeneratorClass from the name tag.
 */

PRIVATE GHashTable *generatorclasses = NULL;

/**
 * \brief This queue contains EventLinks which should be added to the system.
 */

PRIVATE GAsyncQueue *gen_link_queue;
/**
 * \brief This queue contains EventLinks which should be removed from the system.
 */

PRIVATE GAsyncQueue *gen_unlink_queue;

/**
 * \brief This queue contains Generator s which shall be killed.
 *        These will be passed to a free thread but at the moment will only use safe_free.
 */

PRIVATE GAsyncQueue *gen_kill_queue;
PRIVATE GAsyncQueue *gen_kill_queue_stage2;

PRIVATE GThread *kill_thread;

PRIVATE int gen_samplerate = 44100;

PUBLIC int gen_get_sample_rate( void ) {
    return gen_samplerate;
}

/**
 * \brief Initialize an AEvent.
 * 
 * Fills in the fields of an AEvent.
 *
 * \param e AEvent to fill.
 * \param kind kind of the AEvent.
 * \param src Where the event comes from.
 * \param src_q The Connector number the AEvent comes from.
 * \param dst The Generator which is to receive the AEvent.
 * \param dst_q The Connector number which receives the event.
 * \param time The SAMPLETIME when the event shall be processed.
 *
 */

PUBLIC void gen_init_aevent(AEvent *e, AEventKind kind,
			    Generator *src, int src_q,
			    Generator *dst, int dst_q, SAMPLETIME time) {
  g_return_if_fail(e != NULL);

  e->kind = kind;
  e->src = src;
  e->src_q = src_q;
  e->dst = dst;
  e->dst_q = dst_q;
  e->time = time;
}

/**
 * \brief Registers a new GeneratorClass with the system.
 *
 * \param name The Name Tag of the GenratorClass (This must be unique as it is used to find the GenratorClass on load)
 * \param prefer If a GenratorClass with the same name exists should this be overwritten.
 * \param count_event_in Number of Input AEvents the GenratorClass will have.
 * \param count_event_out Number of Output AEvents the GenratorClass will have.
 * \param input_sigs An Array of InputSignalDescriptor describing the Input Signals. (Can be NULL)
 * \param output_sigs An Array of OutputSignalDescriptor describing the Output Signals. (Can be NULL)
 * \param controls An Array of ControlDescriptor describing the Controls of the GenratorClass. (Can bu NULL)
 * \param initializer The Constructor Function for the GeneratorClass specific initialisation. (Can be NULL)
 * \param destructor The Destructor Function for the GeneratorClass specific destruction. (Can be NULL)
 * \param unpickle_instance Unpickle Function
 * \param pickle_instance Pickle Function
 *
 */

PUBLIC GeneratorClass *gen_new_generatorclass(const char *name, gboolean prefer,
					      gint count_event_in, gint count_event_out,
					      InputSignalDescriptor *input_sigs,
					      OutputSignalDescriptor *output_sigs,
					      ControlDescriptor *controls,
					      int (*initializer)(Generator *),
					      void (*destructor)(Generator *),
					      AGenerator_pickle_t unpickle_instance,
					      AGenerator_pickle_t pickle_instance) {

    return
	gen_new_generatorclass_with_different_tag( name, name, prefer,
		count_event_in, count_event_out,
		input_sigs,
		output_sigs,
		controls,
		initializer,
		destructor,
		unpickle_instance,
		pickle_instance );
}
/**
 * \brief Registers a new GeneratorClass with the system.
 *
 * \param name The Name of the GenratorClass which is presented to the humen (you know... those who cant remeber numbers)
 * \param tag The Tag of the GenratorClass (This must be unique as it is used to find the GenratorClass on load)
 * \param prefer If a GenratorClass with the same name exists should this be overwritten.
 * \param count_event_in Number of Input AEvents the GenratorClass will have.
 * \param count_event_out Number of Output AEvents the GenratorClass will have.
 * \param input_sigs An Array of InputSignalDescriptor describing the Input Signals. (Can be NULL)
 * \param output_sigs An Array of OutputSignalDescriptor describing the Output Signals. (Can be NULL)
 * \param controls An Array of ControlDescriptor describing the Controls of the GenratorClass. (Can bu NULL)
 * \param initializer The Constructor Function for the GeneratorClass specific initialisation. (Can be NULL)
 * \param destructor The Destructor Function for the GeneratorClass specific destruction. (Can be NULL)
 * \param unpickle_instance Unpickle Function
 * \param pickle_instance Pickle Function
 *
 */

PUBLIC GeneratorClass *gen_new_generatorclass_with_different_tag(const char *name, const char *tag, gboolean prefer,
					      gint count_event_in, gint count_event_out,
					      InputSignalDescriptor *input_sigs,
					      OutputSignalDescriptor *output_sigs,
					      ControlDescriptor *controls,
					      int (*initializer)(Generator *),
					      void (*destructor)(Generator *),
					      AGenerator_pickle_t unpickle_instance,
					      AGenerator_pickle_t pickle_instance) {

  GeneratorClass *k = safe_malloc(sizeof(GeneratorClass));

  k->name = safe_string_dup(name);
  k->tag = safe_string_dup(tag);

  k->in_count = count_event_in;
  k->out_count = count_event_out;

  k->in_sigs = input_sigs;
  k->out_sigs = output_sigs;
  k->controls = controls;

  if (input_sigs == NULL)
    k->in_sig_count = 0;
  else
    for (k->in_sig_count = 0;
	 k->in_sigs[k->in_sig_count].name != NULL;
	 k->in_sig_count++) ;

  if (output_sigs == NULL)
    k->out_sig_count = 0;
  else
    for (k->out_sig_count = 0;
	 k->out_sigs[k->out_sig_count].name != NULL;
	 k->out_sig_count++) ;

  if (controls == NULL)
    k->numcontrols = 0;
  else
    for (k->numcontrols = 0;
	 k->controls[k->numcontrols].kind != CONTROL_KIND_NONE;
	 k->numcontrols++) ;

  if( count_event_in > 0 ) {
	  k->in_names = safe_calloc(count_event_in, sizeof(char *));
	  k->in_handlers = safe_calloc(count_event_in, sizeof(AEvent_handler_t));
  }
  if( count_event_out > 0 ) 
	  k->out_names = safe_calloc(count_event_out, sizeof(char *));

  k->initialize_instance = initializer;
  k->destroy_instance = destructor;
  k->unpickle_instance = unpickle_instance;
  k->pickle_instance = pickle_instance;

  /* Only insert into hash table if this name is not already taken, or if we are preferred. */
  {
    GeneratorClass *oldk = g_hash_table_lookup(generatorclasses, k->tag);

    if (oldk == NULL)
      g_hash_table_insert(generatorclasses, k->tag, k);
    else if (prefer) {
      g_hash_table_remove(generatorclasses, k->tag);
      g_hash_table_insert(generatorclasses, k->tag, k);
    }
  }

  return k;
}

/**
 * \brief Free Memory used by a GeneratorClass
 *
 * If you have some Instances of this GenratorClass in your Memory and kill the Class
 * expect Segfaults.
 *
 * \param g GenratorClass to free.
 */

PUBLIC void gen_kill_generatorclass(GeneratorClass *g) {
  int i;

  free(g->name);
  free(g->tag);

  for (i = 0; i < g->in_count; i++)
    if (g->in_names[i] != NULL)
      free(g->in_names[i]);
  free(g->in_names);
  free(g->in_handlers);

  for (i = 0; i < g->out_count; i++)
    if (g->out_names[i] != NULL)
      free(g->out_names[i]);
  free(g->out_names);

  free(g);
}

/**
 * \brief Setup EventInput \a index on GenratorClass \a g.
 *
 * \param g GenratorClass you want to setup.
 * \param index Number of EventInput you want to setup.
 * \param name The Name of the Input Connector which will show up in the application.
 * \param handler When an Event is received on the input this function will be called.
 *
 * TODO: change the g to a k.
 */

PUBLIC void gen_configure_event_input(GeneratorClass *g, gint index,
				      const char *name, AEvent_handler_t handler) {
  if (g->in_names[index] != NULL)
    g_warning("Event input already configured: class (%s tag: %s), index %d, name %s, existing name %s",
	      g->name, g->tag, index, name, g->in_names[index]);

  g->in_names[index] = safe_string_dup(name);
  g->in_handlers[index] = handler;
}

/**
 * \brief Setup EventOutput \a index on GenratorClass \a g.
 *
 * \param g GenratorClass you want to setup.
 * \param index Number of EventInput you want to setup.
 * \param name The Name of the Output Connector which will show up in the application.
 *
 */
PUBLIC void gen_configure_event_output(GeneratorClass *g, gint index, const char *name) {
  if (g->out_names[index] != NULL)
    g_warning("Event output already configured: class %s, index %d, name %s, existing name %s",
	      g->name, index, name, g->out_names[index]);

  g->out_names[index] = safe_string_dup(name);
}

PRIVATE GList **make_event_list(gint count) {
  GList **l = safe_calloc(count, sizeof(GList *));
  return l;
}

/**
 * \brief Instantiate a new Genrator.
 *
 * \param k The GenratorClass of the Generator.
 * \param name The name of the new instance.
 * \return The New Generator. 
 */

PUBLIC Generator *gen_new_generator(GeneratorClass *k, char *name) {
    
  Generator *g = safe_malloc(sizeof(Generator));
  int i;

  g->klass = k;
  g->name = safe_string_dup(name);

  g->in_events = make_event_list(k->in_count);
  g->out_events = make_event_list(k->out_count);
  g->in_signals = make_event_list(k->in_sig_count);
  g->out_signals = make_event_list(k->out_sig_count);

  //g->input_events = NULL;

  g->last_sampletime = safe_calloc( k->out_sig_count, sizeof( SAMPLETIME) );
  g->last_buffers = safe_calloc(k->out_sig_count, sizeof(SAMPLE *));
  g->last_buflens = safe_calloc(k->out_sig_count, sizeof(int));
  g->last_results = safe_calloc(k->out_sig_count, sizeof(gboolean));

  for( i=0; i<k->out_sig_count; i++ )
      g->last_buffers[i] = safe_malloc( sizeof(SAMPLE) * MAXIMUM_REALTIME_STEP );

  g->controls = NULL;

  g->data = NULL;

  if (k->initialize_instance != NULL &&
      !k->initialize_instance(g)) {
    gen_kill_generator(g);
    return NULL;
  }

  return g;
}

PRIVATE void empty_link_list(GList *l, int is_signal, int outbound) {
  while (l != NULL) {
    GList *next = g_list_next(l);
    EventLink *el = l->data;
    GList **rl;

    if (outbound)
      rl = &( (is_signal ? el->dst->in_signals : el->dst->in_events) [el->dst_q] );
    else
      rl = &( (is_signal ? el->src->out_signals : el->src->out_events) [el->src_q] );

    *rl = g_list_remove(*rl, el);

    g_list_free_1(l);
    l = next;
  }
}

PRIVATE void empty_all_connections(gint count, GList **array, int is_signal, int outbound) {
  int i;

  for (i = 0; i < count; i++)
    empty_link_list(array[i], is_signal, outbound);
}

/**
 * \brief Free Memory of Generator \a g
 *
 * \param g The Genrator to be freed.
 *
 * Make this function threadsafe:
 *   pass \a g to the audio thread via GAsyncQueue 
 *   in audio thread empty_all_connections() and gen_purge_event_queue_ref().
 *   empty_all_connections() uses g_list_free_1() shit i must obtain the malloc lock for this.
 *
 *    BTW: i need to change the g_free() function to use the free_thread i will implement.
 *   
 *   then pass on to another thread which does the real destruction of the generator.
 *
 *   looks like i only need to change event_allocation functions. and event_free
 *   normally event is only copied by the generator framework.
 */

PUBLIC void gen_kill_generator(Generator *g) {
    g_async_queue_push( gen_kill_queue, g );
}

PRIVATE void check_for_kills( void ) {

  Generator *g;

  while( (g = g_async_queue_try_pop( gen_kill_queue )) != NULL ) {
      gen_purge_event_queue_refs(g);

      empty_all_connections(g->klass->in_count, g->in_events, 0, 0);
      empty_all_connections(g->klass->out_count, g->out_events, 0, 1);
      empty_all_connections(g->klass->in_sig_count, g->in_signals, 1, 0);
      empty_all_connections(g->klass->out_sig_count, g->out_signals, 1, 1);

      //g_print( "Hello a kill \n" );
      g_async_queue_push( gen_kill_queue_stage2, g );
  }
}

PRIVATE gint gen_kill_generator_stage2_thread() {

    Generator *g;
    int i;
    while( (g = g_async_queue_pop( gen_kill_queue_stage2 )) != (gpointer) -1 )
    {
	if (g->controls != NULL) {
	    GList *c = g->controls;
	    g->controls = NULL;

	    while (c != NULL) {
		GList *tmp = g_list_next(c);

		gdk_threads_enter();
		control_kill_control(c->data);
		gdk_threads_leave();
		//g_list_free_1(c);
		c = tmp;
	    }
	}
	if (g->klass->destroy_instance != NULL)
	    g->klass->destroy_instance(g);


	for (i = 0; i < g->klass->out_sig_count; i++)
	    if (g->last_buffers[i] != NULL)
		safe_free(g->last_buffers[i]);


	safe_free(g->name);
	safe_free(g->in_events);
	safe_free(g->out_events);
	safe_free(g->in_signals);
	safe_free(g->out_signals);
	safe_free(g->last_buffers);
	safe_free(g->last_buflens);
	safe_free(g->last_results);
	safe_free(g);
    }
    return 0;
}

PRIVATE void unpickle_eventlink(ObjectStoreItem *item) {
  int is_signal = objectstore_item_get_integer(item, "is_signal", FALSE);
  Generator *src = gen_unpickle(objectstore_item_get_object(item, "src"));
  int src_q = objectstore_item_get_integer(item, "src_q", 0);
  Generator *dst = gen_unpickle(objectstore_item_get_object(item, "dst"));
  int dst_q = objectstore_item_get_integer(item, "dst_q", 0);

  gen_link(is_signal, src, src_q, dst, dst_q);
}

PRIVATE void unpickle_eventlink_list_array(ObjectStoreDatum *array, ObjectStore *db) {
  int arraylen = objectstore_datum_array_length(array);
  int i;

  for (i = 0; i < arraylen; i++) {
    ObjectStoreDatum *row = objectstore_datum_array_get(array, i);
    int rowlen = objectstore_datum_array_length(row);
    int j;

    for (j = 0; j < rowlen; j++) {
      ObjectStoreDatum *elt = objectstore_datum_array_get(row, j);
      ObjectStoreKey key = objectstore_datum_object_key(elt);
      ObjectStoreItem *item = objectstore_get_item_by_key(db, key);
      unpickle_eventlink(item);
    }
  }
}

/**
 * \brief unpickles a Generator from the ObjectStoreItem \a item
 *
 * \param ObjectStoreItem representing the Generator
 * \return The Generator.
 */

PUBLIC Generator *gen_unpickle(ObjectStoreItem *item) {
  Generator *g = objectstore_get_object(item);
  GeneratorClass *k;
  int i;

  if( item == NULL )
      return NULL;

  if (g == NULL) {
    g = safe_malloc(sizeof(Generator));
    objectstore_set_object(item, g);

    {
      char *name = objectstore_item_get_string(item, "class_name", NULL);
      RETURN_VAL_UNLESS(name != NULL, NULL);
      k = g_hash_table_lookup(generatorclasses, name);
      if (k == NULL) {
	popup_msgbox("Class not found", MSGBOX_CANCEL, 0, MSGBOX_CANCEL,
		     "Generator-class not found: name = %s", name);
	g_message("Generator Class not found; name = %s", name);
	//free(g);
	//return NULL;
	
	// Make it the dummy class...
	// XXX: Saving does not work now.
	
	k = g_hash_table_lookup( generatorclasses, "dummy" );
      }
      g->klass = k;
    }

    g->name = safe_string_dup(objectstore_item_get_string(item, "name", "anonym"));
    g->in_events = make_event_list(k->in_count);
    g->out_events = make_event_list(k->out_count);
    g->in_signals = make_event_list(k->in_sig_count);
    g->out_signals = make_event_list(k->out_sig_count);


    g->last_sampletime = safe_calloc( k->out_sig_count, sizeof( SAMPLETIME) );
    g->last_buffers = safe_calloc(k->out_sig_count, sizeof(SAMPLE *));
    g->last_buflens = safe_calloc(k->out_sig_count, sizeof(int));
    g->last_results = safe_calloc(k->out_sig_count, sizeof(gboolean));
    for( i=0; i<k->out_sig_count; i++ )
	g->last_buffers[i] = safe_malloc( sizeof(SAMPLE) * MAXIMUM_REALTIME_STEP );

    g->controls = NULL;
    g->data = NULL;

    if (g->klass->unpickle_instance != NULL)
      g->klass->unpickle_instance(g, item, item->db);

    unpickle_eventlink_list_array(objectstore_item_get(item, "out_events"), item->db);
    unpickle_eventlink_list_array(objectstore_item_get(item, "out_signals"), item->db);
    g->controls = objectstore_extract_list_of_items(objectstore_item_get(item, "controls"),
						    item->db,
						    (objectstore_unpickler_t) control_unpickle);
    g_list_foreach(g->controls, (GFunc) control_update_value, NULL);
  }

  return g;
}

PRIVATE ObjectStoreItem *pickle_eventlink(EventLink *el, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_new_item(db, "EventLink", el);
  objectstore_item_set_integer(item, "is_signal", el->is_signal);
  objectstore_item_set_object(item, "src", gen_pickle(el->src, db));
  objectstore_item_set_integer(item, "src_q", el->src_q);
  objectstore_item_set_object(item, "dst", gen_pickle(el->dst, db));
  objectstore_item_set_integer(item, "dst_q", el->dst_q);
  return item;
}

PRIVATE ObjectStoreDatum *pickle_eventlink_list_array(ObjectStore *db, GList **ar, int n) {
  ObjectStoreDatum *array = objectstore_datum_new_array(n);
  int i;

  for (i = 0; i < n; i++)
    objectstore_datum_array_set(array, i,
				objectstore_create_list_of_items(ar[i], db,
								 (objectstore_pickler_t)
								   pickle_eventlink));

  return array;
}

/**
 * \brief Pickle the Generator \a g into ObjectStore \a db.
 *
 * \param g The Generator to be pickled.
 * \param db The ObjectStore into which the Generator will be inserted.
 */


PUBLIC ObjectStoreItem *gen_pickle(Generator *g, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_get_item(db, g);

  if (item == NULL) {
    item = objectstore_new_item(db, "Generator", g);
    objectstore_item_set_string(item, "class_name", g->klass->tag);
    objectstore_item_set_string(item, "name", g->name);
    objectstore_item_set(item, "out_events",
			 pickle_eventlink_list_array(db, g->out_events, g->klass->out_count));
    objectstore_item_set(item, "out_signals",
			 pickle_eventlink_list_array(db, g->out_signals, g->klass->out_sig_count));

    if (g->klass->pickle_instance != NULL)
      g->klass->pickle_instance(g, item, db);

    objectstore_item_set(item, "controls",
			 objectstore_create_list_of_items(g->controls, db,
							  (objectstore_pickler_t) control_pickle));
  }

  return item;
}

/**
 * \brief Pickle the Generator \a g into ObjectStore \a db without EventLink s.
 *
 * \param g The Generator to be pickled.
 * \param db The ObjectStore into which the Generator will be inserted.
 *
 * \return The ObjectStoreItem for this Generator. If it does not exist
 *         It will be created by this function. If it already exists 
 *         only the pointer will be returned.
 *
 * This function pickles a Generator into an ObjectStore, like gen_pickle(),
 * but it does not pickle the event links.
 *
 * This is for copy paste. I only modify the core.
 * The interface, the plugins speak to, is not changed.
 * i can now write a copy function for a gencomp.
 * I must still determine the wanted connecttions and pickle them as well.
 * i will see how this will work....
 */

PUBLIC ObjectStoreItem *gen_pickle_without_el(Generator *g, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_get_item(db, g);

  if (item == NULL) {
    item = objectstore_new_item(db, "Generator", g);
    objectstore_item_set_string(item, "class_name", g->klass->tag);
    objectstore_item_set_string(item, "name", g->name);

    objectstore_item_set(item, "out_events",
			 pickle_eventlink_list_array(db, g->out_events, 0));
    objectstore_item_set(item, "out_signals",
			 pickle_eventlink_list_array(db, g->out_signals, 0));

    if (g->klass->pickle_instance != NULL)
      g->klass->pickle_instance(g, item, db);

    objectstore_item_set(item, "controls", objectstore_datum_new_array(0) );
    
    // XXX: Controls should also be copied but they have a reference to
    //      the sheet so i would need a different unpickler.
    //      I wont do that now.

    //objectstore_item_set(item, "controls",
    //		 objectstore_create_list_of_items(g->controls, db,
    //						  (objectstore_pickler_t) control_pickle));
  }

  return item;
}


/*
 * \brief Clone the current generator.
 *
 * \param g The Generator to be cloned.
 * \param cp ControlPanel where the generator resides.
 * 
 * the generator will be cloned. And its controls will be cloned also.
 * they will then sit on the the new controlpanel.
 */

PUBLIC Generator *gen_clone( Generator *src, ControlPanel *cp ) {
    ObjectStore *db = objectstore_new_objectstore();
    ObjectStoreItem *it = gen_pickle_without_el( src, db );
    Generator *dst;
    GList *controlX;
    
    objectstore_set_object( it, NULL );
    dst = gen_unpickle( it );

    objectstore_kill_objectstore( db );

    for( controlX = src->controls; controlX; controlX = g_list_next( controlX ) ) {
	Control *c = controlX->data;
	control_clone( c, dst, cp );
    }

    return dst;
}

/**
 * \brief Setup a Link between 2 Genrators.
 *
 * \param is_signal TRUE if a Signal Link is to be established, FALSE for an event Link.
 * \param src The Source Generator.
 * \param src_q The Connector number of the Source.
 * \param dst The Destination Genrator.
 * \param dst_q The Connector number at the Destination.
 *
 * \return The EventLink representing this Connection or NULL on failure.
 */

PUBLIC EventLink *gen_link(int is_signal, Generator *src, gint32 src_q, Generator *dst, gint32 dst_q) {
  EventLink *el = gen_find_link(is_signal, src, src_q, dst, dst_q);

  //g_print( "gen_link_s1_enter() \n" );
  if (el != NULL)
    return el;

  RETURN_VAL_UNLESS(src_q >= 0 && dst_q >= 0, NULL);

  if (is_signal) {
    guint32 infl, outfl;

    if (src_q >= src->klass->out_sig_count || dst_q >= dst->klass->in_sig_count)
      return NULL;

    infl = dst->klass->in_sigs[dst_q].flags;
    outfl = src->klass->out_sigs[src_q].flags;

    if ((infl & outfl) == 0) {
      /* No overlap in signaldescriptor capabilities: fail. */
      return NULL;
    }
  } else {
    if (src_q >= src->klass->out_count || dst_q >= dst->klass->in_count)
      return NULL;
  }

  el = safe_malloc(sizeof(EventLink));

  el->is_signal = is_signal;
  el->src = src;
  el->src_q = src_q;
  el->dst = dst;
  el->dst_q = dst_q;

  /*
   * all setup done now pass this structure to the
   * realtime thread to actually establish the link.
   */

  g_async_queue_push( gen_link_queue, el );

  //g_print( "gen_link_s1_exit() \n" );
  return el;
}

PRIVATE void check_for_gen_links() {
    
    EventLink *el;
    while( (el = g_async_queue_try_pop( gen_link_queue )) != NULL ) {

	GList **outq;
	GList **inq;

	//g_print( "Enter gen_link_audio \n" );
	outq = el->is_signal ? el->src->out_signals : el->src->out_events;
	inq = el->is_signal ? el->dst->in_signals : el->dst->in_events;

	outq[el->src_q] = g_list_prepend(outq[el->src_q], el);
	inq[el->dst_q] = g_list_prepend(inq[el->dst_q], el);

	//g_print( "exit gen_link_audio()\n" );
    }

}

/**
 * \brief find a Link between 2 Generators.
 *
 * \param is_signal TRUE if a Signal Link is to be found, FALSE for an event Link.
 * \param src The Source Generator.
 * \param src_q The Connector number of the Source.
 * \param dst The Destination Genrator.
 * \param dst_q The Connector number at the Destination.
 *
 * \return The EventLink representing this Connection.
 */

PUBLIC EventLink *gen_find_link(int is_signal,
				Generator *src, gint32 src_q,
				Generator *dst, gint32 dst_q) {
  GList *l;

  if (src_q >= (is_signal ? src->klass->out_sig_count : src->klass->out_count))
    return NULL;

  l = (is_signal ? src->out_signals : src->out_events) [src_q];

  while (l != NULL) {
    EventLink *el = l->data;

    if (el->dst == dst && el->dst_q == dst_q &&
	el->src == src && el->src_q == src_q &&
	el->is_signal == is_signal)
      return el;

    l = g_list_next(l);
  }

  return NULL;
}

/**
 * \brief Unlink 2 Generators
 *
 * \param el The EventLink which is to be deleted. Use gen_find_link if you have not saved the EventLink
 *           from the gen_link.
 */

PUBLIC void gen_unlink(EventLink *el) {
    g_async_queue_push( gen_unlink_queue, el );
}

PRIVATE void check_for_gen_unlinks( void ) {

    EventLink *el;
    
    while( (el = g_async_queue_try_pop( gen_unlink_queue )) != NULL ) {

	GList **outq;
	GList **inq;

	g_return_if_fail(el != NULL);

	outq = (el->is_signal ? el->src->out_signals : el->src->out_events);
	inq = (el->is_signal ? el->dst->in_signals : el->dst->in_events);

	outq[el->src_q] = g_list_remove(outq[el->src_q], el);
	inq[el->dst_q] = g_list_remove(inq[el->dst_q], el);

	safe_free(el);
    }
}

/**
 * \brief Do the checks of the GAsyncQueues for destruction and gen_(un)link
 */

PUBLIC void gen_mainloop_do_checks( void ) {
    check_for_gen_links();
    check_for_gen_unlinks();
    check_for_kills();
}


/** \brief Tells the Generator that it has a new Control
 * 
 * \param g The Generator.
 * \param c The Control.
 *
 * \note This needs to get thread_safe.
 */

PUBLIC void gen_register_control(Generator *g, Control *c) {
  g->controls = g_list_prepend(g->controls, c);
}

/** \brief Tells the Generator that a Control is no more.
 * 
 * \param g The Generator.
 * \param c The Control.
 *
 * \note This needs to get thread_safe.
 */

PUBLIC void gen_deregister_control(Generator *g, Control *c) {
  g->controls = g_list_remove(g->controls, c);
  //GList *cl = g_list_find( g->controls, c );
  //cl->data = NULL;
}

/**
 * \brief Update all Control `s of the Generator.
 *
 * This Function calls control_update_value for every Control with the specified Type.
 * This function should be called when an event is received which changes
 * Control represented state. For example the value of a gain.
 *
 * \param g The Genrator which should update.
 * \param index The number of the Control Type which should update.
 *              or -1 for all Control Types.
 *
 * \note This function is threadsafe because control_update is.
 *       Well the controls list is not mutexed. FIXME
 *       
 */

PUBLIC void gen_update_controls(Generator *g, int index) {
    GList *cs = g->controls;
    ControlDescriptor *desc = (index == -1) ? NULL : &g->klass->controls[index];

    while (cs != NULL) {
	Control *c = cs->data;

	if (desc == NULL || c->desc == desc) {
	    control_update_value(c);  
	}
	cs = g_list_next(cs);

    }
}

/**
 * \brief read an input of Type SIG_FLAG_REALTIME into the \buffer.
 *
 * This function is used to get the realtime signal data.
 * 
 * \param g this Generator.
 * \param index which input connector
 * \param attachment_number which of the connected Generator s (should normally be -1 for all)
 * \param buffer where to put the data.
 * \param buflen how many samples should be read.
 *
 * \return TRUE if something was put into the \a buffer.
 */

PUBLIC gboolean gen_read_realtime_input(Generator *g, gint index, int attachment_number,
					SAMPLE *buffer, int buflen) {
  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->in_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->in_sigs[index].flags & SIG_FLAG_REALTIME) != 0, FALSE);

  if (attachment_number == -1 && g_list_next(g->in_signals[index]) == NULL)
    attachment_number = 0;	/* one input? don't bother summing. */

  if (attachment_number == -1) {
    SAMPLE tmp[MAXIMUM_REALTIME_STEP];
    GList *lst = g->in_signals[index];
    gboolean result = FALSE;

    memset(buffer, 0, sizeof(SAMPLE) * buflen);
    while (lst != NULL) {
      EventLink *el = lst->data;
      lst = g_list_next(lst);

      if (gen_read_realtime_output(el->src, el->src_q, tmp, buflen)) {
	int i;

	for (i = 0; i < buflen; i++)
	  buffer[i] += tmp[i];
	result = TRUE;
      }
    }

    return result;
  } else {
    GList *input_list = g_list_nth(g->in_signals[index], attachment_number);
    EventLink *el;

    if (input_list == NULL) {
      memset(buffer, 0, buflen * sizeof(SAMPLE));
      return FALSE;
    }

    el = (EventLink *) input_list->data;
    return gen_read_realtime_output(el->src, el->src_q, buffer, buflen);
  }
}

/**
 * \brief read realtime output from a generator and cache it if its necessarry.
 *
 * I dont think someone would use this function. It is used by gen_read_realtime_input()
 * to obtain the data from the individual Generator s and cache it if it will be read multiple
 * times.
 *
 * \note I must check this for malloc usage on initial call.
 *
 * \param g the Generator to be read out.
 * \param index connector number.
 * \param buffer where the data should be placed.
 * \param buflen how many SAMPLE s should be read.
 *
 * \return TRUE if data could be read.
 *
 * XXX: the event processing will be moved into this function i think.
 *      then we can do local buffer splitting which should improve
 *      performance very much... (i hope at least)
 *
 *      now to the other stuff :)
 *      how about those pure event processors ?
 *      shall we really switch to the XAP process() semantics ?
 *
 *      events could very well be processed through the chains until they
 *      get stuck on a realtime component.
 *
 *      and the plugins can still get their realtime callbacks for event
 *      emission.
 *
 *      hmm... lets see where this will take us...
 *
 *      how about queuing some events locally and queueing the others 
 *      globally ?
 *
 *      if plugin has realtime ins or outs it can process events from
 *      local queue. if not use the global queue for now....
 *
 *      cool... nice migration path... make it so.
 *
 *
 * 
 */

PUBLIC gboolean gen_read_realtime_output(Generator *g, gint index, SAMPLE *buffer, int buflen) {
  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->out_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->out_sigs[index].flags & SIG_FLAG_REALTIME) != 0, FALSE);

  if (g_list_next(g->out_signals[index]) == NULL)
    /* Don't bother caching the only output. */
    return g->klass->out_sigs[index].d.realtime(g, buffer, buflen);
  else {
    /* Cache for multiple outputs... you never know who'll be reading you, or how often */
    if (g->last_buffers[index] == NULL || g->last_sampletime[index] < gen_get_sampletime()) {
	
      /* Cache is not present, or expired */
	
      /*
       * why is this freed and malloced all the time ?
       * should be malloced only if buflen > act_buflen
       * or last_buflen if that has no side effects.
       */
	
      //if (g->last_buffers[index] != NULL)
	//free(g->last_buffers[index]);
      //g->last_buffers[index] = malloc(sizeof(SAMPLE) * buflen);
      //------------------------------------------------------------------

      g->last_buflens[index] = buflen;
      g->last_sampletime[index] = gen_get_sampletime();
      g->last_results[index] =
	g->klass->out_sigs[index].d.realtime(g, g->last_buffers[index], buflen);
    } else if (g->last_buflens[index] < buflen) {
      /* A small chunk was read. Fill it out. */
      //SAMPLE *newbuf = malloc(sizeof(SAMPLE) * buflen);
      int oldlen = g->last_buflens[index];

      /*
       * ooh... this is not necessarry at all.
       * have one buffer sized MAX_REALTIME_STEP and all should be fine.
       * handling of last_buflen must remaim.
       * the buffers need to be allocated in gen_new_generator() and gen_unpickle()
       */
      
      //if (g->last_results[index])
	//memcpy(newbuf, g->last_buffers[index], sizeof(SAMPLE) * g->last_buflens[index]);
      //else
	//memset(newbuf, 0, sizeof(SAMPLE) * g->last_buflens[index]);
      //free(g->last_buffers[index]);
      //g->last_buffers[index] = newbuf;
      g->last_buflens[index] = buflen;
      g->last_results[index] = 
	g->klass->out_sigs[index].d.realtime(g, &g->last_buffers[index][oldlen], buflen - oldlen);
    }

    if (g->last_results[index])
      memcpy(buffer, g->last_buffers[index], buflen * sizeof(SAMPLE));
    return g->last_results[index];
  }
}

/**
 * \brief Read output range for this Generator.
 *
 * This function should not be used by the user.
 * it is called by gen_get_randomaccess_input_range().
 *
 * \param g Generator to read out.
 * \param index number of output connector.
 *
 * \return the number of samples the randonacces provides.
 */

PUBLIC SAMPLETIME gen_get_randomaccess_output_range(Generator *g, gint index) {
  SAMPLETIME (*fn)(Generator *, OutputSignalDescriptor *);

  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->out_sig_count || index >= 0, 0);
  g_return_val_if_fail((g->klass->out_sigs[index].flags & SIG_FLAG_RANDOMACCESS) != 0, 0);

  fn = g->klass->out_sigs[index].d.randomaccess.get_range;

  if (fn)
    return fn(g, &g->klass->out_sigs[index]);
  else {
    g_warning("Generator (%s tag: %s) does not implement get_range", g->klass->name, g->klass->tag);
    return 0;
  }
}

/**
 * \brief Get the randomaccess input range
 *
 * \param g This Generator
 * \param index Which input connector
 * \param attachment_number which connection or -1 for all connections
 *
 * \return the length of the given RandomAccess Input.
 */

PUBLIC SAMPLETIME gen_get_randomaccess_input_range(Generator *g, gint index,
						   int attachment_number) {
  GList *input_list;
  EventLink *el;
  OutputSignalDescriptor *desc;

  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->in_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->in_sigs[index].flags & SIG_FLAG_RANDOMACCESS) != 0, FALSE);
  g_return_val_if_fail(attachment_number != -1, FALSE);

  input_list = g_list_nth(g->in_signals[index], attachment_number);
  if (input_list == NULL) {
    /*memset(buffer, 0, buflen * sizeof(SAMPLE));*/
    return 0;
  }

  el = (EventLink *) input_list->data;
  desc = &el->src->klass->out_sigs[el->src_q];
  return desc->d.randomaccess.get_range(el->src, desc);
}

/**
 * \brief Get the randomaccess input
 *
 * \param g This Generator
 * \param index Which input connector
 * \param attachment_number which connection or -1 for all connections
 * \param offset Get input from which offset.
 * \param buflen How many SAMPLE s.
 *
 * \retval buffer Where to put the data.
 * \return TRUE if the data has been put into the buffer.
 */

PUBLIC gboolean gen_read_randomaccess_input(Generator *g, gint index, int attachment_number,
					    SAMPLETIME offset, SAMPLE *buffer, int buflen) {
  GList *input_list;
  EventLink *el;
  OutputSignalDescriptor *desc;

  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->in_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->in_sigs[index].flags & SIG_FLAG_RANDOMACCESS) != 0, FALSE);
  g_return_val_if_fail(attachment_number != -1, FALSE);

  input_list = g_list_nth(g->in_signals[index], attachment_number);
  if (input_list == NULL) {
    /*memset(buffer, 0, buflen * sizeof(SAMPLE));*/
    return FALSE;
  }

  el = (EventLink *) input_list->data;
  desc = &el->src->klass->out_sigs[el->src_q];
  return desc->d.randomaccess.get_samples(el->src, desc, offset, buffer, buflen);
}


/**
 * \brief render gl inputs
 *
 * \param g This Generator
 * \param index Which input connector
 * \param attachment_number which connection or -1 for all connections
 *
 * \return TRUE if something was rendered.
 *
 * This function calls the render gl method of the connected generators.
 * See the renderfunction of gltranslate.
 *
 * \code
 * PRIVATE gboolean render_function(Generator *g ) {
 * 
 *     Data *data = g->data;
 * 
 *     glPushMatrix();
 *     glTranslatef( data->tx, data->ty, data->tz );
 *     gen_render_gl( g, 0, -1 );
 *     glPopMatrix();
 * 
 *     return TRUE;
 * }
 *
 * \endcode
 */

PUBLIC gboolean gen_render_gl(Generator *g, gint index, int attachment_number ) {

  GList *input_list;
  EventLink *el;
  OutputSignalDescriptor *desc;

  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->in_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->in_sigs[index].flags & SIG_FLAG_OPENGL) != 0, FALSE);

  if (g->in_signals[index] == NULL) {
    /*memset(buffer, 0, buflen * sizeof(SAMPLE));*/
    return FALSE;
  }

  for( input_list = g->in_signals[index]; input_list != NULL; input_list = g_list_next( input_list ) ) {

      el = (EventLink *) input_list->data;
      desc = &el->src->klass->out_sigs[el->src_q];
      desc->d.render_gl(el->src);
  }
  return TRUE;
}

PRIVATE void send_one_event(EventLink *el, AEvent *e) {
  e->dst = el->dst;
  e->dst_q = el->dst_q;
  gen_post_aevent(e);
}

/**
 * \brief send an event to an output connector
 *
 * \param g The Generator we are sending from
 * \param index The number of the Connector we send the event from.
 * \param attachment_number number of eventlink to send the event to. (-1 for all)
 * \param e Pointer to event we want to send. This is copied and can be freed after the call.
 *
 * It is general practise to use a received event as \a e here. See the event processing function of
 * plugins/evtadd.c
 *
 * \code
 * PRIVATE void evt_input_handler(Generator *g, AEvent *event) {
 * 
 *   Data *data = g->data;
 *
 *   event->d.number += data->addend;
 *   gen_send_events(g, EVT_OUTPUT, -1, event);
 * }
 * \endcode
 *
 * With this method we dont have to bother with the timestamp of the event.
 * But after being passed to the evt_input_handler() the AEvent is freed
 * by gen_mainloop_once(). if you changed the type of the event to AE_STRING
 * or AE_NUMARRAY the memory at event->d.string or event->d.array.number 
 * would be freed also. If this memory is already freed galan will segfault.
 *
 * So be careful. Set event->kind back to AE_NUMBER if you received an AE_NUMBER AEvent.
 * I see no way to prevent you from making this error except forbidding this practise.
 * But its efficient so i dont forbid this.
 */

PUBLIC void gen_send_events(Generator *g, gint index, int attachment_number, AEvent *e) {
  e->src = g;
  e->src_q = index;

  if (attachment_number == -1)
    g_list_foreach(g->out_events[index], (GFunc) send_one_event, e);
  else {
    GList *output = g_list_nth(g->out_events[index], attachment_number);

    if (output != NULL) {
      EventLink *el = output->data;
      e->dst = el->dst;
      e->dst_q = el->dst_q;
      gen_post_aevent(e);
    }
  }
}


// Dummy Generator...

PRIVATE gboolean init_dummy(Generator *g) {
  return TRUE;
}

PRIVATE void done_dummy(Generator *g) {
}

PRIVATE gboolean dummy_output_generator(Generator *g, SAMPLE *buf, int buflen) {

    return FALSE;
}

PRIVATE void evt_dummy_handler(Generator *g, AEvent *event) {
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Input1", SIG_FLAG_REALTIME },
  { "Input2", SIG_FLAG_REALTIME },
  { "Input3", SIG_FLAG_REALTIME },
  { "Input4", SIG_FLAG_REALTIME },
  { "Input5", SIG_FLAG_REALTIME },
  { "Input6", SIG_FLAG_REALTIME },
  { "Input7", SIG_FLAG_REALTIME },
  { "Input8", SIG_FLAG_REALTIME },
  { "Input9", SIG_FLAG_REALTIME },
  { "Input10", SIG_FLAG_REALTIME },
  { "Input11", SIG_FLAG_REALTIME },
  { "Input12", SIG_FLAG_REALTIME },
  { NULL, }
};


PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output1", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output2", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output3", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output4", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output5", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output6", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output7", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output8", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output9", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output10", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output11", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { "Output12", SIG_FLAG_REALTIME, { dummy_output_generator, } },
  { NULL, }
};

PRIVATE ControlDescriptor dummy_controls[] = {
  /* { kind, name, min,max,step,page, size,editable, is_dst,queue_number,
       init,destroy,refresh,refresh_data }, */
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_KNOB, "defect", 0,5,0.01,0.01, 0,TRUE, TRUE,0,
    NULL,NULL, NULL, NULL },
  { CONTROL_KIND_NONE, }
};
PRIVATE void init_dummy_generator( void ) {

    int i;
    GeneratorClass *k = gen_new_generatorclass("dummy", FALSE,
	    120, 120,
	    input_sigs, output_sigs, dummy_controls,
	    init_dummy, done_dummy,
	    (AGenerator_pickle_t) init_dummy, NULL);


    for( i=0; i<120; i++ ) {
	gen_configure_event_output( k, i, "Out" );
	gen_configure_event_input( k, i, "In", evt_dummy_handler );
    }
}

PUBLIC GHashTable *get_generator_classes( void ) {
  return generatorclasses;
}

PUBLIC void init_generator_thread(void) {

    GError *err;
    kill_thread = g_thread_create( (GThreadFunc) gen_kill_generator_stage2_thread, NULL, TRUE, &err );
}

PUBLIC void init_generator(void) {

    gen_samplerate = jack_get_sample_rate( galan_jack_get_client() );

    gen_link_queue = g_async_queue_new();
    gen_unlink_queue = g_async_queue_new();
    gen_kill_queue = g_async_queue_new();
    gen_kill_queue_stage2 = g_async_queue_new();


    generatorclasses = g_hash_table_new(g_str_hash, g_str_equal);

    init_dummy_generator();
}

PUBLIC void done_generator(void) {
    
    g_async_queue_push( gen_kill_queue_stage2, (gpointer) -1 );
    g_thread_join( kill_thread );

    g_hash_table_destroy(generatorclasses);
    generatorclasses = NULL;
  
    g_async_queue_unref( gen_link_queue );
    g_async_queue_unref( gen_unlink_queue );
    g_async_queue_unref( gen_kill_queue );
    g_async_queue_unref( gen_kill_queue_stage2 );
}
