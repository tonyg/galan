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

#ifndef GenComp_H
#define GenComp_H

#define PIXMAPDIRIFY(filename) \
		(SITE_PKGDATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S filename)

typedef struct GenCompData GenCompData;
typedef struct GenCompInitData GenCompInitData;
typedef void (*PropertiesCallback)(Component *c, Generator *g);

struct GenCompData {
  Generator *g;
  GdkPixmap *icon;
  PropertiesCallback propgen;
};

struct GenCompInitData {
  GeneratorClass *k;
  char *iconpath;
  PropertiesCallback propgen;
};

extern ComponentClass GeneratorComponentClass;
extern void gencomp_register_generatorclass(GeneratorClass *k, gboolean prefer,
					    char *menupath, char *iconpath,
					    PropertiesCallback propgen);

extern void init_gencomp(void);
extern void done_gencomp(void);

#endif
