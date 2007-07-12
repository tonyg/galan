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

#ifndef Sheet_H
#define Sheet_H

#include "control.h"

typedef struct sheet {

    int sheetmode;
    gdouble saved_x, saved_y;
    ConnectorReference saved_ref;
    ConnectorReference highlight_ref;

    GtkWidget *scrollwin;	    //*< Add this widget to your gui 
    ControlPanel *control_panel;    //*< A field for the ControlPanel fill in yourself
    Control *panel_control;	    //*< The Control for this sheet on another ControlPanel
    gboolean panel_control_active;  //*< TRUE if the panel is a Control at the Moment
    GtkWidget *drawingwidget;	    //*< The widget on which we draw the whole stuff
    GList *components;		    //*< The list of Component s on this sheet
    GList *selected_comps;	    //*< The list of Component s which are selected.
    GList *referring_sheets;	    //*< List of Sheet s which have a reference of this Sheet

    GdkRectangle sel_rect;	    //*< The current selection rectangle
    gboolean sel_valid;		    //*< TRUE is the selection Rectangle should be drawn.

    GeneratorClass *sheetklass;	    //*< This can be removed
    gchar *name;		    //*< the name of this sheet.
    gchar *filename;		    //*< the filename (hopefully or at least NULL)
    gboolean	visible;	    //*< controls if sheet is visible
    gboolean	dirty;		    //*< dirty tag for save question
} Sheet;

extern Sheet *create_sheet();
PUBLIC Sheet *sheet_clone( Sheet *sheet );

extern Component *sheet_build_new_component(Sheet *sheet, ComponentClass *k, gpointer init_data);
extern void sheet_add_component( Sheet *sheet, Component *c );
extern void sheet_delete_component(Sheet *sheet, Component *c);
extern void sheet_queue_redraw_component(Sheet *sheet, Component *c);

extern GdkWindow *sheet_get_window(Sheet *sheet);
extern GdkColor *sheet_get_transparent_color(Sheet *sheet);
extern int sheet_get_textwidth(Sheet *sheet, char *text);

extern void sheet_register_component_class( Sheet *sheet );
extern void sheet_clear(Sheet *sheet);
extern void sheet_remove( Sheet *sheet );
extern void sheet_rename( Sheet *sheet );
extern Sheet *sheet_loadfrom(Sheet *sheet, FILE *f);
extern void sheet_saveto(Sheet *sheet, FILE *f, gboolean sheet_only);
extern Sheet *sheet_unpickle( ObjectStoreItem *item );
extern ObjectStoreItem *sheet_pickle( Sheet *sheet, ObjectStore *db );

extern gboolean sheet_dont_like_be_destroyed( Sheet *sheet );

extern int sheet_get_textwidth(Sheet *sheet, char *text);
extern int sheet_get_textheight(Sheet *sheet, char *text);

extern void sheet_register_ref( Sheet *s, Component *comp );
extern void sheet_unregister_ref( Sheet *s, Component *comp );
extern gboolean sheet_has_refs( Sheet *s );
extern void sheet_kill_refs( Sheet *s );

#endif
