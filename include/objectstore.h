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

#ifndef ObjectStore_H
#define ObjectStore_H

#define OBJECTSTORE_CURRENT_VERSION 1

typedef struct ObjectStore ObjectStore;				/* entire document */
typedef struct ObjectStoreItem ObjectStoreItem;			/* record */
typedef struct ObjectStoreItemField ObjectStoreItemField;	/* field in record */
typedef struct ObjectStoreDatum ObjectStoreDatum;		/* datum in field or array */

typedef gint32 ObjectStoreKey;
typedef gpointer (*objectstore_unpickler_t)(ObjectStoreItem *item);
typedef ObjectStoreItem *(*objectstore_pickler_t)(gpointer data, ObjectStore *db);

typedef enum ObjectStoreDatumKind {
  OSI_KIND_INT = 0,
  OSI_KIND_DOUBLE,
  OSI_KIND_STRING,
  OSI_KIND_OBJECT,
  OSI_KIND_ARRAY,
  OSI_KIND_BINARY,

  OSI_MAX_KIND
} ObjectStoreItemKind;

struct ObjectStore {
  GHashTable *object_table;	/* key -> ObjectStoreItem */
  GHashTable *key_table;	/* object -> key */
  ObjectStoreKey nextkey;
  ObjectStoreKey rootkey;
};

struct ObjectStoreItem {
  char *tag;
  ObjectStoreKey key;
  gpointer object;
  ObjectStore *db;

  GHashTable *fields;
};

struct ObjectStoreItemField {
  char *name;
  ObjectStoreDatum *value;
};

struct ObjectStoreDatum {
  ObjectStoreItemKind kind;

  union {
    gint32 integer;
    gdouble number;
    char *string;
    ObjectStoreKey object_key;
    struct {
      int count;
      ObjectStoreDatum **elts;
    } array;
    struct {
      int length;
      void *data;
    } binary;
  } d;
};

/* Module setup/shutdown */
extern void init_objectstore(void);
extern void done_objectstore(void);

/* Methods on entire ObjectStores */
extern ObjectStore *objectstore_new_objectstore(void);
extern void objectstore_kill_objectstore(ObjectStore *db);
extern gboolean objectstore_write(FILE *f, ObjectStore *db);
extern gboolean objectstore_read(FILE *f, ObjectStore *db);
extern void objectstore_set_root(ObjectStore *db, ObjectStoreItem *root);
extern ObjectStoreItem *objectstore_get_root(ObjectStore *db);

/* Map between in-core objects and ObjectStoreItems */
extern ObjectStoreItem *objectstore_get_item(ObjectStore *db, gpointer object);
extern ObjectStoreItem *objectstore_get_item_by_key(ObjectStore *db, ObjectStoreKey key);
extern ObjectStoreItem *objectstore_new_item(ObjectStore *db, char *tag, gpointer object);
extern gpointer objectstore_get_object(ObjectStoreItem *item);
extern void objectstore_set_object(ObjectStoreItem *item, gpointer object);

/* Specific getter-methods on ObjectStoreItems */
extern gint32 objectstore_item_get_integer(ObjectStoreItem *item, char *name, gint32 deft);
extern gdouble objectstore_item_get_double(ObjectStoreItem *item, char *name, gdouble deft);
extern char *objectstore_item_get_string(ObjectStoreItem *item, char *name, char *deft);
extern ObjectStoreItem *objectstore_item_get_object(ObjectStoreItem *item, char *name);
extern gint32 objectstore_item_get_binary(ObjectStoreItem *item, char *name, void **dataptr);
/* Specific setter-methods on ObjectStoreItems */
#define objectstore_item_set_integer(item, name, value) \
	objectstore_item_set(item, name, objectstore_datum_new_integer(value))
#define objectstore_item_set_double(item, name, value) \
	objectstore_item_set(item, name, objectstore_datum_new_double(value))
#define objectstore_item_set_string(item, name, value) \
	objectstore_item_set(item, name, objectstore_datum_new_string(value))
#define objectstore_item_set_object(item, name, value) \
	objectstore_item_set(item, name, objectstore_datum_new_object(value))
/* General methods on ObjectStoreItems */
extern ObjectStoreDatum *objectstore_item_get(ObjectStoreItem *item, char *name);
extern void objectstore_item_set(ObjectStoreItem *item, char *name, ObjectStoreDatum *value);

/* Methods on ObjectStoreDatums */
extern ObjectStoreDatum *objectstore_datum_new_integer(gint32 value);
extern ObjectStoreDatum *objectstore_datum_new_double(gdouble value);
extern ObjectStoreDatum *objectstore_datum_new_string(char *value);
extern ObjectStoreDatum *objectstore_datum_new_object(ObjectStoreItem *value);
extern ObjectStoreDatum *objectstore_datum_new_array(int length);
extern ObjectStoreDatum *objectstore_datum_new_binary(int length, void *data);

/* Methods on specific kinds of ObjectStoreDatums */
extern gint32 objectstore_datum_integer_value(ObjectStoreDatum *datum);
extern gdouble objectstore_datum_double_value(ObjectStoreDatum *datum);
extern char *objectstore_datum_string_value(ObjectStoreDatum *datum); 
extern ObjectStoreKey objectstore_datum_object_key(ObjectStoreDatum *obj);
extern int objectstore_datum_array_length(ObjectStoreDatum *array);
extern ObjectStoreDatum *objectstore_datum_array_get(ObjectStoreDatum *array, int index);
extern void objectstore_datum_array_set(ObjectStoreDatum *array, int index,
					ObjectStoreDatum *value);

/* Special utility routines */
extern GList *objectstore_extract_list_of_items(ObjectStoreDatum *array, ObjectStore *db,
						objectstore_unpickler_t unpickler);
extern ObjectStoreDatum *objectstore_create_list_of_items(GList *list, ObjectStore *db,
							  objectstore_pickler_t pickler);

#endif
