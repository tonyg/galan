/* Generated by GOB (v2.0.12) on Sat Feb  7 18:47:28 2009
   (do not edit directly) */

/* End world hunger, donate to the World Food Programme, http://www.wfp.org */

#define GOB_VERSION_MAJOR 2
#define GOB_VERSION_MINOR 0
#define GOB_VERSION_PATCHLEVEL 12

#define selfp (self->_priv)

#include <string.h> /* memset() */

#include "galan-compaction.h"

#ifdef G_LIKELY
#define ___GOB_LIKELY(expr) G_LIKELY(expr)
#define ___GOB_UNLIKELY(expr) G_UNLIKELY(expr)
#else /* ! G_LIKELY */
#define ___GOB_LIKELY(expr) (expr)
#define ___GOB_UNLIKELY(expr) (expr)
#endif /* G_LIKELY */

#line 21 "galan-compaction.gob"

static GtkTargetEntry targette = { "galan/CompAction", 0, 234 };

#line 29 "galan-compaction.c"
/* self casting macros */
#define SELF(x) GALAN_COMPACTION(x)
#define SELF_CONST(x) GALAN_COMPACTION_CONST(x)
#define IS_SELF(x) GALAN_IS_COMPACTION(x)
#define TYPE_SELF GALAN_TYPE_COMPACTION
#define SELF_CLASS(x) GALAN_COMPACTION_CLASS(x)

#define SELF_GET_CLASS(x) GALAN_COMPACTION_GET_CLASS(x)

/* self typedefs */
typedef GalanCompAction Self;
typedef GalanCompActionClass SelfClass;

/* here are local prototypes */
static void ___object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void ___object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void galan_compaction_init (GalanCompAction * o) G_GNUC_UNUSED;
static void galan_compaction_class_init (GalanCompActionClass * c) G_GNUC_UNUSED;
static void galan_compaction_drag_data_get (GalanCompAction * self, GdkDragContext * context, GtkSelectionData * data, guint info, guint time, GtkWidget * w) G_GNUC_UNUSED;
static void galan_compaction_drag_data_received (GalanCompAction * self, GdkDragContext * context, gint x, gint y, GtkSelectionData * data, guint info, guint time, GtkWidget * w) G_GNUC_UNUSED;
static void galan_compaction_drag_begin (GalanCompAction * self, GdkDragContext * context, GtkWidget * w) G_GNUC_UNUSED;
static void galan_compaction_drag_end (GalanCompAction * self, GdkDragContext * context, GtkWidget * w) G_GNUC_UNUSED;
static void ___6_galan_compaction_activate (GtkAction * self) G_GNUC_UNUSED;
static GtkWidget * ___7_galan_compaction_create_menu_item (GtkAction * self) G_GNUC_UNUSED;
static GtkWidget * ___8_galan_compaction_create_tool_item (GtkAction * self) G_GNUC_UNUSED;

enum {
	PROP_0,
	PROP_KLASS,
	PROP_INIT_DATA
};

/* pointer to the class of our parent */
static GtkActionClass *parent_class = NULL;

/* Short form macros */
#define self_drag_data_get galan_compaction_drag_data_get
#define self_drag_data_received galan_compaction_drag_data_received
#define self_drag_begin galan_compaction_drag_begin
#define self_drag_end galan_compaction_drag_end
#define self_create_comp galan_compaction_create_comp
GType
galan_compaction_get_type (void)
{
	static GType type = 0;

	if ___GOB_UNLIKELY(type == 0) {
		static const GTypeInfo info = {
			sizeof (GalanCompActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) galan_compaction_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GalanCompAction),
			0 /* n_preallocs */,
			(GInstanceInitFunc) galan_compaction_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ACTION, "GalanCompAction", &info, (GTypeFlags)0);
	}

	return type;
}

/* a macro for creating a new object of our type */
#define GET_NEW ((GalanCompAction *)g_object_new(galan_compaction_get_type(), NULL))

/* a function for creating a new object of our type */
#include <stdarg.h>
static GalanCompAction * GET_NEW_VARG (const char *first, ...) G_GNUC_UNUSED;
static GalanCompAction *
GET_NEW_VARG (const char *first, ...)
{
	GalanCompAction *ret;
	va_list ap;
	va_start (ap, first);
	ret = (GalanCompAction *)g_object_new_valist (galan_compaction_get_type (), first, ap);
	va_end (ap);
	return ret;
}

static void 
galan_compaction_init (GalanCompAction * o G_GNUC_UNUSED)
{
#define __GOB_FUNCTION__ "Galan:CompAction::init"
}
#undef __GOB_FUNCTION__
static void 
galan_compaction_class_init (GalanCompActionClass * c G_GNUC_UNUSED)
{
#define __GOB_FUNCTION__ "Galan:CompAction::class_init"
	GObjectClass *g_object_class G_GNUC_UNUSED = (GObjectClass*) c;
	GtkActionClass *gtk_action_class = (GtkActionClass *)c;

	parent_class = g_type_class_ref (GTK_TYPE_ACTION);

#line 81 "galan-compaction.gob"
	gtk_action_class->activate = ___6_galan_compaction_activate;
#line 88 "galan-compaction.gob"
	gtk_action_class->create_menu_item = ___7_galan_compaction_create_menu_item;
#line 104 "galan-compaction.gob"
	gtk_action_class->create_tool_item = ___8_galan_compaction_create_tool_item;
#line 134 "galan-compaction.c"
	g_object_class->get_property = ___object_get_property;
	g_object_class->set_property = ___object_set_property;
    {
	GParamSpec   *param_spec;

	param_spec = g_param_spec_pointer
		("klass" /* name */,
		 "klass" /* nick */,
		 "ComponentClass *" /* blurb */,
		 (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
		PROP_KLASS,
		param_spec);
	param_spec = g_param_spec_pointer
		("init_data" /* name */,
		 "id" /* nick */,
		 "InitData gpointer" /* blurb */,
		 (GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
		PROP_INIT_DATA,
		param_spec);
    }
}
#undef __GOB_FUNCTION__

static void
___object_set_property (GObject *object,
	guint property_id,
	const GValue *VAL G_GNUC_UNUSED,
	GParamSpec *pspec G_GNUC_UNUSED)
#define __GOB_FUNCTION__ "Galan:CompAction::set_property"
{
	GalanCompAction *self G_GNUC_UNUSED;

	self = GALAN_COMPACTION (object);

	switch (property_id) {
	case PROP_KLASS:
		{
#line 31 "galan-compaction.gob"
self->klass = g_value_get_pointer (VAL);
#line 176 "galan-compaction.c"
		}
		break;
	case PROP_INIT_DATA:
		{
#line 37 "galan-compaction.gob"
self->init_data = g_value_get_pointer (VAL);
#line 183 "galan-compaction.c"
		}
		break;
	default:
/* Apparently in g++ this is needed, glib is b0rk */
#ifndef __PRETTY_FUNCTION__
#  undef G_STRLOC
#  define G_STRLOC	__FILE__ ":" G_STRINGIFY (__LINE__)
#endif
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
#undef __GOB_FUNCTION__

static void
___object_get_property (GObject *object,
	guint property_id,
	GValue *VAL G_GNUC_UNUSED,
	GParamSpec *pspec G_GNUC_UNUSED)
#define __GOB_FUNCTION__ "Galan:CompAction::get_property"
{
	GalanCompAction *self G_GNUC_UNUSED;

	self = GALAN_COMPACTION (object);

	switch (property_id) {
	case PROP_KLASS:
		{
#line 31 "galan-compaction.gob"
g_value_set_pointer (VAL, self->klass);
#line 214 "galan-compaction.c"
		}
		break;
	case PROP_INIT_DATA:
		{
#line 37 "galan-compaction.gob"
g_value_set_pointer (VAL, self->init_data);
#line 221 "galan-compaction.c"
		}
		break;
	default:
/* Apparently in g++ this is needed, glib is b0rk */
#ifndef __PRETTY_FUNCTION__
#  undef G_STRLOC
#  define G_STRLOC	__FILE__ ":" G_STRINGIFY (__LINE__)
#endif
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
#undef __GOB_FUNCTION__



#line 43 "galan-compaction.gob"
static void 
galan_compaction_drag_data_get (GalanCompAction * self, GdkDragContext * context, GtkSelectionData * data, guint info, guint time, GtkWidget * w)
#line 241 "galan-compaction.c"
{
#define __GOB_FUNCTION__ "Galan:CompAction::drag_data_get"
#line 43 "galan-compaction.gob"
	g_return_if_fail (self != NULL);
#line 43 "galan-compaction.gob"
	g_return_if_fail (GALAN_IS_COMPACTION (self));
#line 248 "galan-compaction.c"
{
#line 43 "galan-compaction.gob"
	
		Self *s = self;
		//gpointer format = g_list_nth_data( context->targets, 0 );
		//GdkAtom format = gtk_drag_dest_find_target( w, context, NULL );

		gtk_selection_data_set( data, data->target, 8, (gpointer) &s, sizeof( Self * ) );
	}}
#line 258 "galan-compaction.c"
#undef __GOB_FUNCTION__

#line 51 "galan-compaction.gob"
static void 
galan_compaction_drag_data_received (GalanCompAction * self, GdkDragContext * context, gint x, gint y, GtkSelectionData * data, guint info, guint time, GtkWidget * w)
#line 264 "galan-compaction.c"
{
#define __GOB_FUNCTION__ "Galan:CompAction::drag_data_received"
#line 51 "galan-compaction.gob"
	g_return_if_fail (self != NULL);
#line 51 "galan-compaction.gob"
	g_return_if_fail (GALAN_IS_COMPACTION (self));
#line 271 "galan-compaction.c"
{
#line 51 "galan-compaction.gob"
	
		
		//Self *other = SELF( data->data );
	
	}}
#line 279 "galan-compaction.c"
#undef __GOB_FUNCTION__

#line 57 "galan-compaction.gob"
static void 
galan_compaction_drag_begin (GalanCompAction * self, GdkDragContext * context, GtkWidget * w)
#line 285 "galan-compaction.c"
{
#define __GOB_FUNCTION__ "Galan:CompAction::drag_begin"
#line 57 "galan-compaction.gob"
	g_return_if_fail (self != NULL);
#line 57 "galan-compaction.gob"
	g_return_if_fail (GALAN_IS_COMPACTION (self));
#line 292 "galan-compaction.c"
{
#line 57 "galan-compaction.gob"
	
		self->dragwidget = g_object_new( GTK_TYPE_WINDOW,
			"accept-focus", FALSE,
			"focus-on-map", FALSE,
			"decorated", FALSE,
			"type", GTK_WINDOW_TOPLEVEL,
			"default-width", 30,
			"default-height", 30,
			NULL );
		
		gtk_container_add( GTK_CONTAINER( self->dragwidget ), gtk_label_new( "Drag" ) );
		gtk_drag_set_icon_widget( context, self->dragwidget, -10, -10 );
		gtk_widget_show_all( self->dragwidget );
	}}
#line 309 "galan-compaction.c"
#undef __GOB_FUNCTION__

#line 71 "galan-compaction.gob"
static void 
galan_compaction_drag_end (GalanCompAction * self, GdkDragContext * context, GtkWidget * w)
#line 315 "galan-compaction.c"
{
#define __GOB_FUNCTION__ "Galan:CompAction::drag_end"
#line 71 "galan-compaction.gob"
	g_return_if_fail (self != NULL);
#line 71 "galan-compaction.gob"
	g_return_if_fail (GALAN_IS_COMPACTION (self));
#line 322 "galan-compaction.c"
{
#line 71 "galan-compaction.gob"
	
		gtk_object_destroy( GTK_OBJECT(self->dragwidget) );
	}}
#line 328 "galan-compaction.c"
#undef __GOB_FUNCTION__

#line 75 "galan-compaction.gob"
void 
galan_compaction_create_comp (GalanCompAction * self, Sheet * s, int x, int y)
#line 334 "galan-compaction.c"
{
#define __GOB_FUNCTION__ "Galan:CompAction::create_comp"
#line 75 "galan-compaction.gob"
	g_return_if_fail (self != NULL);
#line 75 "galan-compaction.gob"
	g_return_if_fail (GALAN_IS_COMPACTION (self));
#line 341 "galan-compaction.c"
{
#line 75 "galan-compaction.gob"
	
		s->saved_x = x;
		s->saved_y = y;
    		sheet_build_new_component( s, self->klass, self->init_data );
	}}
#line 349 "galan-compaction.c"
#undef __GOB_FUNCTION__

#line 81 "galan-compaction.gob"
static void 
___6_galan_compaction_activate (GtkAction * self G_GNUC_UNUSED)
#line 355 "galan-compaction.c"
#define PARENT_HANDLER(___self) \
	{ if(GTK_ACTION_CLASS(parent_class)->activate) \
		(* GTK_ACTION_CLASS(parent_class)->activate)(___self); }
{
#define __GOB_FUNCTION__ "Galan:CompAction::activate"
{
#line 81 "galan-compaction.gob"
	

		Self  *ich = SELF( self );
    		Sheet *s = gui_get_active_sheet();
    		sheet_build_new_component( s, ich->klass, ich->init_data );
	}}
#line 369 "galan-compaction.c"
#undef __GOB_FUNCTION__
#undef PARENT_HANDLER

#line 88 "galan-compaction.gob"
static GtkWidget * 
___7_galan_compaction_create_menu_item (GtkAction * self G_GNUC_UNUSED)
#line 376 "galan-compaction.c"
#define PARENT_HANDLER(___self) \
	((GTK_ACTION_CLASS(parent_class)->create_menu_item)? \
		(* GTK_ACTION_CLASS(parent_class)->create_menu_item)(___self): \
		((GtkWidget * )0))
{
#define __GOB_FUNCTION__ "Galan:CompAction::create_menu_item"
{
#line 88 "galan-compaction.gob"
	

		Self *ich = SELF( self );
		GtkWidget *ret = PARENT_HANDLER( self );

		gtk_drag_source_set( ret, GDK_BUTTON1_MASK, &targette, 1, GDK_ACTION_COPY );
		g_signal_connect_swapped( ret, "drag_data_get", G_CALLBACK(self_drag_data_get), ich );
		g_signal_connect_swapped( ret, "drag_begin", G_CALLBACK(self_drag_begin), ich );
		g_signal_connect_swapped( ret, "drag_end", G_CALLBACK(self_drag_end), ich );

		gtk_drag_dest_set( ret, GTK_DEST_DEFAULT_DROP, &targette, 1, GDK_ACTION_COPY );
		g_signal_connect_swapped( ret, "drag_data_received", G_CALLBACK(self_drag_data_received), ich );

		return ret;
	}}
#line 400 "galan-compaction.c"
#undef __GOB_FUNCTION__
#undef PARENT_HANDLER

#line 104 "galan-compaction.gob"
static GtkWidget * 
___8_galan_compaction_create_tool_item (GtkAction * self G_GNUC_UNUSED)
#line 407 "galan-compaction.c"
#define PARENT_HANDLER(___self) \
	((GTK_ACTION_CLASS(parent_class)->create_tool_item)? \
		(* GTK_ACTION_CLASS(parent_class)->create_tool_item)(___self): \
		((GtkWidget * )0))
{
#define __GOB_FUNCTION__ "Galan:CompAction::create_tool_item"
{
#line 104 "galan-compaction.gob"
	

		Self *ich = SELF(self);
		GtkWidget *ret = PARENT_HANDLER( self );

		gtk_drag_source_set( ret, GDK_BUTTON1_MASK, &targette, 1, GDK_ACTION_COPY );
		g_signal_connect_swapped( ret, "drag_data_get", G_CALLBACK(self_drag_data_get), ich );

		gtk_drag_dest_set( ret, GTK_DEST_DEFAULT_DROP, &targette, 1, GDK_ACTION_COPY );
		g_signal_connect_swapped( ret, "drag_data_received", G_CALLBACK(self_drag_data_received), ich );

		return ret;
	}}
#line 429 "galan-compaction.c"
#undef __GOB_FUNCTION__
#undef PARENT_HANDLER
