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

#include "comp-action-class.h"
#include "global.h"
#include "gui.h"
#include "sheet.h"


enum {
    COMPACTION_COMPKLASS = 1,
    COMPACTION_INIT_DATA
};


static void compaction_instance_init( GTypeInstance *instance, gpointer g_class ) {

    //CompAction *self = (CompAction *) instance;

}

static void compaction_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *pspec ) {
    CompAction *self = COMPACTION( object );
    
    switch( property_id ) {
	case COMPACTION_COMPKLASS:
	    self->k = g_value_get_pointer( value );
	    break;
	case COMPACTION_INIT_DATA:
	    self->init_data = g_value_get_pointer( value );
	    break;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	    break;
	printf( "halb gut...\n" );
    }
}

static void compaction_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *pspec ) {
    CompAction *self = COMPACTION( object );
    
    switch( property_id ) {
	case COMPACTION_COMPKLASS:
	    g_value_set_pointer( value, self->k );
	    break;
	case COMPACTION_INIT_DATA:
	    g_value_set_pointer( value, self->init_data );
	    break;
	printf( "halb gut...\n" );
    }
}

static void compaction_activate( GtkAction *action ) {
    CompAction *self = COMPACTION( action );

    printf( "Ok... soweit isses schon... k->name = %s\n", self->k->class_tag );
    Sheet *s = gui_get_active_sheet();
    sheet_build_new_component(s, self->k, self->init_data);
}

static void compaction_class_init( gpointer g_class, gpointer g_class_data ) {

    GObjectClass *gobject_class = G_OBJECT_CLASS( g_class );
    GtkActionClass *gtkaction_class = GTK_ACTION_CLASS( g_class );
    //CompActionClass *klass = COMPACTION_CLASS( g_class );

    GParamSpec *pspec;
    
    gobject_class->set_property = compaction_set_property;
    gobject_class->get_property = compaction_get_property;

    pspec = g_param_spec_pointer( 
	    "component-class",
	    "class",
	    "comp",
	    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE );
    g_object_class_install_property( gobject_class, COMPACTION_COMPKLASS, pspec );

    pspec = g_param_spec_pointer( 
	    "init-data",
	    "data",
	    "init",
	    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE );
    g_object_class_install_property( gobject_class, COMPACTION_INIT_DATA, pspec );

    gtkaction_class->activate = compaction_activate;
}


GType compaction_get_type( void ) {
    static GType type = 0;
    if( type == 0 ) {
	static const GTypeInfo info = {
	    sizeof( CompActionClass ),
	    NULL,
	    NULL,
	    compaction_class_init,
	    NULL,
	    NULL,
	    sizeof( CompAction ),
	    0,
	    compaction_instance_init
	};
	type = g_type_register_static( GTK_TYPE_ACTION, "CompActionType", &info, 0 );
    }
    return type;
}


