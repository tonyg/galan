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

#ifndef ShComp_H
#define ShComp_H

#define PIXMAPDIRIFY(filename) \
		(SITE_PKGDATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S filename)


typedef struct ShCompData ShCompData;
typedef struct ShCompInitData ShCompInitData;
typedef struct FileShCompInitData FileShCompInitData;

typedef struct InterSheetLinks {
    GList *inputevents, *outputevents, *inputsignals, *outputsignals;
    int anzinputevents, anzoutputevents, anzinputsignals, anzoutputsignals;
} InterSheetLinks;

struct ShCompData {
    Sheet *sheet;
    InterSheetLinks isl;
};

struct ShCompInitData {
    Sheet *sheet;
};

struct FileShCompInitData {
    char *filename;
};

extern void sheet_set_load_hidden( gboolean v );
extern void shcomp_register_sheet( Sheet *sheet );
extern void shcomp_resize( Component *c );
extern void init_shcomp(void);
extern void done_shcomp(void);

#endif
