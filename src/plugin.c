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
#include <string.h>
#include <stdio.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "gmodule.h"
#include "msgbox.h"

/* %%% Win32: dirent.h seems to conflict with glib-1.3, so ignore dirent.h */
#ifndef NATIVE_WIN32
#include <dirent.h>
#endif

/* On Win32, these headers seem to need to follow glib.h */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define HOMEDIR_SUFFIX	"/.galan/plugins"
#define INITFUNC_PREFIX "init_plugin_"

PRIVATE void load_plugin(char *plugin, char *leafname) {
  GModule *handle = g_module_open(plugin, 0);
  void (*initializer)(void);
  char *initstr;
  char *dotpos;

  if (handle == NULL) {
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	  "g_module_open(%s, 0) failed: %s", plugin, g_module_error());
    return;
  }

  initstr = malloc(strlen(INITFUNC_PREFIX) + strlen(leafname) + 1);
  strcpy(initstr, INITFUNC_PREFIX);
  strcat(initstr, leafname);

  /* Strip file suffix */
  dotpos = strrchr(initstr, '.');
  if (dotpos != NULL)
    *dotpos = '\0';

  if (!g_module_symbol(handle, initstr, (gpointer *) &initializer) ) {
    popup_msgbox("Plugin Error", MSGBOX_OK, 0, MSGBOX_OK,
		 "Plugin %s has no accessible initializer.\n"
		 "This is most likely a bug in the plugin.\n"
		 "Please report this to the author of the *PLUGIN*.",
		 leafname);
    g_message("Error finding initializer for plugin %s", leafname);
    free(initstr);
    g_module_close(handle);
    return;
  }

  free(initstr);
  initializer();

  /* Don't g_module_close(handle) because it will unload the .so */
}

PRIVATE void load_all_plugins(char *dir);	/* forward decl */

PRIVATE int check_plugin_validity(char *name) {
  struct stat sb;

#ifdef G_OS_WIN32
  if( strcmp(name+(strlen(name)-4), ".dll" ) )
    return 0;
#else
  if( strcmp(name+(strlen(name)-3), ".so" ) )
    return 0;
#endif

  if (stat(name, &sb) == -1)
    return 0;

  if (S_ISDIR(sb.st_mode))
    load_all_plugins(name);

  return S_ISREG(sb.st_mode);
}

PRIVATE void load_all_plugins(char *dir) {
  DIR *d = opendir(dir);
  struct dirent *de;

  if (d == NULL)
    /* the plugin directory cannot be read */
    return;

  while ((de = readdir(d)) != NULL) {
    char *fullname;

    if (de->d_name[0] == '.')
      /* Don't load 'hidden' files or directories */
      continue;

    fullname = safe_malloc(strlen(dir) + strlen(de->d_name) + 2);	/* "/" and the NUL byte */

    strcpy(fullname, dir);
    strcat(fullname, G_DIR_SEPARATOR_S);
    strcat(fullname, de->d_name);

    if (check_plugin_validity(fullname))
      load_plugin(fullname, de->d_name);

    free(fullname);
  }

  closedir(d);
}

PUBLIC void init_plugins(void) {
  char *home;
  char *plugindir = getenv("GALAN_PLUGIN_DIR");

  if (plugindir != NULL)
    load_all_plugins(plugindir);
  else
    load_all_plugins(SITE_PKGLIB_DIR G_DIR_SEPARATOR_S "plugins");

  home = getenv("HOME");
  if (home != NULL) {
    char *userdir = safe_malloc(strlen(home) + strlen(HOMEDIR_SUFFIX) + 1);

    strcpy(userdir, home);
    strcat(userdir, HOMEDIR_SUFFIX);
    load_all_plugins(userdir);
    free(userdir);
  }
}
