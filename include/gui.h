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

#ifndef Gui_H
#define Gui_H

struct sheet;
extern void gui_register_sheet( struct sheet *sheet );
extern void gui_unregister_sheet( struct sheet *sheet );
extern GList *get_sheet_list( void );
extern void update_sheet_name( struct sheet *sheet );
extern void gui_statusbar_push( char *msg );

extern void init_gui(void);
extern void done_gui(void);

#endif
