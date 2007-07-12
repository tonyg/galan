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
#include <glib.h>

#include "global.h"
#include "generator.h"
#include "plugin.h"
#include "gui.h"
#include "comp.h"
#include "gencomp.h"
#include "iscomp.h"
#include "cocomp.h"
#include "sheet.h"
#include "shcomp.h"
#include "control.h"
#include "prefs.h"
#include "galan_jack.h"
#include "galan_lash.h"

//PRIVATE GStaticMutex malloc_mutex = G_STATIC_MUTEX_INIT;
PUBLIC GThread *main_thread;

PUBLIC char *safe_string_dup(const char *str) {
  char *n;

  if (str == NULL)
    return NULL;

  n = safe_malloc(strlen(str) + 1);
  strcpy(n, str);
  return n;
}

PUBLIC void *safe_malloc(size_t size) {
    void *result;

    //g_static_mutex_lock( &malloc_mutex );

    result = malloc(size);

    //printf( "malloc size = %d\n", size );
    //printf( "malloc\n" ); 
    //g_static_mutex_unlock( &malloc_mutex );


    if (result == NULL)
	g_error("Out of memory mallocing %d bytes...", (int) size);

    return result;
}

PUBLIC void *safe_calloc(int nelems, size_t size) {
    void *result;

    //g_static_mutex_lock( &malloc_mutex );

    result = calloc(nelems, size);

    //g_static_mutex_unlock( &malloc_mutex );


    if (result == NULL)
	g_error("Out of memory callocing %d bytes...", (int) size);

    return result;
}

PUBLIC void *safe_realloc( gpointer mem, gsize nbytes ) {
    
    void *result;

    //g_static_mutex_lock( &malloc_mutex );

    result = realloc(mem, nbytes);

    //g_static_mutex_unlock( &malloc_mutex );

    return result;
}

PUBLIC void safe_free( void *ptr ) {
    //g_static_mutex_lock( &malloc_mutex );

    free(ptr);

    //g_static_mutex_unlock( &malloc_mutex );
}

PUBLIC void lock_malloc_lock( void ) {
    //g_static_mutex_lock( &malloc_mutex );
}

PUBLIC void unlock_malloc_lock( void ) {
    //g_static_mutex_unlock( &malloc_mutex );
}

/* Called by main() in main.c */
PUBLIC int galan_main(int argc, char *argv[]) {

    //GMemVTable vtable = { safe_malloc, safe_realloc, safe_free, NULL, NULL, NULL };
    //g_mem_set_vtable( &vtable );
  g_thread_init(NULL);
  
  gdk_threads_init();
  main_thread = g_thread_self();
  gtk_set_locale();
  gtk_init(&argc, &argv);
  gdk_rgb_init();

  gtk_rc_parse_string( "style \"trans\" { bg_pixmap[NORMAL] = \"<parent>\" } \nwidget \"control_panel.*.GtkLayout.*\" style \"trans\" " );

  init_jack();
  init_lash( argc, argv );
  init_generator();
  init_event();
  init_clock();
  init_control();
  init_gui();
  init_comp();
  init_gencomp();
  init_iscomp();
  init_cocomp();
  init_shcomp();
  init_prefs();
  init_objectstore();
  init_plugins();

  gui_add_comps();
  
  init_generator_thread();
  init_control_thread();

  if( argc > 1 )
      if( ! g_path_is_absolute( argv[1] ) ) {
	  char *cwd = g_get_current_dir();
	  char *absname = g_build_filename( cwd, argv[1], NULL );
	  load_sheet_from_name( absname );
	  free( absname );
	  free( cwd );
      } else
	  load_sheet_from_name( argv[1] );
  else {
      Sheet *s = create_sheet();
      s->control_panel = control_panel_new( s->name, TRUE, s );
      gui_register_sheet( s );
  }



  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  done_objectstore();
  done_prefs();
  done_shcomp();
  done_iscomp();
  done_cocomp();
  done_gencomp();
  done_comp();
  done_gui();
  done_clock();
  done_generator();
  done_lash();
  done_jack();

  return EXIT_SUCCESS;
}
