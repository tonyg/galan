/* Generated by GOB (v2.0.12)   (do not edit directly) */

#include <glib.h>
#include <glib-object.h>


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
#include "gui.h"


#ifndef __GALAN_COMPACTION_H__
#define __GALAN_COMPACTION_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Type checking and casting macros
 */
#define GALAN_TYPE_COMPACTION	(galan_compaction_get_type())
#define GALAN_COMPACTION(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), galan_compaction_get_type(), GalanCompAction)
#define GALAN_COMPACTION_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), galan_compaction_get_type(), GalanCompAction const)
#define GALAN_COMPACTION_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), galan_compaction_get_type(), GalanCompActionClass)
#define GALAN_IS_COMPACTION(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), galan_compaction_get_type ())

#define GALAN_COMPACTION_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), galan_compaction_get_type(), GalanCompActionClass)

/*
 * Main object structure
 */
#ifndef __TYPEDEF_GALAN_COMPACTION__
#define __TYPEDEF_GALAN_COMPACTION__
typedef struct _GalanCompAction GalanCompAction;
#endif
struct _GalanCompAction {
	GtkAction __parent__;
	/*< public >*/
	ComponentClass * klass;
	gpointer init_data;
	GtkWidget * dragwidget;
};

/*
 * Class definition
 */
typedef struct _GalanCompActionClass GalanCompActionClass;
struct _GalanCompActionClass {
	GtkActionClass __parent__;
};


/*
 * Public methods
 */
GType	galan_compaction_get_type	(void);
void 	galan_compaction_create_comp	(GalanCompAction * self,
					Sheet * s,
					int x,
					int y);

/*
 * Argument wrapping macros
 */
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define GALAN_COMPACTION_PROP_KLASS(arg)    	"klass", __extension__ ({gpointer z = (arg); z;})
#define GALAN_COMPACTION_GET_PROP_KLASS(arg)	"klass", __extension__ ({gpointer *z = (arg); z;})
#define GALAN_COMPACTION_PROP_INIT_DATA(arg)    	"init_data", __extension__ ({gpointer z = (arg); z;})
#define GALAN_COMPACTION_GET_PROP_INIT_DATA(arg)	"init_data", __extension__ ({gpointer *z = (arg); z;})
#else /* __GNUC__ && !__STRICT_ANSI__ */
#define GALAN_COMPACTION_PROP_KLASS(arg)    	"klass",(gpointer )(arg)
#define GALAN_COMPACTION_GET_PROP_KLASS(arg)	"klass",(gpointer *)(arg)
#define GALAN_COMPACTION_PROP_INIT_DATA(arg)    	"init_data",(gpointer )(arg)
#define GALAN_COMPACTION_GET_PROP_INIT_DATA(arg)	"init_data",(gpointer *)(arg)
#endif /* __GNUC__ && !__STRICT_ANSI__ */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
