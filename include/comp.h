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

#ifndef Comp_H
#define Comp_H

struct sheet;

typedef enum ComponentColors {
  COMP_COLOR_BLACK = 0,
  COMP_COLOR_WHITE,
  COMP_COLOR_RED,
  COMP_COLOR_GREEN,
  COMP_COLOR_BLUE,
  COMP_COLOR_YELLOW,
  COMP_COLOR_VIOLET,
  COMP_COLOR_CYAN,

  COMP_NUM_COLORS
} ComponentColors;

typedef enum ConnectorKind {
  COMP_NO_CONNECTOR = 0,
  COMP_EVENT_CONNECTOR,
  COMP_SIGNAL_CONNECTOR,
  COMP_ANY_CONNECTOR,

  COMP_NUM_CONNECTORKINDS
} ConnectorKind;

typedef struct ConnectorReference ConnectorReference;
typedef struct Connector Connector;
typedef struct ComponentClass ComponentClass;
typedef struct Component Component;

/**
 * \brief This is a reference to a Connector
 *
 * This is a reference to a connector. This uniquely identifies the connector.
 * Every connector consists of a ConnectorReference for itself and a List of ConnectorReferences
 * it is connected to.
 */

struct ConnectorReference {
  Component *c;  /**< The Component */
  ConnectorKind kind;
  gboolean is_output;
  gint queue_number;
};

/**
 * \brief The connector.
 *
 * The connector contains a reference on itself, a list of ConnectorReferences it is connected to, and
 * the x,y position relative to the component it is part of.
 */

struct Connector {
  ConnectorReference ref;	/* this connector */
  GList *refs;			/* list of other-end ConnectorReferences */
  gint x, y;			/* location of connector relative to owning component */
};

struct ComponentClass {
  char *class_tag;		/* for save/load */

  int (*initialize_instance)(Component *c, gpointer init_data);
  void (*destroy_instance)(Component *c);
  Component * (*clone_instance)(Component *c, struct sheet *sheet );

  void (*unpickle_instance)(Component *c, ObjectStoreItem *item, ObjectStore *db);
  void (*pickle_instance)(Component *c, ObjectStoreItem *item, ObjectStore *db);

  void (*paint)(Component *c, GdkRectangle *area,
		GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);

  int (*find_connector_at)(Component *c, gint x, gint y, ConnectorReference *ref);
  int (*contains_point)(Component *c, gint x, gint y);

  gboolean (*accept_outbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  gboolean (*accept_inbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  gboolean (*unlink_outbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  gboolean (*unlink_inbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);

  /* These both return freshly-malloced strings */
  char *(*get_title)(Component *c);
  char *(*get_connector_name)(Component *c, ConnectorReference *ref);

  GtkWidget *(*build_popup)(Component *c);	/* returns a referenced widget */
};

struct Component {
  ComponentClass *klass;

  struct sheet *sheet; 
  gint x, y, width, height;
  gint saved_x, saved_y;
  GList *connectors;

  void *data;
};

/*=======================================================================*/
/* 'New' menu management and ComponentClass registry */
extern void comp_add_newmenu_item(char *menupath, ComponentClass *k, gpointer init_data);
extern void comp_create_action( char *menuitem, ComponentClass *k, gpointer init_data, char *name, char *label );
extern GtkWidget *comp_get_newmenu(struct sheet *sheet);

extern void comp_register_componentclass(ComponentClass *k);

/*=======================================================================*/
/* Component creation/deletion */
extern Component *comp_new_component(ComponentClass *k, gpointer init_data,
				     struct sheet *sheet, gint x, gint y);
extern gboolean comp_kill_component(Component *c);

extern Component *comp_unpickle(ObjectStoreItem *item);
extern ObjectStoreItem *comp_pickle(Component *c, ObjectStore *db);
extern ConnectorReference *unpickle_connectorreference(ConnectorReference *ref,
							ObjectStoreItem *item);
extern gpointer unpickler_for_connectorreference(ObjectStoreItem *item);
extern ObjectStoreItem *pickle_connectorreference(ConnectorReference *ref, ObjectStore *db);

// Clone component on sheet
extern Component *comp_clone( Component *c, struct sheet *sheet );

/*=======================================================================*/
/* Methods on Components */
extern void comp_paint_connections(Component *c, GdkRectangle *area,
				   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);
extern void comp_paint(Component *c, GdkRectangle *area,
		       GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);

extern int comp_find_connector(Component *c, gint x, gint y, ConnectorReference *ref);
extern int comp_contains_point(Component *c, gint x, gint y);

extern void comp_link(ConnectorReference *a, ConnectorReference *b);
extern gboolean comp_unlink(ConnectorReference *a, ConnectorReference *b);

extern char *comp_get_title(Component *c);
extern char *comp_get_connector_name(ConnectorReference *ref);

/*=======================================================================*/
/* Individual component popup-menu management */
extern void comp_append_popup(Component *c, GtkWidget *menu);
extern GtkWidget *comp_get_popup(Component *c);

/*=======================================================================*/
/* Functions dealing with Connectors */
extern Connector *comp_new_connector(Component *c, ConnectorKind kind,
				     gboolean is_output, gint queue_number,
				     gint x, gint y);
extern Connector *comp_get_connector(ConnectorReference *ref);
extern void comp_kill_connector(Connector *con);

extern void comp_insert_connection(Connector *con, ConnectorReference *other);
extern void comp_remove_connection(Connector *con, ConnectorReference *other);
extern gint connectorreference_equal(ConnectorReference *r1, ConnectorReference *r2);

// Clone a list of components and also their connections
extern void comp_clone_list( GList *lst, struct sheet *sheet );
/*=======================================================================*/
/* Module setup/shutdown */
extern void init_comp(void);
extern void done_comp(void);

#endif
