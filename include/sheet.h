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

    GtkWidget *scrollwin;
    ControlPanel *control_panel;
    Control *panel_control;
    gboolean panel_control_active;
    GtkWidget *drawingwidget;
    GList *components;
    GList *referring_sheets;

    GeneratorClass *sheetklass;
    gchar *name;
} Sheet;

extern Sheet *create_sheet();

extern void sheet_build_new_component(Sheet *sheet, ComponentClass *k, gpointer init_data);
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

extern void sheet_register_ref( Sheet *s, Component *comp );
extern void sheet_unregister_ref( Sheet *s, Component *comp );
extern gboolean sheet_has_refs( Sheet *s );
extern void sheet_kill_refs( Sheet *s );

#endif
