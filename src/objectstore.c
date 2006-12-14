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
#include <locale.h>
#include <ctype.h>

#include <glib.h>

#include "global.h"
#include "objectstore.h"
#include "buffer.h"

PUBLIC void init_objectstore(void) {
}

PUBLIC void done_objectstore(void) {
}

/**
 * \brief allocate a new ObjectStore
 *
 * \return A new ObjectStore
 */

PUBLIC ObjectStore *objectstore_new_objectstore(void) {
  ObjectStore *db = safe_malloc(sizeof(ObjectStore));

  db->object_table = g_hash_table_new(g_direct_hash, g_direct_equal);
  db->key_table = g_hash_table_new(g_direct_hash, g_direct_equal);
  db->nextkey = 1;
  db->rootkey = 0;	/* 0 is the null key */

  return db;
}

PRIVATE void objectstore_kill_objectstoredatum(ObjectStoreDatum *datum) {
  switch (datum->kind) {
    case OSI_KIND_STRING:
      free(datum->d.string);
      break;

    case OSI_KIND_ARRAY: {
      int i;

      for (i = 0; i < datum->d.array.count; i++)
	objectstore_kill_objectstoredatum(datum->d.array.elts[i]);

      free(datum->d.array.elts);
      break;
    }

    case OSI_KIND_BINARY:
      free(datum->d.binary.data);
      break;

    default:
      break;
  }

  free(datum);
}

PRIVATE void objectstore_kill_objectstoreitem_field(gpointer key, gpointer value,
						    gpointer user_data) {
  ObjectStoreItemField *field = value;

  free(field->name);
  objectstore_kill_objectstoredatum(field->value);
  free(field);
}

PRIVATE void objectstore_kill_objectstoreitem(gpointer key, gpointer value, gpointer user_data) {
  ObjectStoreItem *item = (ObjectStoreItem *) value;

  free(item->tag);
  g_hash_table_foreach(item->fields, objectstore_kill_objectstoreitem_field, NULL);
  g_hash_table_destroy(item->fields);
  free(item);
}

/**
 * \brief Free the Memory allocated by the ObjectStore
 *
 * \param db The ObjectStore to be freed.
 */

PUBLIC void objectstore_kill_objectstore(ObjectStore *db) {
  g_hash_table_foreach(db->object_table, objectstore_kill_objectstoreitem, NULL);
  g_hash_table_destroy(db->object_table);
  g_hash_table_destroy(db->key_table);
  free(db);
}

PRIVATE void objectstore_write_objectstoredatum(FILE *f, ObjectStoreDatum *datum) {
  switch (datum->kind) {
    case OSI_KIND_INT:
      fprintf(f, "i%d", datum->d.integer);
      break;

    case OSI_KIND_DOUBLE:
      fprintf(f, "d%g", datum->d.number);
      break;

    case OSI_KIND_STRING:
      fprintf(f, "s%d:%s:", strlen(datum->d.string), datum->d.string);
      break;

    case OSI_KIND_OBJECT:
      fprintf(f, "o%d", datum->d.object_key);
      break;

    case OSI_KIND_ARRAY: {
      int i;

      fprintf(f, "a%d:", datum->d.array.count);
      for (i = 0; i < datum->d.array.count; i++) {
	objectstore_write_objectstoredatum(f, datum->d.array.elts[i]);
	fprintf(f, ":");
      }
      break;
    }

    case OSI_KIND_BINARY:
      fprintf(f, "b%d:", datum->d.binary.length);
      fwrite(datum->d.binary.data, 1, datum->d.binary.length, f);
      break;

    default:
      break;
  }
}

PRIVATE void objectstore_write_objectstoreitem_field(gpointer key, gpointer value,
						     gpointer user_data) {
  FILE *f = (FILE *) user_data;
  ObjectStoreItemField *field = (ObjectStoreItemField *) value;

  fprintf(f, "  %s = ", field->name);
  objectstore_write_objectstoredatum(f, field->value);
  fprintf(f, "\n");
}

PRIVATE void objectstore_write_objectstoreitem(gpointer key_p, gpointer value,
					       gpointer user_data) {
  FILE *f = (FILE *) user_data;
  ObjectStoreItem *item = (ObjectStoreItem *) value;

  fprintf(f, "%s %d [\n", item->tag, item->key);
  g_hash_table_foreach(item->fields, objectstore_write_objectstoreitem_field, f);
  fprintf(f, "]\n\n");
}

/**
 * \brief Write out an ObjectStore
 *
 * \param f FILE * where the ObjectStore should be written to.
 * \param db the ObjectStore
 *
 * \return TRUE on success.
 */

PUBLIC gboolean objectstore_write(FILE *f, ObjectStore *db) {

  int i;

  setlocale( LC_ALL, "C" );
  fprintf(f,
	  "Mjik\n"
	  "ObjectStore 0 [\n"
	  "  version = i%d\n"
	  "  rootkey = i%d\n"
	  "]\n\n",
	  OBJECTSTORE_CURRENT_VERSION,
	  db->rootkey);

  for (i = 1; i < db->nextkey; i++)
    objectstore_write_objectstoreitem((gpointer) i,
				      g_hash_table_lookup(db->object_table, (gconstpointer) i),
				      (gpointer) f);
  setlocale( LC_ALL, "" );
  return TRUE;
}

PRIVATE ObjectStoreDatum *objectstore_datum_new_object_key(ObjectStoreKey key);	/* forward */

PRIVATE ObjectStoreDatum *read_item_field_value(FILE *f) {
  int ch = fgetc(f);

  switch (ch) {
    case 'i': {
      int val;
      fscanf(f, "%d", &val);
      return objectstore_datum_new_integer(val);
    }

    case 'd': {
      gdouble val;
      fscanf(f, "%lg", &val);
      return objectstore_datum_new_double(val);
    }

    case 's': {
      int len;
      char *str;
      ObjectStoreDatum *result;

      fscanf(f, "%d", &len);
      str = safe_malloc(len + 1);
      fgetc(f);	/* skip ':' */
      fread(str, sizeof(char), len, f);
      str[len] = '\0';
      fgetc(f);	/* skip ':' */

      result = objectstore_datum_new_string(str);
      free(str);
      return result;
    }

    case 'o': {
      int key;
      fscanf(f, "%d", &key);
      return objectstore_datum_new_object_key(key);
    }

    case 'a': {
      int len;
      int i;
      ObjectStoreDatum *result;

      fscanf(f, "%d", &len);
      result = objectstore_datum_new_array(len);
      fgetc(f);	/* skip ':' */
      for (i = 0; i < len; i++) {
	objectstore_datum_array_set(result, i, read_item_field_value(f));
	fgetc(f);	/* skip separating ':' */
      }

      return result;
    }

    case 'b': {
      int len;
      char *buf;
      ObjectStoreDatum *result;

      fscanf(f, "%d", &len);
      buf = safe_malloc(len);
      fgetc(f);	/* skip ':' */
      fread(buf, sizeof(char), len, f);
      /* no trailing ':', bit pointless really */

      result = objectstore_datum_new_binary(len, buf);
      free(buf);
      return result;
    }

    default:
      g_warning("Unknown ObjectStoreDatum tag character %c in read_item_field_value!",
		ch);
      return NULL;
  }
}

PRIVATE ObjectStoreItem *read_item(FILE *f) {
  ObjectStoreItem *item = safe_malloc(sizeof(ObjectStoreItem));
  BUFFER varname;
  char str[1024];	/* %%% Yuck, a fixed-size buffer. */
  int key;

  if (fscanf(f, "%s %d [", str, &key) < 2) {
    free(item);
    return NULL;
  }

  item->tag = safe_string_dup(str);
  item->key = key;
  item->object = NULL;
  item->db = NULL;
  item->fields = g_hash_table_new(g_str_hash, g_str_equal);

  varname = newbuf(128);

  while (!feof(f)) {
    int ch;

    /* Strip leading whitespace (there _should_ always be some). */
    do { ch = fgetc(f); } while (isspace(ch) && (ch != ']') && (ch != EOF));

    if (ch == ']' || ch == EOF) {
      /* That's it. We're done with this item. */
      /* Empty line signals no more fields. */
      break;
    }

    /* Read the field name */
    do {
      buf_append(varname, ch);
      ch = fgetc(f);
    } while (!isspace(ch));

    /* Skip the whitespace and equals-sign */
    do { ch = fgetc(f); } while (isspace(ch) || ch == '=');
    ungetc(ch, f);

    /* Now we have the complete variable name in varname, and we are about to read the
       first character of the variable value. */
    buf_append(varname, '\0');
    objectstore_item_set(item, varname->buf, read_item_field_value(f));
    varname->pos = 0;	/* reset varname for next round */

    /* Trailing whitespace, if any, is dealt with at the top of the loop. */
  }

  killbuf(varname);
  return item;
}

/**
 * \brief Read ObjectStore from a FILE
 *
 * \param f FILE
 * \param db the empty ObjectStore.
 *
 * \return TRUE if the FILE could be read in.
 */

PUBLIC gboolean objectstore_read(FILE *f, ObjectStore *db) {
    ObjectStoreItem *item;
    ObjectStoreDatum *datum;
    char magic[5];

    setlocale( LC_NUMERIC, "C" );


    fread(magic, sizeof(char), 4, f);
    magic[4] = '\0';
    if (strcmp(magic, "Mjik")) {
	setlocale( LC_NUMERIC, "" );
	return FALSE;
    }

    item = read_item(f);

    if (strcmp(item->tag, "ObjectStore") ||
	    item->key != 0) {
	objectstore_kill_objectstoreitem(NULL, item, NULL);
	setlocale( LC_NUMERIC, "" );
	return FALSE;
    }

    datum = objectstore_item_get(item, "version");
    if (datum == NULL ||
	    datum->kind != OSI_KIND_INT ||
	    datum->d.integer != OBJECTSTORE_CURRENT_VERSION) {
	objectstore_kill_objectstoreitem(NULL, item, NULL);
	setlocale( LC_NUMERIC, "" );
	return FALSE;
    }

    datum = objectstore_item_get(item, "rootkey");
    if (datum == NULL ||
	    datum->kind != OSI_KIND_INT) {
	setlocale( LC_NUMERIC, "");
	return FALSE;
    }
    db->rootkey = datum->d.integer;

    objectstore_kill_objectstoreitem(NULL, item, NULL);

    while (!feof(f)) {
	item = read_item(f);
	if (item != NULL) {
	    g_hash_table_insert(db->object_table, (gpointer) item->key, item);
	    item->db = db;
	    db->nextkey = MAX(db->nextkey, item->key + 1);
	}
    }

    setlocale( LC_NUMERIC, "" );
    return TRUE;
}


/**
 * \brief Set the Root ObjectStoreItem.
 *
 * \param db ObjectStore
 * \param root The new root ObjectStoreItem
 */

PUBLIC void objectstore_set_root(ObjectStore *db, ObjectStoreItem *root) {
  db->rootkey = root->key;
}


/**
 * \brief Get the Root Item
 *
 * \param db The ObjectStore
 *
 * \return The ObjectStoreItem which was set with objectstore_set_root().
 */

PUBLIC ObjectStoreItem *objectstore_get_root(ObjectStore *db) {
  return g_hash_table_lookup(db->object_table, (gconstpointer) db->rootkey);
}


/**
 * \brief Get the ObjectStoreItem representing the object at a given pointer
 *
 * \param db ObjectStore
 * \param object A pointer to a Structure which is represented in the ObjectStore.
 * 
 * \return The ObjectStoreItem or NULL if the pointer is not known.
 */

PUBLIC ObjectStoreItem *objectstore_get_item(ObjectStore *db, gpointer object) {
  return g_hash_table_lookup(db->object_table,
			     g_hash_table_lookup(db->key_table, object));
}

/**
 * \brief Get ObjectStoreItem by key
 *
 * \param db the ObjectStore
 * \param key the ObjectStoreKey for the ObjectStoreItem we want to be returned.
 *
 * \return The ObjectStoreItem identified by \a key
 */

PUBLIC ObjectStoreItem *objectstore_get_item_by_key(ObjectStore *db, ObjectStoreKey key) {
  return g_hash_table_lookup(db->object_table, (gpointer) key);
}

/**
 * \brief Create a new ObjectStoreItem
 *
 * \param db the ObjectStore
 * \param tag a string describing the type of the ObjectStoreItem
 * \param object a pointer to the structure we want to put into the ObjectStoreItem.
 *               Because the pointer is known in the ObjectStore we can find out if we
 *               already have a structure in the object store and only have a reference on that.
 */

PUBLIC ObjectStoreItem *objectstore_new_item(ObjectStore *db, char *tag, gpointer object) {
  ObjectStoreItem *item = safe_malloc(sizeof(ObjectStoreItem));

  item->tag = safe_string_dup(tag);
  item->key = db->nextkey++;
  item->object = object;
  item->db = db;
  item->fields = g_hash_table_new(g_str_hash, g_str_equal);

  g_hash_table_insert(db->object_table, (gpointer) item->key, item);
  g_hash_table_insert(db->key_table, object, (gpointer) item->key);

  return item;
}

/**
 * \brief Get the pointer associated with an ObjectStoreItem.
 *
 * \param item an ObjectStoreItem.
 *
 * \return The pointer to the data or NULL if the item is not valid.
 */

PUBLIC gpointer objectstore_get_object(ObjectStoreItem *item) {
    if( item )
	return item->object;
    else
	return NULL;
}

/**
 * \brief set the pointer of an ObjectStoreItem
 *
 * \param item The ObjectStoreItem we want to setup.
 * \param object the pointer.
 */

PUBLIC void objectstore_set_object(ObjectStoreItem *item, gpointer object) {

    if( object == NULL ) {

	if (item->object != NULL) {
	    g_hash_table_remove(item->db->key_table, item->object);
	    item->object = NULL;
	}

	return;
    }

    if (item->object != NULL) {
	g_warning("item->object != NULL in objectstore_set_object");
	g_hash_table_remove(item->db->key_table, item->object);
	item->object = NULL;
    }

    item->object = object;
    g_hash_table_insert(item->db->key_table, item->object, (gpointer) item->key);
}

/**
 * \brief get an integer value from an ObjectStoreItem
 *
 * \param item The ObjectStoreItem
 * \param name The Name of the value we want to read out
 * \param deft default value.
 *
 * \return The gint32 associated with \a name on \a item, or \a deft if \a item does not have
 *         a value named \a name.
 */

PUBLIC gint32 objectstore_item_get_integer(ObjectStoreItem *item, char *name, gint32 deft) {
  ObjectStoreDatum *datum = objectstore_item_get(item, name);

  if (datum == NULL)
    return deft;

  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_INT, deft);
  return datum->d.integer;
}

/**
 * \brief get a double value from an ObjectStoreItem
 *
 * \param item The ObjectStoreItem
 * \param name The Name of the value we want to read out
 * \param deft default value.
 *
 * \return The double associated with \a name on \a item, or \a deft if \a item does not have
 *         a value named \a name.
 */

PUBLIC gdouble objectstore_item_get_double(ObjectStoreItem *item, char *name, gdouble deft) {
  ObjectStoreDatum *datum = objectstore_item_get(item, name);

  if (datum == NULL)
    return deft;

  return objectstore_datum_double_value(datum);
}

/**
 * \brief get a string value from an ObjectStoreItem
 *
 * \param item The ObjectStoreItem
 * \param name The Name of the value we want to read out
 * \param deft default value.
 *
 * \return The string associated with \a name on \a item, or \a deft if \a item does not have
 *         a value named \a name.
 */

PUBLIC char *objectstore_item_get_string(ObjectStoreItem *item, char *name, char *deft) {
  ObjectStoreDatum *datum = objectstore_item_get(item, name);

  if (datum == NULL)
    return deft;

  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_STRING, deft);
  return datum->d.string;
}

/**
 * \brief get an Object from an ObjectStoreItem
 *
 * \param item The ObjectStoreItem
 * \param name The Name of the value we want to read out
 *
 * \return The ObjectStoreItem associated with \a name on \a item.
 */

PUBLIC ObjectStoreItem *objectstore_item_get_object(ObjectStoreItem *item, char *name) {
  ObjectStoreDatum *datum = objectstore_item_get(item, name);

  if (datum == NULL)
    return NULL;

  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_OBJECT, NULL);
  return g_hash_table_lookup(item->db->object_table, (gconstpointer) datum->d.object_key);
}

/**
 * \brief get binary data from ObjectStoreItem
 *
 * \param item The ObjectStoreItem
 * \param name The name of the value we want to read out.
 *
 * \retval dataptr a void * to the data.
 * \return the length of the data or -1 on failure.
 */

PUBLIC gint32 objectstore_item_get_binary(ObjectStoreItem *item, char *name, void **dataptr) {
  ObjectStoreDatum *datum = objectstore_item_get(item, name);

  if (datum == NULL)
    return -1;

  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_BINARY, -1);
  *dataptr = datum->d.binary.data;
  return datum->d.binary.length;
}

/**
 * \brief get an ObjectStoreDatum from an ObjectStoreItem.
 *
 * \param item the ObjectStoreItem.
 * \param name the name of the ObjectStoreDatum we want to get.
 *
 * \return an ObjectStoreDatum or NULL on failure.
 */

PUBLIC ObjectStoreDatum *objectstore_item_get(ObjectStoreItem *item, char *name) {
  ObjectStoreItemField *field;
  if( item == NULL )
      return NULL;

  field = g_hash_table_lookup(item->fields, name);

  return field ? field->value : NULL;
}

/**
 * \brief set that \a name on \a item is \a value.
 *
 * \param item ObjectStoreItem to set.
 * \param name the name of the ObjectStoreDatum
 * \param value ObjectStoreDatum.
 */

PUBLIC void objectstore_item_set(ObjectStoreItem *item, char *name, ObjectStoreDatum *value) {
  ObjectStoreItemField *field;

  field = g_hash_table_lookup(item->fields, name);

  if (field != NULL) {
    if (field->value != NULL)
      objectstore_kill_objectstoredatum(field->value);
    field->value = value;
    return;
  }

  field = safe_malloc(sizeof(ObjectStoreItemField));

  field->name = safe_string_dup(name);
  field->value = value;
  g_hash_table_insert(item->fields, field->name, field);
}

PRIVATE ObjectStoreDatum *objectstore_datum_new(enum ObjectStoreDatumKind kind) {
  ObjectStoreDatum *datum = safe_malloc(sizeof(ObjectStoreDatum));

  datum->kind = kind;
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum from an integer.
 *
 * \param value The integer.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_INT
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_integer(gint32 value) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_INT);
  datum->d.integer = value;
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum from a double.
 *
 * \param value The double.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_DOUBLE
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_double(gdouble value) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_DOUBLE);
  datum->d.number = value;
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum from an string.
 *
 * \param value The string.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_STRING
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_string(char *value) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_STRING);
  datum->d.string = safe_string_dup(value);
  return datum;
}

PRIVATE ObjectStoreDatum *objectstore_datum_new_object_key(ObjectStoreKey key) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_OBJECT);
  datum->d.object_key = key;
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum from an ObjectStoreItem.
 *
 * \param value The ObjectStoreItem.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_OBJECT
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_object(ObjectStoreItem *value) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_OBJECT);
  datum->d.object_key = value->key;
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum of type Array.
 *
 * \param length The Length of the new array.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_ARRAY
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_array(int length) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_ARRAY);
  datum->d.array.count = length;
  datum->d.array.elts = calloc(length, sizeof(ObjectStoreDatum *));
  return datum;
}

/**
 * \brief Create an ObjectStoreDatum of type Binary.
 *
 * \param length The Size of the data in bytes.
 * \param data Pointer to the binary data.
 * 
 * \return New ObjectStoreDatum of Type OSI_KIND_BINARY
 */

PUBLIC ObjectStoreDatum *objectstore_datum_new_binary(int length, void *data) {
  ObjectStoreDatum *datum = objectstore_datum_new(OSI_KIND_BINARY);
  datum->d.binary.length = length;
  datum->d.binary.data = malloc(length);
  memcpy(datum->d.binary.data, data, length);
  return datum;
}

/**
 * \brief get the Integer from an ObjectStoreDatum
 *
 * \param datum The ObjectStoreDatum
 *
 * \return the Integer or 0 on failure.
 */

PUBLIC gint32 objectstore_datum_integer_value(ObjectStoreDatum *datum) {
  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_INT, 0);
  return datum->d.integer;
}

/**
 * \brief get the Double from an ObjectStoreDatum
 *
 * \param datum The ObjectStoreDatum
 *
 * \return the double or 0 on failure.
 */

PUBLIC gdouble objectstore_datum_double_value(ObjectStoreDatum *datum) {
  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_DOUBLE || datum->kind == OSI_KIND_INT, 0);
  return (datum->kind == OSI_KIND_DOUBLE) ? datum->d.number : datum->d.integer;
}




/**
 * \brief get the string from an ObjectStoreDatum
 *
 * \param datum The ObjectStoreDatum
 *
 * \return the string or NULL on failure. The string is in the Memory of the ObjectStore so if that is
 *         destroyed then the string will become invalid.
 */


PUBLIC char *objectstore_datum_string_value(ObjectStoreDatum *datum) {
  RETURN_VAL_UNLESS(datum->kind == OSI_KIND_STRING, NULL);
  return safe_string_dup( datum->d.string );
}

/**
 * \brief get the ObjectStoreKey from an ObjectStoreDatum of Type OSI_KIND_OBJECT
 *
 * \param obj The ObjectStoreDatum
 *
 * \return the key or 0 on failure.
 */

PUBLIC ObjectStoreKey objectstore_datum_object_key(ObjectStoreDatum *obj) {
  RETURN_VAL_UNLESS(obj->kind == OSI_KIND_OBJECT, 0);
  return obj->d.object_key;
}

/**
 * \brief Get the length of an ObjectStoreDatum of type Array
 *
 * \param array The ObjectStoreDatum of Type OSI_KIND_ARRAY
 *
 * \return the length or 0 on failure.
 */

PUBLIC int objectstore_datum_array_length(ObjectStoreDatum *array) {
  RETURN_VAL_UNLESS(array->kind == OSI_KIND_ARRAY, 0);
  return array->d.array.count;
}

/**
 * \brief Get the ObjectStoreDatum at \a index of an ObjectStoreDatum of type Array
 *
 * \param array The ObjectStoreDatum of Type OSI_KIND_ARRAY
 * \param index The index in the array.
 *
 * \return The ObjectStoreDatum or NULL on failure.
 */

PUBLIC ObjectStoreDatum *objectstore_datum_array_get(ObjectStoreDatum *array, int index) {
  RETURN_VAL_UNLESS(array, NULL);
  RETURN_VAL_UNLESS(array->kind == OSI_KIND_ARRAY, NULL);
  g_return_val_if_fail(index >= 0, NULL);
  g_return_val_if_fail(index < array->d.array.count, NULL);

  return array->d.array.elts[index];
}

/**
 * \brief Set the ObjectStoreDatum at \a index of an ObjectStoreDatum of type Array
 *
 * \param array The ObjectStoreDatum of Type OSI_KIND_ARRAY
 * \param index The index in the array.
 * \param value The ObjectStoreDatum which the array element will be set to.
 *
 * \return The ObjectStoreDatum or NULL on failure.
 */

PUBLIC void objectstore_datum_array_set(ObjectStoreDatum *array, int index,
					ObjectStoreDatum *value) {
  RETURN_UNLESS(array->kind == OSI_KIND_ARRAY);
  g_return_if_fail(index >= 0);
  g_return_if_fail(index < array->d.array.count);

  if (array->d.array.elts[index] != NULL)
    objectstore_kill_objectstoredatum(array->d.array.elts[index]);
  array->d.array.elts[index] = value;
}

/**
 * \brief extract the objects from an Array into a GList
 *
 * \param array ObjectStoreDatum of Type OSI_KIND_ARRAY
 * \param db the ObjectStore
 * \param unpickler a function which returns a pointer from an ObjectStoreItem
 *
 * \return GList of unpickled objects
 */

PUBLIC GList *objectstore_extract_list_of_items(ObjectStoreDatum *array, ObjectStore *db,
						gpointer (*unpickler)(ObjectStoreItem *item)) {
  int i, len;
  GList *result = NULL;

  RETURN_VAL_UNLESS(array != NULL, NULL);
  len = objectstore_datum_array_length(array);
  for (i = 0; i < len; i++) {
    ObjectStoreDatum *elt = objectstore_datum_array_get(array, i);
    ObjectStoreItem *item = objectstore_get_item_by_key(db, objectstore_datum_object_key(elt));
    result = g_list_append(result, unpickler(item));
  }

  return result;
}

/**
 * \brief create an ObjectStoreDatum of type Array from a GList of Objects.
 *
 * \param list A GList of Objects which can be pickled with \a pickler
 * \param db the ObjectStore
 * \param pickler a function of type objectstore_pickler_t.
 *
 * \return ObjectStoreDatum of type Array.
 */

PUBLIC ObjectStoreDatum *objectstore_create_list_of_items(GList *list, ObjectStore *db,
							  objectstore_pickler_t pickler) {
  int len = g_list_length(list);
  ObjectStoreDatum *result = objectstore_datum_new_array(len);
  int i;

  for (i = 0; i < len; i++, list = g_list_next(list))
    objectstore_datum_array_set(result, i, objectstore_datum_new_object(pickler(list->data, db)));

  return result;
}
