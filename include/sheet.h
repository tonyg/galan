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

extern GtkWidget *create_sheet(GtkBox *parent);

extern void sheet_build_new_component(ComponentClass *k, gpointer init_data);
extern void sheet_delete_component(Component *c);
extern void sheet_queue_redraw_component(Component *c);

extern GdkWindow *sheet_get_window(void);
extern GdkColor *sheet_get_transparent_color(void);
extern int sheet_get_textwidth(char *text);

extern void sheet_clear(void);
extern gboolean sheet_loadfrom(FILE *f);
extern void sheet_saveto(FILE *f);

#endif
