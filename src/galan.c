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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "plugin.h"
#include "gui.h"
#include "comp.h"
#include "gencomp.h"
#include "control.h"
#include "prefs.h"

PUBLIC char *safe_string_dup(char *str) {
  char *n;

  if (str == NULL)
    return NULL;

  n = safe_malloc(strlen(str) + 1);
  strcpy(n, str);
  return n;
}

PUBLIC void *safe_malloc(size_t size) {
  void *result = malloc(size);

  if (result == NULL)
    g_error("Out of memory mallocing %d bytes...", (int) size);

  return result;
}

PUBLIC void *safe_calloc(int nelems, size_t size) {
  void *result = calloc(nelems, size);

  if (result == NULL)
    g_error("Out of memory callocing %d bytes...", (int) size);

  return result;
}

/* Called by main() in main.c */
PUBLIC int galan_main(int argc, char *argv[]) {
  gtk_set_locale();
  gtk_init(&argc, &argv);

  init_generator();
  init_clock();
  init_prefs();
  init_gui();
  init_control();
  init_comp();
  init_gencomp();
  init_objectstore();
  init_plugins();

  gtk_main();

  done_objectstore();
  done_gencomp();
  done_comp();
  done_gui();
  done_prefs();
  done_clock();
  done_generator();

  return EXIT_SUCCESS;
}
