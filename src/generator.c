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

PRIVATE GHashTable *generatorclasses = NULL;

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

PUBLIC GeneratorClass *gen_new_generatorclass(char *name, gboolean prefer,
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

  k->in_names = safe_calloc(count_event_in, sizeof(char *));
  k->in_handlers = safe_calloc(count_event_in, sizeof(AEvent_handler_t));
  k->out_names = safe_calloc(count_event_out, sizeof(char *));

  k->initialize_instance = initializer;
  k->destroy_instance = destructor;
  k->unpickle_instance = unpickle_instance;
  k->pickle_instance = pickle_instance;

  /* Only insert into hash table if this name is not already taken, or if we are preferred. */
  {
    GeneratorClass *oldk = g_hash_table_lookup(generatorclasses, k->name);

    if (oldk == NULL)
      g_hash_table_insert(generatorclasses, k->name, k);
    else if (prefer) {
      g_hash_table_remove(generatorclasses, k->name);
      g_hash_table_insert(generatorclasses, k->name, k);
    }
  }

  return k;
}

PUBLIC void gen_kill_generatorclass(GeneratorClass *g) {
  int i;

  free(g->name);

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

PUBLIC void gen_configure_event_input(GeneratorClass *g, gint index,
				      char *name, AEvent_handler_t handler) {
  if (g->in_names[index] != NULL)
    g_warning("Event input already configured: class %s, index %d, name %s, existing name %s",
	      g->name, index, name, g->in_names[index]);

  g->in_names[index] = safe_string_dup(name);
  g->in_handlers[index] = handler;
}

PUBLIC void gen_configure_event_output(GeneratorClass *g, gint index, char *name) {
  if (g->out_names[index] != NULL)
    g_warning("Event output already configured: class %s, index %d, name %s, existing name %s",
	      g->name, index, name, g->out_names[index]);

  g->out_names[index] = safe_string_dup(name);
}

PRIVATE GList **make_event_list(gint count) {
  GList **l = safe_calloc(count, sizeof(GList *));
  return l;
}

PUBLIC Generator *gen_new_generator(GeneratorClass *k, char *name) {
  Generator *g = safe_malloc(sizeof(Generator));

  g->klass = k;
  g->name = safe_string_dup(name);

  g->in_events = make_event_list(k->in_count);
  g->out_events = make_event_list(k->out_count);
  g->in_signals = make_event_list(k->in_sig_count);
  g->out_signals = make_event_list(k->out_sig_count);

  g->last_sampletime = 0;
  g->last_buffers = safe_calloc(k->out_sig_count, sizeof(SAMPLE *));
  g->last_buflens = safe_calloc(k->out_sig_count, sizeof(int));
  g->last_results = safe_calloc(k->out_sig_count, sizeof(gboolean));

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

PUBLIC void gen_kill_generator(Generator *g) {
  int i;

  if (g->klass->destroy_instance != NULL)
    g->klass->destroy_instance(g);

  gen_purge_event_queue_refs(g);

  empty_all_connections(g->klass->in_count, g->in_events, 0, 0);
  empty_all_connections(g->klass->out_count, g->out_events, 0, 1);
  empty_all_connections(g->klass->in_sig_count, g->in_signals, 1, 0);
  empty_all_connections(g->klass->out_sig_count, g->out_signals, 1, 1);

  for (i = 0; i < g->klass->out_sig_count; i++)
    if (g->last_buffers[i] != NULL)
      free(g->last_buffers[i]);

  if (g->controls != NULL) {
    GList *c = g->controls;
    g->controls = NULL;

    while (c != NULL) {
      GList *tmp = g_list_next(c);

      control_kill_control(c->data);
      g_list_free_1(c);
      c = tmp;
    }
  }

  free(g->name);
  free(g->in_events);
  free(g->out_events);
  free(g->in_signals);
  free(g->out_signals);
  free(g->last_buffers);
  free(g->last_buflens);
  free(g->last_results);
  free(g);
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

PUBLIC Generator *gen_unpickle(ObjectStoreItem *item) {
  Generator *g = objectstore_get_object(item);
  GeneratorClass *k;

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
	free(g);
	return NULL;
      }
      g->klass = k;
    }

    g->name = safe_string_dup(objectstore_item_get_string(item, "name", "anonym"));
    g->in_events = make_event_list(k->in_count);
    g->out_events = make_event_list(k->out_count);
    g->in_signals = make_event_list(k->in_sig_count);
    g->out_signals = make_event_list(k->out_sig_count);
    unpickle_eventlink_list_array(objectstore_item_get(item, "out_events"), item->db);
    unpickle_eventlink_list_array(objectstore_item_get(item, "out_signals"), item->db);

    g->last_sampletime = 0;
    g->last_buffers = safe_calloc(k->out_sig_count, sizeof(SAMPLE *));
    g->last_buflens = safe_calloc(k->out_sig_count, sizeof(int));
    g->last_results = safe_calloc(k->out_sig_count, sizeof(gboolean));

    g->controls = NULL;
    g->data = NULL;

    if (g->klass->unpickle_instance != NULL)
      g->klass->unpickle_instance(g, item, item->db);

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

PUBLIC ObjectStoreItem *gen_pickle(Generator *g, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_get_item(db, g);

  if (item == NULL) {
    item = objectstore_new_item(db, "Generator", g);
    objectstore_item_set_string(item, "class_name", g->klass->name);
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

PUBLIC EventLink *gen_link(int is_signal, Generator *src, gint32 src_q, Generator *dst, gint32 dst_q) {
  EventLink *el = gen_find_link(is_signal, src, src_q, dst, dst_q);
  GList **outq;
  GList **inq;

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

  outq = is_signal ? src->out_signals : src->out_events;
  inq = is_signal ? dst->in_signals : dst->in_events;

  outq[src_q] = g_list_prepend(outq[src_q], el);
  inq[dst_q] = g_list_prepend(inq[dst_q], el);

  return el;
}

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

PUBLIC void gen_unlink(EventLink *el) {
  GList **outq;
  GList **inq;

  g_return_if_fail(el != NULL);

  outq = (el->is_signal ? el->src->out_signals : el->src->out_events);
  inq = (el->is_signal ? el->dst->in_signals : el->dst->in_events);

  outq[el->src_q] = g_list_remove(outq[el->src_q], el);
  inq[el->dst_q] = g_list_remove(inq[el->dst_q], el);

  free(el);
}

PUBLIC void gen_register_control(Generator *g, Control *c) {
  g->controls = g_list_prepend(g->controls, c);
}

PUBLIC void gen_deregister_control(Generator *g, Control *c) {
  g->controls = g_list_remove(g->controls, c);
}

PUBLIC void gen_update_controls(Generator *g, int index) {
  GList *cs = g->controls;
  ControlDescriptor *desc = (index == -1) ? NULL : &g->klass->controls[index];

  while (cs != NULL) {
    Control *c = cs->data;

    if (desc == NULL || c->desc == desc) {
      c->events_flow = FALSE;	/* as already stated... not very elegant. */
      control_update_value(c);
      c->events_flow = TRUE;
    }

    cs = g_list_next(cs);
  }
}

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

PUBLIC gboolean gen_read_realtime_output(Generator *g, gint index, SAMPLE *buffer, int buflen) {
  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->out_sig_count && index >= 0, FALSE);
  g_return_val_if_fail((g->klass->out_sigs[index].flags & SIG_FLAG_REALTIME) != 0, FALSE);

  if (g_list_next(g->out_signals[index]) == NULL)
    /* Don't bother caching the only output. */
    return g->klass->out_sigs[index].d.realtime(g, buffer, buflen);
  else {
    /* Cache for multiple outputs... you never know who'll be reading you, or how often */
    if (g->last_buffers[index] == NULL || g->last_sampletime < gen_get_sampletime()) {
      /* Cache is not present, or expired */
      if (g->last_buffers[index] != NULL)
	free(g->last_buffers[index]);
      g->last_buffers[index] = malloc(sizeof(SAMPLE) * buflen);
      g->last_buflens[index] = buflen;
      g->last_sampletime = gen_get_sampletime();
      g->last_results[index] =
	g->klass->out_sigs[index].d.realtime(g, g->last_buffers[index], buflen);
    } else if (g->last_buflens[index] < buflen) {
      /* A small chunk was read. Fill it out. */
      SAMPLE *newbuf = malloc(sizeof(SAMPLE) * buflen);
      int oldlen = g->last_buflens[index];

      if (g->last_results[index])
	memcpy(newbuf, g->last_buffers[index], sizeof(SAMPLE) * g->last_buflens[index]);
      else
	memset(newbuf, 0, sizeof(SAMPLE) * g->last_buflens[index]);
      free(g->last_buffers[index]);
      g->last_buffers[index] = newbuf;
      g->last_buflens[index] = buflen;
      g->last_results[index] = 
	g->klass->out_sigs[index].d.realtime(g, &g->last_buffers[index][oldlen], buflen - oldlen);
    }

    if (g->last_results[index])
      memcpy(buffer, g->last_buffers[index], buflen * sizeof(SAMPLE));
    return g->last_results[index];
  }
}

PUBLIC SAMPLETIME gen_get_randomaccess_output_range(Generator *g, gint index) {
  SAMPLETIME (*fn)(Generator *, OutputSignalDescriptor *);

  /* Sanity checks */
  g_return_val_if_fail(index < g->klass->out_sig_count || index >= 0, 0);
  g_return_val_if_fail((g->klass->out_sigs[index].flags & SIG_FLAG_RANDOMACCESS) != 0, 0);

  fn = g->klass->out_sigs[index].d.randomaccess.get_range;

  if (fn)
    return fn(g, &g->klass->out_sigs[index]);
  else {
    g_warning("Generator %s does not implement get_range", g->klass->name);
    return 0;
  }
}

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

PRIVATE void send_one_event(EventLink *el, AEvent *e) {
  e->dst = el->dst;
  e->dst_q = el->dst_q;
  gen_post_aevent(e);
}

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

PUBLIC void init_generator(void) {
  generatorclasses = g_hash_table_new(g_str_hash, g_str_equal);
}

PUBLIC void done_generator(void) {
  g_hash_table_destroy(generatorclasses);
  generatorclasses = NULL;
}
