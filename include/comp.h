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

typedef enum ComponentColors {
  COMP_COLOR_BLACK = 0,
  COMP_COLOR_WHITE,
  COMP_COLOR_RED,
  COMP_COLOR_GREEN,
  COMP_COLOR_BLUE,
  COMP_COLOR_YELLOW,

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

struct ConnectorReference {
  Component *c;
  ConnectorKind kind;
  gboolean is_output;
  gint queue_number;
};

struct Connector {
  ConnectorReference ref;	/* this connector */
  GList *refs;			/* list of other-end ConnectorReferences */
  gint x, y;			/* location of connector relative to owning component */
};

struct ComponentClass {
  char *class_tag;		/* for save/load */

  int (*initialize_instance)(Component *c, gpointer init_data);
  void (*destroy_instance)(Component *c);

  void (*unpickle_instance)(Component *c, ObjectStoreItem *item, ObjectStore *db);
  void (*pickle_instance)(Component *c, ObjectStoreItem *item, ObjectStore *db);

  void (*paint)(Component *c, GdkRectangle *area,
		GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);

  int (*find_connector_at)(Component *c, gint x, gint y, ConnectorReference *ref);
  int (*contains_point)(Component *c, gint x, gint y);

  gboolean (*accept_outbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  gboolean (*accept_inbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  void (*unlink_outbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);
  void (*unlink_inbound)(Component *c, ConnectorReference *src, ConnectorReference *dst);

  /* These both return freshly-malloced strings */
  char *(*get_title)(Component *c);
  char *(*get_connector_name)(Component *c, ConnectorReference *ref);

  GtkWidget *(*build_popup)(Component *c);	/* returns a referenced widget */
};

struct Component {
  ComponentClass *klass;

  gint x, y, width, height;
  GList *connectors;

  void *data;
};

/*=======================================================================*/
/* 'New' menu management and ComponentClass registry */
extern void comp_add_newmenu_item(char *menupath, ComponentClass *k, gpointer init_data);
extern GtkWidget *comp_get_newmenu(void);

extern void comp_register_componentclass(ComponentClass *k);

/*=======================================================================*/
/* Component creation/deletion */
extern Component *comp_new_component(ComponentClass *k, gpointer init_data,
				     gint x, gint y);
extern void comp_kill_component(Component *c);

extern Component *comp_unpickle(ObjectStoreItem *item);
extern ObjectStoreItem *comp_pickle(Component *c, ObjectStore *db);

/*=======================================================================*/
/* Methods on Components */
extern void comp_paint_connections(Component *c, GdkRectangle *area,
				   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);
extern void comp_paint(Component *c, GdkRectangle *area,
		       GdkDrawable *drawable, GtkStyle *style, GdkColor *colors);

extern int comp_find_connector(Component *c, gint x, gint y, ConnectorReference *ref);
extern int comp_contains_point(Component *c, gint x, gint y);

extern void comp_link(ConnectorReference *a, ConnectorReference *b);
extern void comp_unlink(ConnectorReference *a, ConnectorReference *b);

extern char *comp_get_title(Component *c);
extern char *comp_get_connector_name(ConnectorReference *ref);

/*=======================================================================*/
/* Individual component popup-menu management */
extern void comp_append_popup(Component *c, GtkWidget *menu);

/*=======================================================================*/
/* Functions dealing with Connectors */
extern Connector *comp_new_connector(Component *c, ConnectorKind kind,
				     gboolean is_output, gint queue_number,
				     gint x, gint y);
extern Connector *comp_get_connector(ConnectorReference *ref);
extern void comp_kill_connector(Connector *con);

extern void comp_insert_connection(Connector *con, ConnectorReference *other);
extern void comp_remove_connection(Connector *con, ConnectorReference *other);

/*=======================================================================*/
/* Module setup/shutdown */
extern void init_comp(void);
extern void done_comp(void);

#endif
