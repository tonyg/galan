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

#ifndef Prefs_H
#define Prefs_H

extern char *prefs_get_item(const char *key);
extern void prefs_set_item(const char *key, const char *value);
extern void prefs_clear_item(const char *key);

extern void prefs_register_option(const char *key, const char *value);

extern void prefs_edit_prefs(void);

extern void init_prefs(void);
extern void done_prefs(void);

#endif
