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

#ifndef Buffer_H
#define Buffer_H

typedef struct Buffer {
    int buflength;
    int pos;
    char *buf;
} Buffer, *BUFFER;

extern BUFFER newbuf(int initial_length);
extern BUFFER dupbuf(BUFFER buf);
extern void killbuf(BUFFER buf);
extern void buf_append(BUFFER buf, char ch);
extern void buf_insert(BUFFER buf, char ch, int pos);
extern void buf_delete(BUFFER buf, int pos);

#endif
