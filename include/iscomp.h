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

#ifndef ISComp_H
#define ISComp_H

#define PIXMAPDIRIFY(filename) \
		(SITE_PKGDATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S filename)


typedef struct ISCompData ISCompData;
typedef struct ISCompInitData ISCompInitData;

typedef enum RefType {
    SIGIN, SIGOUT, EVTIN, EVTOUT
} RefType;

struct ISCompData {
    gchar *name;
    gboolean poly;
    RefType reftype;
    ConnectorReference *ref;
};

struct ISCompInitData {
    RefType reftype;
    gboolean poly;
};

extern void init_iscomp(void);
extern void done_iscomp(void);

#endif
