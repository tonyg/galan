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
#include "comp.h"
#include "sheet.h"
#include "msgbox.h"
#include "control.h"
#include "gencomp.h"

#define GENCOMP_ICONLENGTH	48 /**< The Length of an icon. Hmmm what is that for ? */
#define GENCOMP_TITLEHEIGHT	15 /**< The height of the title string */
#define GENCOMP_CONNECTOR_SPACE	5  /**< Space between two connectors */
#define GENCOMP_CONNECTOR_WIDTH	10 
#define GENCOMP_BORDER_WIDTH	(GENCOMP_CONNECTOR_WIDTH + GENCOMP_CONNECTOR_SPACE)

/**
 * This is for enumerating all the components.
 * I would prefer a counter for every Generatorclass.
 * But this is for the future.
 */

PRIVATE int next_component_number = 1;

/**
 * \brief Dictionary mapping from string -> GeneratorClass
 *
 * This hash contains all GeneratorClasses. When a generator is unpickled
 * its class is determined via this dictionary.
 */

PRIVATE GHashTable *generatorclasses = NULL;


/**
 * \brief Build Connector s when instantiating a Component.
 *
 * \param c The Component the connectors should be build on.
 * \param count The number of connectors to be build.
 * \param is_outbound When TRUE indicates that the function should build outbound connectors.
 * \param is_signal When TRUE indicates that the function should build signal connectors.
 *
 */

PRIVATE void build_connectors(Component *c, int count, gboolean is_outbound, gboolean is_signal) {
  int i;

  for (i = 0; i < count; i++)
    comp_new_connector(c, is_signal? COMP_SIGNAL_CONNECTOR : COMP_EVENT_CONNECTOR,
		       is_outbound, i,
		       0, 0);
}

/**
 * \brief This function sets the \a x and \a y fields of the Connector `s of a Component.
 *
 * \param c The Component the connectors should be set.
 * \param count The number of connectors to be set.
 * \param is_outbound When TRUE indicates that the function should setup the outbound connectors.
 * \param is_signal When TRUE indicates that the function should setup the signal connectors.
 * \param hsize The horizontal size of the area relevatn for connector placement.
 * \param vsize The vertical size of the area relevatn for connector placement.
 *
 */

PRIVATE void resize_connectors(Component *c, int count,
			       gboolean is_outbound, gboolean is_signal,
			       int hsize, int vsize) {
  int spacing = (is_signal ? vsize : hsize) / (count + 1);
  int startpos = is_outbound ? (GENCOMP_BORDER_WIDTH * 2
				+ (is_signal ? hsize : vsize)
				- (GENCOMP_CONNECTOR_WIDTH >> 1))
			     : (GENCOMP_CONNECTOR_WIDTH >> 1);
  int x = is_signal ? startpos : GENCOMP_BORDER_WIDTH + spacing;
  int y = is_signal ? GENCOMP_BORDER_WIDTH + spacing : startpos;
  int dx = is_signal ? 0 : spacing;
  int dy = is_signal ? spacing : 0;
  int i;

  for (i = 0; i < count; i++, x += dx, y += dy) {
    ConnectorReference ref = { c, is_signal ? COMP_SIGNAL_CONNECTOR : COMP_EVENT_CONNECTOR,
			       is_outbound, i };
    Connector *con = comp_get_connector(&ref);

    con->x = x;
    con->y = y;
  }
}

/**
 * \brief Determine the size of the Component \a c and lay out the Connector `s.
 *
 * \param c The Component to resize.
 *
 */

PRIVATE void gencomp_resize(Component *c) {
  int body_vert, body_horiz;
  GenCompData *d = c->data;
  Generator *g = d->g;
  GeneratorClass *k = g->klass;

  body_vert =
    GENCOMP_CONNECTOR_WIDTH
    + MAX(MAX(k->in_sig_count, k->out_sig_count) * GENCOMP_CONNECTOR_WIDTH,
	  GENCOMP_TITLEHEIGHT + (d->icon ? GENCOMP_ICONLENGTH : 0));
  body_horiz =
    GENCOMP_CONNECTOR_WIDTH
    + MAX((d->icon ? GENCOMP_ICONLENGTH : 0) + 2,
	  MAX(sheet_get_textwidth(c->sheet, g->name),
	      MAX(k->in_count * GENCOMP_CONNECTOR_WIDTH,
		  k->out_count * GENCOMP_CONNECTOR_WIDTH)));

  resize_connectors(c, k->in_count, 0, 0, body_horiz, body_vert);
  resize_connectors(c, k->in_sig_count, 0, 1, body_horiz, body_vert);
  resize_connectors(c, k->out_count, 1, 0, body_horiz, body_vert);
  resize_connectors(c, k->out_sig_count, 1, 1, body_horiz, body_vert);

  c->width = body_horiz + 2 * GENCOMP_BORDER_WIDTH + 1;
  c->height = body_vert + 2 * GENCOMP_BORDER_WIDTH + 1;
}

/**
 * \brief Initialize the GenComp specific part of the Component.
 *
 * \param c The Component which is already setup by the component constructor.
 * \param init_data The GenCompInitData which is needed to setup the component.
 */

PRIVATE int gencomp_initialize(Component *c, gpointer init_data) {
  GenCompData *d = safe_malloc(sizeof(GenCompData));
  GenCompInitData *id = (GenCompInitData *) init_data;
  char *name;
  GdkBitmap *mask;

  name = safe_malloc(strlen(id->k->name) + 20);
  sprintf(name, "%s%d", id->k->name, next_component_number++);

  d->g = gen_new_generator(id->k, name);

  if (d->g == NULL) {
    safe_free(name);
    safe_free(d);
    return 0;
  }

  d->icon =
    id->iconpath ? gdk_pixmap_create_from_xpm(sheet_get_window(c->sheet),
					      &mask,
					      sheet_get_transparent_color(c->sheet),
					      id->iconpath)
    : NULL;

  d->propgen = id->propgen;

  build_connectors(c, id->k->in_count, 0, 0);
  build_connectors(c, id->k->in_sig_count, 0, 1);
  build_connectors(c, id->k->out_count, 1, 0);
  build_connectors(c, id->k->out_sig_count, 1, 1);

  c->x -= GENCOMP_BORDER_WIDTH;
  c->y -= GENCOMP_BORDER_WIDTH;
  c->width = c->height = 0;
  c->data = d;

  safe_free(name);

  gencomp_resize(c);

  return 1;
}

/**
 * \brief Destroy the GenCompData of a Component.
 *
 * \pram c The Component which is being destroyed.
 */

PRIVATE void gencomp_destroy(Component *c) {
  GenCompData *d = c->data;

  gen_kill_generator(d->g);
  if (d->icon != NULL)
    gdk_pixmap_unref(d->icon);

  safe_free(d);
}

/**
 * \brief Clone a GenComp
 *
 * \param c The Component to be cloned.
 * \param sheet The Sheet where it should be placed
 */

PRIVATE Component *gencomp_clone(Component *c, Sheet *sheet) {
    
    GenCompData *d = c->data;

    GenCompInitData *id = g_hash_table_lookup(generatorclasses, d->g->klass->tag);

    Component *clone = comp_new_component(  c->klass, id, sheet, 0, 0 );
    GenCompData *clonedata = clone->data;

    gen_kill_generator( clonedata->g );
    clonedata->g = gen_clone( d->g, sheet->control_panel );

    return clone;
    gencomp_resize(clone);
    //sheet_queue_redraw_component( sheet, clone );
}


/**
 * \brief assure that connectors are valid.
 *
 */

PRIVATE GList *collect_connectors(GList *lst, Component *c, ConnectorKind kind,
				  gboolean is_output, int num_conns) {
  ConnectorReference ref;
  int i;

  ref.c = c;
  ref.kind = kind;
  ref.is_output = is_output;

  for (i = 0; i < num_conns; i++) {
    Connector *con;

    ref.queue_number = i;
    con = comp_get_connector(&ref);
    if (con == NULL)
      con = comp_new_connector(c, kind, is_output, i, 0, 0);
    lst = g_list_prepend(lst, con);
  }

  return lst;
}

/**
 * \brief Validate all Connector `s on Component \a c.
 *
 * \pram c The Component to validate.
 */

PRIVATE void validate_connectors(Component *c) {
  GenCompData *d = c->data;
  GeneratorClass *k = d->g->klass;
  GList *lst, *temp;

  lst = NULL;
  lst = collect_connectors(lst, c, COMP_EVENT_CONNECTOR, FALSE, k->in_count);
  lst = collect_connectors(lst, c, COMP_EVENT_CONNECTOR, TRUE, k->out_count);
  lst = collect_connectors(lst, c, COMP_SIGNAL_CONNECTOR, FALSE, k->in_sig_count);
  lst = collect_connectors(lst, c, COMP_SIGNAL_CONNECTOR, TRUE, k->out_sig_count);

  temp = c->connectors;
  c->connectors = lst;
  g_list_free(temp);
}

PRIVATE void gencomp_unpickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  GenCompData *d = safe_malloc(sizeof(GenCompData));
  GenCompInitData *id;
  GdkBitmap *mask;

  d->g = gen_unpickle(objectstore_item_get_object(item, "gencomp_generator"));

  id = g_hash_table_lookup(generatorclasses, d->g->klass->tag);

  if( id != NULL ) {
      d->icon =
	  id->iconpath ? gdk_pixmap_colormap_create_from_xpm(sheet_get_window(c->sheet), gdk_colormap_get_system(),
		  &mask,
		  sheet_get_transparent_color(c->sheet),
		  id->iconpath)
	  : NULL;

      d->propgen = id->propgen;
  }
  else
  {
      d->icon = NULL;
      d->propgen = NULL;
  }

  c->data = d;
  validate_connectors(c);

  gencomp_resize(c);	/* because things may be different if loading on a different
			   system from the one we saved with */
}

PRIVATE void gencomp_pickle(Component *c, ObjectStoreItem *item, ObjectStore *db) {
  GenCompData *d = c->data;
  objectstore_item_set_object(item, "gencomp_generator", gen_pickle(d->g, db));
}

/* %%% Is this due to GTK changing from 1.2.x to 1.3.x, or is it a bug? */
#ifdef NATIVE_WIN32
#define FULLCIRCLE_DEGREES_NUMBER	72000
#else
#define FULLCIRCLE_DEGREES_NUMBER	36000
#endif

PRIVATE void gencomp_paint(Component *c, GdkRectangle *area,
			   GdkDrawable *drawable, GtkStyle *style, GdkColor *colors) {
  GenCompData *d = c->data;
  GList *l = c->connectors;
  GdkGC *gc = style->black_gc;

  while (l != NULL) {
    Connector *con = l->data;
    int colidx;
    int x, y;

    if( !connectorreference_equal( &(con->ref), &(c->sheet->highlight_ref) ) )
	colidx = COMP_COLOR_VIOLET;
    else {
	if (con->ref.kind == COMP_SIGNAL_CONNECTOR) {
	    guint flags = con->ref.is_output
			? d->g->klass->out_sigs[con->ref.queue_number].flags
			: d->g->klass->in_sigs[con->ref.queue_number].flags;


	    if( flags & SIG_FLAG_OPENGL ) {
		colidx = (con->refs == NULL) ? COMP_COLOR_CYAN : COMP_COLOR_BLACK;
	    } else if( flags & SIG_FLAG_RANDOMACCESS ) {
		colidx = (con->refs == NULL) ? COMP_COLOR_RED : COMP_COLOR_YELLOW;
	    } else {
		colidx = (con->refs == NULL) ? COMP_COLOR_BLUE : COMP_COLOR_GREEN;
	    }
	} else
	    colidx = (con->refs == NULL) ? COMP_COLOR_BLUE : COMP_COLOR_GREEN;
    }

    gdk_gc_set_foreground(gc, &colors[colidx]);
    gdk_draw_arc(drawable, gc, TRUE,
		 con->x + c->x - (GENCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (GENCOMP_CONNECTOR_WIDTH>>1),
		 GENCOMP_CONNECTOR_WIDTH,
		 GENCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);
    gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
    gdk_draw_arc(drawable, gc, FALSE,
		 con->x + c->x - (GENCOMP_CONNECTOR_WIDTH>>1),
		 con->y + c->y - (GENCOMP_CONNECTOR_WIDTH>>1),
		 GENCOMP_CONNECTOR_WIDTH,
		 GENCOMP_CONNECTOR_WIDTH,
		 0, FULLCIRCLE_DEGREES_NUMBER);

    x = ((con->ref.kind == COMP_SIGNAL_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->x - (GENCOMP_CONNECTOR_SPACE + (GENCOMP_CONNECTOR_WIDTH>>1))
	    : con->x + (GENCOMP_CONNECTOR_WIDTH>>1))
	 : con->x) + c->x;

    y = ((con->ref.kind == COMP_EVENT_CONNECTOR)
	 ? (con->ref.is_output
	    ? con->y - (GENCOMP_CONNECTOR_SPACE + (GENCOMP_CONNECTOR_WIDTH>>1))
	    : con->y + (GENCOMP_CONNECTOR_WIDTH>>1))
	 : con->y) + c->y;

    gdk_draw_line(drawable, gc,
		  x, y,
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? x + GENCOMP_CONNECTOR_SPACE : x),
		  (con->ref.kind == COMP_SIGNAL_CONNECTOR ? y : y + GENCOMP_CONNECTOR_SPACE));

    l = g_list_next(l);
  }

  gdk_gc_set_foreground(gc, &colors[COMP_COLOR_BLACK]);
  gdk_draw_rectangle(drawable, gc, TRUE,
		     c->x + GENCOMP_BORDER_WIDTH,
		     c->y + GENCOMP_BORDER_WIDTH,
		     c->width - 2 * GENCOMP_BORDER_WIDTH,
		     c->height - 2 * GENCOMP_BORDER_WIDTH);
  gdk_gc_set_foreground(gc, &colors[COMP_COLOR_WHITE]);
  gdk_draw_rectangle(drawable, gc, FALSE,
		     c->x + GENCOMP_BORDER_WIDTH,
		     c->y + GENCOMP_BORDER_WIDTH,
		     c->width - 2 * GENCOMP_BORDER_WIDTH - 1,
		     c->height - 2 * GENCOMP_BORDER_WIDTH - 1);

// XXX: There is only style->font_desc
//      Get some pango layout and render it here...
//
  {
  PangoLayout *layout = gtk_widget_create_pango_layout( c->sheet->drawingwidget, d->g->name );
  gdk_draw_layout(drawable, gc,
		c->x + GENCOMP_BORDER_WIDTH + (GENCOMP_CONNECTOR_WIDTH>>1),
		c->y + GENCOMP_BORDER_WIDTH, 
		layout);
  g_object_unref( G_OBJECT( layout ) );
  }

/*  gdk_draw_text(drawable, gtk_style_get_font(style), gc,
		c->x + GENCOMP_BORDER_WIDTH + (GENCOMP_CONNECTOR_WIDTH>>1),
		c->y + GENCOMP_BORDER_WIDTH + GENCOMP_TITLEHEIGHT - 3,
		d->g->name, strlen(d->g->name));
		*/

  if (d->icon != NULL)
    gdk_draw_pixmap(drawable, gc, d->icon, 0, 0,
		    c->x + (c->width>>1) - (GENCOMP_ICONLENGTH>>1),
		    c->y + ((c->height-GENCOMP_TITLEHEIGHT)>>1) - (GENCOMP_ICONLENGTH>>1)
		         + GENCOMP_TITLEHEIGHT,
		    GENCOMP_ICONLENGTH,
		    GENCOMP_ICONLENGTH);
  
  gdk_gc_set_foreground(gc, &style->black);
}

PRIVATE int gencomp_find_connector_at(Component *c, gint x, gint y, ConnectorReference *ref) {
  GList *l = c->connectors;

  x -= c->x;
  y -= c->y;

  while (l != NULL) {
    Connector *con = l->data;

    if (ABS(x - con->x) < (GENCOMP_CONNECTOR_WIDTH>>1) &&
	ABS(y - con->y) < (GENCOMP_CONNECTOR_WIDTH>>1)) {
      if (ref != NULL)
	*ref = con->ref;
      return 1;
    }

    l = g_list_next(l);
  }

  return 0;
}

PRIVATE int gencomp_contains_point(Component *c, gint x, gint y) {
  gint dx = x - c->x;
  gint dy = y - c->y;

  if (dx >= GENCOMP_BORDER_WIDTH &&
      dy >= GENCOMP_BORDER_WIDTH &&
      dx < (c->width - GENCOMP_BORDER_WIDTH) &&
      dy < (c->height - GENCOMP_BORDER_WIDTH))
    return 1;

  return gencomp_find_connector_at(c, x, y, NULL);
}

PRIVATE gboolean gencomp_accept_outbound(Component *c, ConnectorReference *src,
					 ConnectorReference *dst) {
  GenCompData *data = c->data;
  GenCompData *otherdata = dst->c->data;

  if (dst->c->klass == &GeneratorComponentClass)
    return (gen_link(src->kind == COMP_SIGNAL_CONNECTOR,
		     data->g, src->queue_number,
		     otherdata->g, dst->queue_number) != NULL);
  else
    return TRUE;	/* accept links to other classes of component */
}

PRIVATE gboolean gencomp_accept_inbound(Component *c, ConnectorReference *src,
					ConnectorReference *dst) {
  /* noop: the gen_link has already been done by
     gencomp_accept_outbound, if it's supposed to happen at all. */
  return TRUE;
}

PRIVATE gboolean gencomp_unlink_outbound(Component *c, ConnectorReference *src,
				     ConnectorReference *dst) {
  GenCompData *data = c->data;
  GenCompData *otherdata = dst->c->data;

  if (dst->c->klass == &GeneratorComponentClass)
    gen_unlink(gen_find_link(src->kind == COMP_SIGNAL_CONNECTOR,
			     data->g, src->queue_number,
			     otherdata->g, dst->queue_number));
  return TRUE;
}

PRIVATE gboolean gencomp_unlink_inbound(Component *c, ConnectorReference *src,
				    ConnectorReference *dst) {
  /* noop: the gen_link has already been removed by
     gencomp_unlink_outbound, if it's supposed to happen at all. */

  return TRUE;
}

PRIVATE char *gencomp_get_title(Component *c) {
  return safe_string_dup(((GenCompData *) c->data)->g->name);
}

PRIVATE char *gencomp_get_connector_name(Component *c, ConnectorReference *ref) {
  GeneratorClass *k = ((GenCompData *) c->data)->g->klass;

  return safe_string_dup(ref->kind == COMP_SIGNAL_CONNECTOR
			 ? (ref->is_output
			    ? k->out_sigs[ref->queue_number].name
			    : k->in_sigs[ref->queue_number].name)
			 : (ref->is_output
			    ? k->out_names[ref->queue_number]
			    : k->in_names[ref->queue_number]));
}

PRIVATE void rename_controls(Control *c, Generator *g) {
  control_update_names(c);
}

PRIVATE GtkWidget *rename_text_widget = NULL;

PRIVATE void rename_handler(MsgBoxResponse action_taken, Component *c) {
  if (action_taken == MSGBOX_OK) {
    GenCompData *d = c->data;
    free(d->g->name);
    d->g->name = safe_string_dup(gtk_entry_get_text(GTK_ENTRY(rename_text_widget)));

    g_list_foreach(d->g->controls, (GFunc) rename_controls, d->g);

    sheet_queue_redraw_component(c->sheet, c);	/* to 'erase' the old size */
    gencomp_resize(c);
    gtk_widget_queue_draw(c->sheet->drawingwidget);
    //sheet_queue_redraw_component(c->sheet, c);	/* to 'fill in' the new size */
  }
}

PRIVATE void do_rename(Component *c, guint action, GtkWidget *widget) {
  GenCompData *d = c->data;
  GtkWidget *hb = gtk_hbox_new(FALSE, 5);
  GtkWidget *label = gtk_label_new("Rename component:");
  GtkWidget *text = gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(hb), label, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hb), text, TRUE, FALSE, 0);

  gtk_widget_show(label);
  gtk_widget_show(text);

  gtk_entry_set_text(GTK_ENTRY(text), d->g->name);

  rename_text_widget = text;
  popup_dialog("Rename", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK, hb,
	       (MsgBoxResponseHandler) rename_handler, c);
}

PRIVATE void do_props(Component *c, guint action, GtkWidget *widget) {
  GenCompData *d = c->data;
  if (d->propgen)
    d->propgen(c, d->g);
}

PRIVATE void do_delete(Component *c, guint action, GtkWidget *widget) {
  sheet_delete_component(c->sheet, c);
}

PRIVATE void do_clone(Component *c, guint action, GtkWidget *widget) {
    
    GenCompData *d = c->data;

    GenCompInitData *id = g_hash_table_lookup(generatorclasses, d->g->klass->tag);

    Component *clone = sheet_build_new_component(  c->sheet, c->klass, id );
    GenCompData *clonedata = clone->data;
    gen_kill_generator( clonedata->g );
    clonedata->g = gen_clone( d->g, c->sheet->control_panel );
    gencomp_resize(clone);
    sheet_queue_redraw_component( c->sheet, c );
}

PRIVATE GtkItemFactoryEntry popup_items[] = {
  { "/_Rename...",	NULL,	do_rename, 0,		NULL },
  { "/_Clone",		NULL,	do_clone, 0,		NULL },
  { "/New Control",	NULL,	NULL, 0,		"<Branch>" },
  { "/_Properties...",	NULL,	do_props, 0,		NULL },
  { "/sep1",		NULL,	NULL, 0,		"<Separator>" },
  { "/_Delete",		NULL,	do_delete, 0,		NULL },
};

PRIVATE void kill_popup(GtkWidget *popup, GtkItemFactory *ifact) {
  gtk_object_unref(GTK_OBJECT(ifact));
}

PRIVATE void new_control_callback(Component *c, guint control_index, GtkWidget *menuitem) {
  GenCompData *d = c->data;

  control_new_control(&d->g->klass->controls[control_index], d->g, c->sheet->control_panel );
}

PRIVATE void all_controls_callback(Component *c, guint control_index, GtkWidget *menuitem) {

  int i;
  GenCompData *d = c->data;

  for (i = 0; i < d->g->klass->numcontrols; i++)
      control_new_control(&d->g->klass->controls[i], d->g, c->sheet->control_panel);
}

#define NEW_CONTROL_PREFIX "/New Control/"

PRIVATE GtkWidget *gencomp_build_popup(Component *c) {
  GenCompData *d = c->data;
  GtkItemFactory *ifact;
  int nitems = sizeof(popup_items) / sizeof(popup_items[0]);
  GtkWidget *result;
  int i;

  ifact = gtk_item_factory_new(GTK_TYPE_MENU, "<gencomp-popup>", NULL);
  gtk_item_factory_create_items(ifact, nitems, popup_items, c);

  for (i = 0; i < d->g->klass->numcontrols; i++) {
    GtkItemFactoryEntry ent = { NULL, NULL, new_control_callback, i, NULL };
    char *name = safe_malloc(strlen(NEW_CONTROL_PREFIX) + strlen(d->g->klass->controls[i].name) + 1);

    strcpy(name, NEW_CONTROL_PREFIX);
    strcat(name, d->g->klass->controls[i].name);
    ent.path = name;

    gtk_item_factory_create_item(ifact, &ent, c, 1);
    free(name);
  }

  if(d->g->klass->numcontrols > 3) {
      GtkItemFactoryEntry ent = { NEW_CONTROL_PREFIX "ALL", NULL, all_controls_callback, 0, NULL };
      gtk_item_factory_create_item(ifact, &ent, c, 1);
  }


  result = gtk_item_factory_get_widget(ifact, "<gencomp-popup>");

#ifndef NATIVE_WIN32
  /* %%% Why does gtk_item_factory_get_item() not exist in the gtk-1.3 libraries?
     Maybe it's something I'm doing wrong. I'll have another go at fixing it later. */
  if (d->g->klass->numcontrols == 0)
    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<gencomp-popup>/New Control"),
			 GTK_STATE_INSENSITIVE);

  if (d->propgen == NULL)
    gtk_widget_set_state(gtk_item_factory_get_item(ifact, "<gencomp-popup>/Properties..."),
			 GTK_STATE_INSENSITIVE);
#endif

  gtk_signal_connect(GTK_OBJECT(result), "destroy", GTK_SIGNAL_FUNC(kill_popup), ifact);

  return result;
}

/**
 * \brief The GenComp ComponentClass.
 *
 */

PUBLIC ComponentClass GeneratorComponentClass = {
  "gencomp",

  gencomp_initialize,
  gencomp_destroy,
  gencomp_clone,

  gencomp_unpickle,
  gencomp_pickle,

  gencomp_paint,

  gencomp_find_connector_at,
  gencomp_contains_point,

  gencomp_accept_outbound,
  gencomp_accept_inbound,
  gencomp_unlink_outbound,
  gencomp_unlink_inbound,

  gencomp_get_title,
  gencomp_get_connector_name,

  gencomp_build_popup
};

/**
 * \brief Register a GeneratorClass so that a Component with GeneratorComponentClass can be instantiated.
 *
 * This function allocates the GenCompInitData describing GenratorClass \a k and puts it into
 * the generatorclasses hash.
 *
 * \param k The New GeneratorClass.
 * \param prefer If there already exists a GeneratorClass with the same name and \a prefer is TRUE 
 *        it is overwritten by the new GenratorClass. This is used by the audio_out plugins for example.
 * \param menupath This is a string indicating where the GenratorClass shows up in the New Menu.
 * \param iconpath This is a string is the path to the pixmap which is shown in the component.
 * \param propgen This function will be called when the Properties menu of the Component is selected.
 *
 */

PUBLIC void gencomp_register_generatorclass(GeneratorClass *k, gboolean prefer, char *menupath, char *iconpath, PropertiesCallback propgen) {
  GenCompInitData *id = safe_malloc(sizeof(GenCompInitData));

  id->k = k;
  id->iconpath = safe_string_dup(iconpath);
  id->propgen = propgen;

  comp_add_newmenu_item(menupath, &GeneratorComponentClass, id);

  comp_create_action( menupath, &GeneratorComponentClass, id, k->name, k->tag );


  /* Only insert into hash table if this name is not already taken, or if we are preferred. */
  {
    GenCompInitData *oldid = g_hash_table_lookup(generatorclasses, k->tag);

    if (oldid == NULL)
      g_hash_table_insert(generatorclasses, k->tag, id);
    else if (prefer) {
      g_hash_table_remove(generatorclasses, k->tag);
      g_hash_table_insert(generatorclasses, k->tag, id);
    }
  }
}

/**
 * \brief Initialize the GenComp.
 */

PUBLIC void init_gencomp(void) {
  generatorclasses = g_hash_table_new(g_str_hash, g_str_equal);
  comp_register_componentclass(&GeneratorComponentClass);
}

/**
 * \brief Cleanup the GenComp.
 */

PUBLIC void done_gencomp(void) {
  g_hash_table_destroy(generatorclasses);
  generatorclasses = NULL;
}


