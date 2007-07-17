
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

#ifndef COMP_ACTION_CLASS_H
#define COMP_ACTION_CLASS_H


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


#define COMPACTION_TYPE (compaction_get_type())
#define COMPACTION(obj)	      (G_TYPE_CHECK_INSTANCE_CAST ((obj), COMPACTION_TYPE, CompAction))
#define COMPACTION_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), COMPACTION_TYPE, CompActionClass))
#define IS_COMPACTION(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COMPACTION_TYPE))
#define IS_COMPACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), COMPACTION_TYPE))
#define COMPACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), COMPACTION_TYPE, CompActionClass))

typedef struct _CompAction CompAction;
typedef struct _CompActionClass CompActionClass;

struct _CompAction {
      GtkAction parent;
        /* instance members */
      ComponentClass *k;
      gpointer init_data;
};

struct _CompActionClass {
      GtkActionClass parent;

        /* class members */
};


extern GType compaction_get_type( void );



#endif
