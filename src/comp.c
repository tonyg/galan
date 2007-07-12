/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 * Copyright (C) 2001 Torben Hohn
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
#include "comp.h"
#include "gencomp.h"
#include "sheet.h"
#include "gui.h"
#include "msgbox.h"

#include "galan-compaction.h"
#include "galan-comptree-model.h"

#define NEWMENU_PATH_PREFIX	"/"

typedef struct MenuEntry {
  char *menupath;
  ComponentClass *k;
  gpointer init_data;
} MenuEntry;

PRIVATE GList *menuentries = NULL;
PRIVATE gboolean menuentries_dirty = TRUE;
PRIVATE GHashTable *componentclasses = NULL;
PRIVATE GtkItemFactory *menufact = NULL;

PRIVATE GalanCompTreeModel *tmodel = NULL;

PRIVATE void newmenu_callback(MenuEntry *p, guint callback_action, GtkWidget *widget) {

    GtkItemFactory *ifact = gtk_item_factory_from_widget( widget );
    Sheet *sheet = gtk_object_get_user_data( GTK_OBJECT( ifact ) );
    sheet_build_new_component(sheet, p->k, p->init_data);
}

PUBLIC void comp_add_newmenu_item(char *menupath, ComponentClass *k, gpointer init_data) {

  MenuEntry *m = safe_malloc(sizeof(MenuEntry));

  if (k->initialize_instance == NULL ||
      k->paint == NULL ||
      k->get_title == NULL) {
    g_warning("ComponentClass must have initialize_instance, paint and "
	      "get_title methods (menupath = %s)", menupath);
    free(m);
    return;
  }

  m->menupath = g_strdup_printf( "%s%s", NEWMENU_PATH_PREFIX, menupath );
  m->k = k;
  m->init_data = init_data;

  menuentries = g_list_append(menuentries, m);
  menuentries_dirty = TRUE;

}



PRIVATE void ensure_path_exists( char *mpath, char *base ) {
    
    if( !strcmp( mpath, base ) )
	return;

    GtkWidget *menuaction = gtk_ui_manager_get_widget( ui_manager, mpath );
    if( menuaction == NULL ) {
	char *updir = g_path_get_dirname( mpath );
	char *aname = g_path_get_basename( mpath );
	ensure_path_exists( updir, base );
	GtkAction *tmpact = gtk_action_new( aname, aname, NULL, NULL );
	gtk_action_group_add_action( component_actiongroup, tmpact );

	gtk_ui_manager_add_ui( ui_manager, 
		gtk_ui_manager_new_merge_id( ui_manager ),
		updir,
		aname,
		g_strdup(aname),
		GTK_UI_MANAGER_MENU,
		TRUE );

	//free( aname );
	//free( updir );
    }
}

PRIVATE void experiment( void ) {

    tmodel = g_object_new( GALAN_TYPE_COMPTREE_MODEL, NULL );
    
    GtkTreeView *tview = GTK_TREE_VIEW( gtk_tree_view_new_with_model( GTK_TREE_MODEL(tmodel) ) );
    GtkCellRenderer *render = GTK_CELL_RENDERER( gtk_cell_renderer_text_new () );
    gtk_tree_view_insert_column_with_attributes( tview, -1, "test", render, "text", 0, NULL );

static GtkTargetEntry targette = { "galan/CompAction", 0, 234 };
gtk_tree_view_enable_model_drag_dest( tview, &targette, 1, GDK_ACTION_COPY );
    

    GtkWindow *win = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ) );
    GtkContainer *scrollo = GTK_CONTAINER( gtk_scrolled_window_new( NULL, NULL ) );
    
  gtk_container_add (GTK_CONTAINER (win), GTK_WIDGET(scrollo ) );
  gtk_container_add (GTK_CONTAINER (scrollo), GTK_WIDGET(tview));
  gtk_widget_show_all( GTK_WIDGET(win) );

    
}

PUBLIC void comp_create_action( char *menuitem, ComponentClass *k, gpointer init_data, char *name, char *label ) 
{

    char *long_name = g_path_get_basename( menuitem );
    char *base = "/ui/MainMenu/AddComp";
    char *mpath = g_strdup_printf( "%s/%s", base, menuitem );

//    GtkAction *action = g_object_new( COMPACTION_TYPE, 
//	    "component-class", k, 
//	    "init-data", init_data, 
    GtkAction *action = g_object_new( GALAN_TYPE_COMPACTION, 
	    "klass", k, 
	    "init_data", init_data, 

	    "name", g_strdup(name),
	    "label", long_name,
	    "short-label", g_strdup(name),
	    "hide-if-empty", FALSE,

	    NULL );

    gtk_action_group_add_action( component_actiongroup, action );

    char *dir_path = g_path_get_dirname( mpath);
    ensure_path_exists( dir_path, base );
    
    //XXX: the half baked tree model
    GtkTreeIter iter;
  galan_comptree_model_lookup( tmodel, menuitem, &iter, TRUE );
  gtk_tree_store_set( GTK_TREE_STORE(tmodel), &iter, 1, TRUE, 2, action, -1 );
    //XXX:

    //printf( "name  = %s\n", gtk_action_get_name( action ) );
    gtk_ui_manager_add_ui( ui_manager, 
	    gtk_ui_manager_new_merge_id( ui_manager ),
	    dir_path,
	    name,
	    name,
	    GTK_UI_MANAGER_MENUITEM,
	    TRUE );
}

//PRIVATE void kill_newmenu(GtkWidget *menu, GtkItemFactory *ifact) {
//  gtk_object_unref(GTK_OBJECT(ifact));
//}

PRIVATE GtkItemFactory *get_new_ifact( void ) {

  GtkItemFactory *ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<new>", NULL);
  GList *lst = menuentries;

  while (lst != NULL) {
    MenuEntry *m = lst->data;
    GtkItemFactoryEntry ent = { m->menupath, NULL, newmenu_callback, 0, NULL };

    gtk_item_factory_create_item(ifact, &ent, m, 1);

    lst = g_list_next(lst);
  }
  menuentries_dirty = FALSE;




  return ifact;
}

PUBLIC GtkWidget *comp_get_newmenu(Sheet *sheet) {

    GtkWidget *menu;

    if( menufact == NULL ) {
	menufact = get_new_ifact();
	g_object_ref( G_OBJECT(menufact) );
    }

    if( menuentries_dirty ) {
	if( menufact != NULL ) {
	    g_object_unref( G_OBJECT(menufact) );
	}
	menufact = get_new_ifact();
    }

    gtk_object_set_user_data( GTK_OBJECT( menufact ), sheet );
    //gtk_signal_connect(GTK_OBJECT(menu), "destroy", GTK_SIGNAL_FUNC(kill_newmenu), ifact);

    menu = gtk_item_factory_get_widget(menufact, "<new>");

    return menu;
}

PUBLIC void comp_register_componentclass(ComponentClass *k) {
  g_hash_table_insert(componentclasses, k->class_tag, k);
}

PUBLIC Component *comp_new_component(ComponentClass *k, gpointer init_data,
				     Sheet *sheet, gint x, gint y) {
  Component *c = safe_malloc(sizeof(Component));

  c->klass = k;
  c->sheet = sheet;
  c->x = x;
  c->y = y;
  c->saved_x = c->saved_y = 0;
  c->width = c->height = 0;
  c->connectors = NULL;
  c->data = NULL;

  if (k->initialize_instance == NULL) {
    g_warning("initialize_instance == NULL in comp_new_component of class %s",
	      k->class_tag);
    return c;
  }

  if (!k->initialize_instance(c, init_data)) {
    free(c);
    return NULL;
  }

  return c;
}

PUBLIC Component *comp_clone( Component *c, Sheet *sheet ) {

    Component *clone;
 
    if (c->klass->clone_instance == NULL) {
	g_warning("clone_instance == NULL in comp_clone of class %s",
		c->klass->class_tag);
	return NULL;
    }
 
    clone = c->klass->clone_instance( c, sheet );
    if( sheet == c->sheet ) {
	clone->x = c->x + 10;
	clone->y = c->y + 10;
    } else {
	clone->x = c->x;
	clone->y = c->y;
    }

    sheet_add_component( sheet, clone );

    return clone;
}


PRIVATE gboolean comp_try_unconnect( Component *c ) {
    GList *connX = c->connectors;

    while( connX != NULL ) {
	Connector *con = connX->data;

	while( con->refs != NULL ) {
	    ConnectorReference *ref = con->refs->data;
	    if( comp_unlink( ref, &(con->ref) ) != TRUE )
		    return FALSE;
	}

	connX = g_list_next( connX );
    }

    return TRUE;
}

PUBLIC gboolean comp_kill_component(Component *c) {

  if( !comp_try_unconnect( c ) )
      return FALSE;


  while (c->connectors != NULL) {
    GList *tmp = g_list_next(c->connectors);
    Connector *con = c->connectors->data;

    comp_kill_connector(con);

    g_list_free_1(c->connectors);
    c->connectors = tmp;
  }

  if (c->klass->destroy_instance)
    c->klass->destroy_instance(c);

  free(c);
  return TRUE;
}

PUBLIC ConnectorReference *unpickle_connectorreference(ConnectorReference *ref,
							ObjectStoreItem *item) {
  if (ref == NULL)
    ref = safe_malloc(sizeof(ConnectorReference));

  ref->c = comp_unpickle(objectstore_item_get_object(item, "component"));
  ref->kind = objectstore_item_get_integer(item, "kind", COMP_NO_CONNECTOR);
  ref->is_output = objectstore_item_get_integer(item, "is_output", FALSE);
  ref->queue_number = objectstore_item_get_integer(item, "queue_number", 0);

  return ref;
}

PUBLIC gpointer unpickler_for_connectorreference(ObjectStoreItem *item) {
  return unpickle_connectorreference(NULL, item);
}

PRIVATE Connector *unpickle_connector(ObjectStoreItem *item) {
  Connector *conn = safe_malloc(sizeof(Connector));

  conn->x = objectstore_item_get_integer(item, "x_coord", 0);
  conn->y = objectstore_item_get_integer(item, "y_coord", 0);
  unpickle_connectorreference(&conn->ref, objectstore_item_get_object(item, "source_ref"));
  conn->refs = objectstore_extract_list_of_items(objectstore_item_get(item, "targets"),
						 item->db, unpickler_for_connectorreference);
  return conn;
}


PUBLIC Component *comp_unpickle(ObjectStoreItem *item) {
  Component *comp = objectstore_get_object(item);
  ObjectStoreItem *shitem;

  if (comp == NULL) {
    comp = safe_malloc(sizeof(Component));
    objectstore_set_object(item, comp);

    {
      char *tag = objectstore_item_get_string(item, "class_tag", NULL);
      ComponentClass *k;
      RETURN_VAL_UNLESS(tag != NULL, NULL);
      k = g_hash_table_lookup(componentclasses, tag);
      if (k == NULL) {
	popup_msgbox("Class not found", MSGBOX_CANCEL, 0, MSGBOX_CANCEL,
		     "Component-class not found: tag = %s", tag);
	g_message("Component Class not found; tag = %s", tag);
	free(comp);
	return NULL;
      }
      comp->klass = k;
    }
    comp->data = NULL;

    comp->saved_x = comp->saved_y = 0;

    shitem = objectstore_item_get_object( item, "sheet" );
    if( shitem == NULL )
	shitem = objectstore_get_root( item->db );

    comp->sheet = sheet_unpickle( shitem );
    comp->x = objectstore_item_get_integer(item, "x_coord", 0);
    comp->y = objectstore_item_get_integer(item, "y_coord", 0);
    comp->width = objectstore_item_get_integer(item, "width", 70);
    comp->height = objectstore_item_get_integer(item, "height", 70);
    comp->connectors =
      objectstore_extract_list_of_items(objectstore_item_get(item, "connectors"),
					item->db,
					(objectstore_unpickler_t) unpickle_connector);
    comp->klass->unpickle_instance(comp, item, item->db);
  }

  return comp;
}

PUBLIC ObjectStoreItem *pickle_connectorreference(ConnectorReference *ref, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_new_item(db, "ConnectorReference", ref);
  objectstore_item_set_object(item, "component", comp_pickle(ref->c, db));
  objectstore_item_set_integer(item, "kind", ref->kind);
  objectstore_item_set_integer(item, "is_output", ref->is_output);
  objectstore_item_set_integer(item, "queue_number", ref->queue_number);
  return item;
}

PRIVATE ObjectStoreItem *pickle_connector(Connector *con, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_new_item(db, "Connector", con);
  
  objectstore_item_set_integer(item, "x_coord", con->x);
  objectstore_item_set_integer(item, "y_coord", con->y);
  objectstore_item_set_object(item, "source_ref", pickle_connectorreference(&con->ref, db));
  objectstore_item_set(item, "targets",
		       objectstore_create_list_of_items(con->refs, db,
							(objectstore_pickler_t)
							  pickle_connectorreference));
  return item;
}

PUBLIC ObjectStoreItem *comp_pickle(Component *c, ObjectStore *db) {
  ObjectStoreItem *item = objectstore_get_item(db, c);

  if (item == NULL) {
    item = objectstore_new_item(db, "Component", c);
    objectstore_item_set_string(item, "class_tag", c->klass->class_tag);
    objectstore_item_set_object(item, "sheet", sheet_pickle(c->sheet, db) );
    objectstore_item_set_integer(item, "x_coord", c->x);
    objectstore_item_set_integer(item, "y_coord", c->y);
    objectstore_item_set_integer(item, "width", c->width);
    objectstore_item_set_integer(item, "height", c->height);
    objectstore_item_set(item, "connectors",
			 objectstore_create_list_of_items(c->connectors, db,
							  (objectstore_pickler_t)
							    pickle_connector));
    c->klass->pickle_instance(c, item, db);
  }

  return item;
}

PUBLIC void comp_paint_connections(Component *c, GdkRectangle *area,
				   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  GList *l = c->connectors;

  while (l != NULL) {
    Connector *con = l->data;
    GList *o = con->refs;

    l = g_list_next(l);

    if (!con->ref.is_output)
      continue;

    while (o != NULL) {
      Connector *other = comp_get_connector(o->data);

      if( other != NULL ) {
	  gdk_draw_line(drawable, style->white_gc,
		  con->x + c->x, con->y + c->y,
		  other->x + other->ref.c->x, other->y + other->ref.c->y);
      }

      o = g_list_next(o);
    }
  }
}

PUBLIC void comp_paint(Component *c, GdkRectangle *area,
		       GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  g_return_if_fail(c->klass->paint != NULL);
  c->klass->paint(c, area, drawable, style, colors);
}

PUBLIC int comp_find_connector(Component *c, gint x, gint y, ConnectorReference *ref) {
  if (c->klass->find_connector_at)
    return c->klass->find_connector_at(c, x, y, ref);
  else
    return 0;
}

PUBLIC int comp_contains_point(Component *c, gint x, gint y) {
  if (c->klass->contains_point)
    return c->klass->contains_point(c, x, y);
  else
    return (x >= c->x && y >= c->y &&
	    x < (c->x + c->width) &&
	    y < (c->y + c->height));
}

PRIVATE gint find_connector(Connector *con, ConnectorReference *ref) {
  return
    (ref->c != con->ref.c) ||
    (ref->queue_number != con->ref.queue_number) ||
    (ref->kind != con->ref.kind) ||
    (ref->is_output != con->ref.is_output);
}

PUBLIC gint connectorreference_equal(ConnectorReference *r1, ConnectorReference *r2) {
  return
    (r1->c != r2->c) ||
    (r1->queue_number != r2->queue_number) ||
    (r1->kind != r2->kind) ||
    (r1->is_output != r2->is_output);
}

PUBLIC void comp_link(ConnectorReference *src, ConnectorReference *dst) {
  g_return_if_fail(src != NULL && dst != NULL);

  if (src->is_output == dst->is_output)
    return;

  if (!src->is_output) {
    ConnectorReference *tmp = src;
    src = dst;
    dst = tmp;
  }

  if (src->kind != dst->kind &&
      src->kind != COMP_ANY_CONNECTOR &&
      dst->kind != COMP_ANY_CONNECTOR)
    return;

  if (g_list_find_custom(comp_get_connector(src)->refs, dst,
			 (GCompareFunc) connectorreference_equal) != NULL)
    return;

  if (src->c->klass->accept_outbound)
    if (!src->c->klass->accept_outbound(src->c, src, dst))
      return;

  if (dst->c->klass->accept_inbound)
    if (!dst->c->klass->accept_inbound(dst->c, src, dst)) {
      src->c->klass->unlink_outbound(src->c, src, dst);
      return;
    }

  comp_insert_connection(comp_get_connector(src), dst);
  comp_insert_connection(comp_get_connector(dst), src);
}

PUBLIC gboolean comp_unlink(ConnectorReference *src, ConnectorReference *dst) {
  Connector *srccon, *dstcon;
  g_return_val_if_fail(src != NULL && dst != NULL, FALSE);

  if (src->is_output == dst->is_output)
    return FALSE;

  if (!src->is_output) {
    ConnectorReference *tmp = src;
    src = dst;
    dst = tmp;
  }

  if (src->kind != dst->kind &&
      src->kind != COMP_ANY_CONNECTOR &&
      dst->kind != COMP_ANY_CONNECTOR)
    return FALSE;

  if (src->c->klass->unlink_outbound) 
      if( !src->c->klass->unlink_outbound(src->c, src, dst) )
	  return FALSE;

  if (dst->c->klass->unlink_inbound)
      if( !dst->c->klass->unlink_inbound(dst->c, src, dst) )
	  return FALSE;

  srccon = comp_get_connector(src);
  dstcon = comp_get_connector(dst);
  comp_remove_connection(srccon, dst);
  comp_remove_connection(dstcon, src);

  return TRUE;
}

PUBLIC char *comp_get_title(Component *c) {
  g_return_val_if_fail(c->klass->get_title != NULL, NULL);
  return c->klass->get_title(c);
}

PUBLIC char *comp_get_connector_name(ConnectorReference *ref) {
  Component *c = ref->c;
  char *title = comp_get_title(ref->c);
  char *conn_name;
  char *result;

  if (c->klass->get_connector_name == NULL)
    return title;

  conn_name = c->klass->get_connector_name(c, ref);

  result = malloc(strlen(title) + strlen(conn_name) + 4); /* " [" , "]" and '\0' */
  if (result == NULL) {
    free(conn_name);
    return title;
  }

  sprintf(result, "%s [%s]", title, conn_name);
  free(conn_name);
  free(title);

  return result;
}
/*
PUBLIC void comp_append_popup(Component *c, GtkWidget *menu) {
  char *name;
  GtkWidget *submenu;
  GtkWidget *item;

  g_return_if_fail(c->klass->get_title != NULL);

  if (c->klass->build_popup == NULL)
    return;

  name = c->klass->get_title(c);
  submenu = c->klass->build_popup(c);

  item = gtk_menu_item_new_with_label(name);
  free(name);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  gtk_menu_append(GTK_MENU(menu), item);
  gtk_widget_show(item);
}
*/

PUBLIC GtkWidget *comp_get_popup(Component *c) {

  if (c->klass->build_popup == NULL)
    return NULL;

  return c->klass->build_popup(c);
}

PUBLIC Connector *comp_new_connector(Component *c, ConnectorKind kind,
				     gboolean is_output, gint queue_number,
				     gint x, gint y) {
  Connector *con = safe_malloc(sizeof(Connector));

  con->ref.c = c;
  con->ref.kind = kind;
  con->ref.is_output = is_output;
  con->ref.queue_number = queue_number;
  con->refs = NULL;
  con->x = x;
  con->y = y;

  c->connectors = g_list_prepend(c->connectors, con);

  return con;
}

PUBLIC Connector *comp_get_connector(ConnectorReference *ref) {
  GList *node = g_list_find_custom(ref->c->connectors, ref, (GCompareFunc) find_connector);

  return (node == NULL) ? NULL : node->data;
}

PUBLIC void comp_kill_connector(Connector *con) {
  while (con->refs != NULL) {
    ConnectorReference *ref = con->refs->data;

    comp_unlink(ref, &(con->ref) ); 
  }

  free(con);
}

PUBLIC void comp_insert_connection(Connector *con, ConnectorReference *other) {
  ConnectorReference *clone = safe_malloc(sizeof(ConnectorReference));

  *clone = *other;
  con->refs = g_list_prepend(con->refs, clone);
}

PUBLIC void comp_remove_connection(Connector *con, ConnectorReference *other) {
  GList *node = g_list_find_custom(con->refs, other, (GCompareFunc) connectorreference_equal);
  g_return_if_fail(node != NULL);

  free(node->data);
  con->refs = g_list_remove_link(con->refs, node);
  g_list_free_1(node);
}

PRIVATE void clone_connection( ConnectorReference src, ConnectorReference dst, GHashTable *clonemap ) {
    src.c = g_hash_table_lookup( clonemap, src.c );
    dst.c = g_hash_table_lookup( clonemap, dst.c );

    if( src.c && dst.c )
	comp_link( &src, &dst );
}

PUBLIC void comp_clone_list( GList *lst, Sheet *sheet ) {

    GHashTable *clonemap = g_hash_table_new( g_direct_hash, g_direct_equal );
    
    GList *compX;

    // Clone all components in list, and generate the map from src to clone
    for( compX = lst; compX; compX = g_list_next( compX ) ) {

	Component *c = compX->data;

	Component *clone = comp_clone( c, sheet );
	g_hash_table_insert( clonemap, c, clone );
    }

    // now read out all connections and connect the clones

    for( compX = lst; compX; compX = g_list_next( compX ) ) {

	Component *c = compX->data;
	GList *connX;
	for( connX = c->connectors; connX; connX = g_list_next( connX ) ) {
	    Connector *con = connX->data;

	    GList *refX;
	    for( refX=con->refs; refX; refX = g_list_next( refX ) ) {
		ConnectorReference *ref = refX->data;

		clone_connection( con->ref, *ref, clonemap );
	    }
	}
    }
}

PUBLIC void init_comp(void) {
  componentclasses = g_hash_table_new(g_str_hash, g_str_equal);
  experiment();
}

PUBLIC void done_comp(void) {
  g_hash_table_destroy(componentclasses);
  componentclasses = NULL;
}

