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
#include "gui.h"
#include "comp.h"
#include "sheet.h"
#include "msgbox.h"
#include "control.h"
#include "prefs.h"
#include "shcomp.h"

#include "galan-comptree-model.h"

/*=======================================================================*/
/* Variables for GTK gui */

PRIVATE GtkWidget *mainwin;
PRIVATE GtkWidget *mainmenu;
PRIVATE GtkWidget *mainnotebook;
PRIVATE GtkWidget *statusbar;
PRIVATE  GtkWidget *upper_box;

PUBLIC GtkActionGroup *component_actiongroup = NULL;
PUBLIC GtkActionGroup *menu_actiongroup = NULL;
PUBLIC  GtkUIManager *ui_manager;

PRIVATE char *current_filename = NULL;
PRIVATE guint timeout_tag;
PRIVATE AClock *gui_default_clock = NULL;

PRIVATE GList *sheets = NULL;

/*=======================================================================*/
/* Helper Functions */

PUBLIC Sheet *gui_get_active_sheet( void ) {

    int activepagenum = gtk_notebook_get_current_page( GTK_NOTEBOOK(mainnotebook) );
    GtkWidget *scrollwin = gtk_notebook_get_nth_page( GTK_NOTEBOOK(mainnotebook), activepagenum );
    Sheet *s = gtk_object_get_user_data( GTK_OBJECT( scrollwin ) );

    return s;
}

PUBLIC GList *get_sheet_list( void ) {
    return sheets;
}

PUBLIC void gui_register_sheet( struct sheet *sheet ) {

    if( sheet->visible )
	gtk_notebook_append_page( GTK_NOTEBOOK( mainnotebook ), sheet->scrollwin, gtk_label_new( sheet->name ) );

    sheets = g_list_append( sheets, sheet );
}

PUBLIC void gui_unregister_sheet( struct sheet *sheet ) {

    int pagenum;

    sheets = g_list_remove( sheets, sheet );

    if( sheet->visible ) {
	pagenum = gtk_notebook_page_num( GTK_NOTEBOOK( mainnotebook ), sheet->scrollwin );
	gtk_notebook_remove_page( GTK_NOTEBOOK( mainnotebook ), pagenum );
    }
}

PUBLIC void update_sheet_name( struct sheet *sheet ) {
    if( sheet->dirty ) {
	char *dirty_name = g_strdup_printf( "%s [+]", sheet->name );
	gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( mainnotebook ), sheet->scrollwin, dirty_name );
    } else {
	gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( mainnotebook ), sheet->scrollwin, sheet->name );
    }
}

PUBLIC void gui_statusbar_push( char *msg ) {
    gtk_label_set_text( GTK_LABEL(statusbar), msg );
}

PUBLIC gboolean anything_dirty(void) {

    GList *shlist;
    for( shlist = get_sheet_list(); shlist; shlist=g_list_next(shlist) ) {
	Sheet *s = shlist->data;
	if( sheet_dont_like_be_destroyed(s) )
	    return TRUE;
    }
    return FALSE;
}


/*=======================================================================*/
/* GTK gui Callbacks */

PRIVATE gboolean exit_request(GtkWidget *w, GdkEvent *ev, gpointer data) {
  /* %%% should check that the file is saved here */

    if( anything_dirty() ) {
	return TRUE;
    }
    
  gtk_main_quit();
  return TRUE;
}

PRIVATE void about_callback(void) {
  popup_msgbox("About gAlan", MSGBOX_OK, 0, MSGBOX_OK,
	       "gAlan %s\n"
	       "Copyright Tony Garnock-Jones (C) 1999\n"
	       "Copyright Torben Hohn (C) 2002\n"
	       "A modular sound-processing tool\n(Graphical Audio LANguage)\n"
	       "\n"
	       "gAlan comes with ABSOLUTELY NO WARRANTY; for details, see the file\n"
	       "\"COPYING\" that came with the gAlan distribution.\n"
	       "This is free software, distributed under the GNU General Public\n"
	       "License. Please see \"COPYING\" or http://www.gnu.org/copyleft/gpl.txt\n"
	       "\n"
	       "SITE_PKGLIB_DIR = %s\n"
	       "SITE_PKGDATA_DIR = %s\n"
	       "\n"
	       "NOTE: This is BETA software\n",
	       VERSION,
	       SITE_PKGLIB_DIR,
	       SITE_PKGDATA_DIR
	       );
}

PRIVATE void clear_sheet(gpointer userdata, guint action, GtkWidget *widget) {
  sheet_clear( gui_get_active_sheet() );
}

PRIVATE void unconnect_sheet(gpointer userdata, guint action, GtkWidget *widget) {
  sheet_kill_refs( gui_get_active_sheet() );
}

PRIVATE void file_new_callback(gpointer userdata, guint action, GtkWidget *widget) {

    GList *sheetX = get_sheet_list();
    Sheet *s1;

    while( sheetX != NULL ) {
	GList *temp = g_list_next( sheetX );
	Sheet *s = sheetX->data;

	sheet_remove( s );
	sheetX = temp;
    }

    s1 = create_sheet( );
    s1->control_panel = control_panel_new( s1->name, TRUE, s1 );
    gui_register_sheet( s1 );
}

PUBLIC void load_sheet_from_name(char *name) {
  FILE *f;
  Sheet *s;

  f = fopen(name, "rb");

      
  if (f == NULL || !(s=sheet_loadfrom( NULL , f))) {
    popup_msgbox("Error Loading File", MSGBOX_OK, 120000, MSGBOX_OK,
		 "File not found, or file format error: %s",
		 name);
    return;
  }

  fclose(f);

  s->filename = safe_string_dup( name );

  if (current_filename != NULL)
    free(current_filename);
  current_filename = safe_string_dup(name);
}

PRIVATE void load_new_sheet(GtkWidget *widget, GtkWidget *fs) {
  FILE *f;
  Sheet *s;
  const char *newname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  f = fopen(newname, "rb");

      
  if (f == NULL || !(s=sheet_loadfrom( NULL , f))) {
    popup_msgbox("Error Loading File", MSGBOX_OK, 120000, MSGBOX_OK,
		 "File not found, or file format error: %s",
		 newname);
    return;
  }

  fclose(f);

  s->filename = safe_string_dup( newname );

  if (current_filename != NULL)
    free(current_filename);
  current_filename = safe_string_dup(newname);
  gtk_widget_destroy(fs);
}

PRIVATE void open_file(gpointer userdata, guint action, GtkWidget *widget) {
  GtkWidget *fs = gtk_file_selection_new("Open Sheet");
  //g_print( "hello \n" );

  if (current_filename != NULL)
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), current_filename);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		     GTK_SIGNAL_FUNC(load_new_sheet), fs);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

  gtk_widget_show(fs);

  
}

PRIVATE gboolean sheet_only = FALSE;

PRIVATE void save_file_to(const char *filename, Sheet *sheet) {
  FILE *f = fopen(filename, "wb");
  if (f == NULL)
    return;

  if( sheet == NULL )
      sheet = gui_get_active_sheet();

  sheet_saveto( sheet , f, sheet_only);
  fclose(f);
}



PRIVATE void save_sheet_callback(GtkWidget *widget, GtkWidget *fs) {
  const char *newname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  save_file_to(newname, NULL);

  if (current_filename != NULL)
    free(current_filename);
  current_filename = safe_string_dup(newname);
  gtk_widget_destroy(fs);
}

PUBLIC gboolean save_sheet( Sheet *sheet, char *name ) {
    if( !name ) {
	if( !( sheet->filename ) ) {
	    GtkWidget *filechooser = gtk_file_chooser_dialog_new(
		    "Save Sheet. Has no Name yet !",
		    GTK_WINDOW( gtk_widget_get_toplevel( sheet->scrollwin ) ),
		    GTK_FILE_CHOOSER_ACTION_SAVE,
		    GTK_STOCK_SAVE_AS, GTK_RESPONSE_ACCEPT,
		    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		    NULL );
	    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER( filechooser ), TRUE );
	    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( filechooser ), "~/" );
	    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( filechooser ), "untitled.galan" );
	    
	    if( gtk_dialog_run( GTK_DIALOG( filechooser ) ) == GTK_RESPONSE_CANCEL ) {
		gtk_widget_destroy( GTK_WIDGET( filechooser ) );
		return FALSE;
	    }

	    sheet->filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( filechooser ) );
	    gtk_widget_destroy( GTK_WIDGET( filechooser ) );
	}
    } else {
	if( sheet->filename )
	    free( sheet->filename );

	sheet->filename = safe_string_dup( name );
    }
    
    
    
    sheet_only = FALSE;
    save_file_to(sheet->filename, sheet);

    return TRUE;

}

PRIVATE void save_file(gpointer userdata, guint action, GtkWidget *widget) {
  gboolean reuse_filename = (action == 0);

  if( (sheet_only = (action == 2 )) ){
      Sheet *sheet = gui_get_active_sheet();

      if( sheet->referring_sheets ) {
	  popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		  "Sheet %s is connected to other sheets.\n"
		  "I cant save it like this. Please unconnect first.", sheet->name );
	  return;
      }

  }

  if (!reuse_filename || current_filename == NULL) {
    GtkWidget *fs = gtk_file_selection_new("Save Sheet");

    if (current_filename != NULL)
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), current_filename);

    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		       GTK_SIGNAL_FUNC(save_sheet_callback), fs);
    gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			      GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

    gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
    gtk_widget_show(fs);
  } else
    save_file_to(current_filename, NULL);
}

PRIVATE void action_save_sheet( GtkAction *act, gpointer user_data ) {
    save_file( NULL, 2, NULL );
}

PRIVATE void action_save_file( GtkAction *act, gpointer user_data ) {
    save_file( NULL, 0, NULL );
}

PRIVATE void action_save_file_as( GtkAction *act, gpointer user_data ) {
    save_file( NULL, 1, NULL );
}


PRIVATE void new_sheet( gpointer userdata, guint action, GtkWidget *widget ) {

    Sheet *sheet = create_sheet();
    gui_register_sheet( sheet );
    sheet->control_panel = control_panel_new( sheet->name, TRUE, sheet );
}


// Edit functions called for each selected component.

PRIVATE void edit_delete_comp( Component *c, gpointer userdata ) {
    sheet_delete_component( c->sheet, c );
}

PRIVATE void edit_clone_comp( Component *c, gpointer userdata ) {
    comp_clone( c, c->sheet );
}

//---------------------------------------------------------------------------

//PRIVATE GFunc edit_func[] = { (GFunc) edit_delete_comp,
//			       (GFunc) edit_clone_comp   };

PRIVATE void action_del_selected( GtkAction *act ) {
    Sheet *sheet = gui_get_active_sheet();

    g_list_foreach( sheet->selected_comps, (GFunc) edit_delete_comp, NULL );
}

PRIVATE void action_clone_selected( GtkAction *act ) {
    Sheet *sheet = gui_get_active_sheet();

    g_list_foreach( sheet->selected_comps, (GFunc) edit_clone_comp, NULL );
}

PRIVATE void clone_selected( gpointer userdata, guint action, GtkWidget *widget ) {
    Sheet *sheet = gui_get_active_sheet();

    comp_clone_list( sheet->selected_comps, sheet );
}

PRIVATE void clone_selected_on_new_sheet( GtkAction *act ) {
    Sheet *current_sheet = gui_get_active_sheet();
    Sheet *new_sheet = create_sheet();
    gui_register_sheet( new_sheet );
    new_sheet->control_panel = control_panel_new( new_sheet->name, TRUE, new_sheet );

    comp_clone_list( current_sheet->selected_comps, new_sheet );
}

PRIVATE void del_sheet( gpointer userdata, guint action, GtkWidget *widget ) {

    Sheet *sheet = gui_get_active_sheet();

    sheet_remove( sheet );
}

PRIVATE void clone_sheet( gpointer userdata, guint action, GtkWidget *widget ) {
    Sheet *sheet = gui_get_active_sheet();
    Sheet *clone= sheet_clone( sheet ); 

    gui_register_sheet( clone );
}


PRIVATE void ren_sheet( gpointer userdata, guint action, GtkWidget *widget ) {

    Sheet *sheet = gui_get_active_sheet();

    sheet_rename( sheet );
}

PRIVATE void reg_sheet2( gpointer userdata, guint action, GtkWidget *widget ) {

    Sheet *sheet = gui_get_active_sheet();
    shcomp_register_sheet( sheet );
}


PRIVATE void action_switchcontrolpanel_screen( GtkAction *act ) {
    //static int controlpanel_screennumber;
    
    GdkDisplay *disp = gdk_display_get_default();
    int nscreens     = gdk_display_get_n_screens( disp );
    if( nscreens > 1 ) {
	GdkScreen *cur_scr = gtk_window_get_screen( GTK_WINDOW( control_panel ) );
	gint curscr_num = gdk_screen_get_number( cur_scr );
	GdkScreen *new_scr = gdk_display_get_screen( disp, (curscr_num+1)%nscreens );
	gtk_window_set_screen( GTK_WINDOW( control_panel ), new_scr );
    }
}

PRIVATE void select_master_clock(gpointer userdata, guint action, GtkWidget *widget) {
  GtkWidget *contents;
  AClock **clocks;
  char *texts[2] = { NULL, NULL };
  int i, selection = 0;

  clocks = gen_enumerate_clocks();

  contents = gtk_clist_new(1);
  gtk_clist_set_selection_mode(GTK_CLIST(contents), GTK_SELECTION_BROWSE);
  gtk_clist_column_titles_hide(GTK_CLIST(contents));
  gtk_clist_set_column_width(GTK_CLIST(contents), 0, 150);

  for (i = 0; clocks[i] != NULL; i++) {
    texts[0] = clocks[i]->name;
    if (clocks[i]->selected)
      selection = i;
    gtk_clist_append(GTK_CLIST(contents), texts);
  }

  gtk_clist_select_row(GTK_CLIST(contents), selection, 0);

  switch (popup_dialog("Select Master Clock", MSGBOX_OK | MSGBOX_CANCEL, 0, MSGBOX_OK,
		       contents, NULL, NULL)) {
    case MSGBOX_OK:
      gen_select_clock(clocks[(int) GTK_CLIST(contents)->selection->data]);
      break;

    default:
      break;
  }

  free(clocks);
}

/*=======================================================================*/
/* Build the GTK gui objects */

#if 0
PRIVATE GtkItemFactoryEntry mainmenu_items[] = {
  /* Path, accelerator, callback, extra-numeric-argument, kind */
  { "/_File",			NULL,		NULL, 0,		"<Branch>" },
  { "/File/_New",		"<control>N",	file_new_callback, 0,	NULL },
  { "/File/_Open...",		"<control>O",	open_file, 0,		NULL },
  { "/File/sep1",		NULL,		NULL, 0,		"<Separator>" },
  { "/File/_Save",		"<control>S",	save_file, 0,		NULL },
  { "/File/Save _As...",	NULL,		save_file, 1,		NULL },
  { "/File/sep2",		NULL,		NULL, 0,		"<Separator>" },
  { "/File/E_xit",		"<control>Q",	(GtkItemFactoryCallback) exit_request, 0,	NULL },
  { "/_Sheet",			NULL,		NULL, 0,		"<Branch>" },
  { "/Sheet/_New",		NULL,		new_sheet, 0,		NULL },
//  { "/Sheet/_Open",		NULL,		open_sheet, 0,		NULL },
  { "/Sheet/_Save",		NULL,		save_file, 2,		NULL },
  { "/Sheet/Re_name",		NULL,		ren_sheet, 0,		NULL },
  { "/Sheet/Clone",		NULL,		clone_sheet, 0,		NULL },
  { "/Sheet/sep1",		NULL,		NULL, 0,		"<Separator>" },
  { "/Sheet/_Clear",		NULL,		clear_sheet, 0,		NULL },
  { "/Sheet/_Remove",		NULL,		del_sheet, 0,		NULL },
  { "/Sheet/_Unconnect",	NULL,		unconnect_sheet, 0,	NULL },
  { "/Sheet/sep2",		NULL,		NULL, 0,		"<Separator>" },
  { "/Sheet/R_egister",		NULL,		reg_sheet2, 0,		NULL },
  { "/_Edit",			NULL,		NULL, 0,		"<Branch>" },
  { "/Edit/_Preferences...",	"<control>P",	prefs_edit_prefs, 0,	NULL },
  { "/Edit/Delete",		"<del>",	edit_selected, 0,	NULL },
  { "/Edit/Clone",		NULL,	edit_selected, 1,	NULL },
  { "/Edit/Clone W Conn",		NULL,	clone_selected, 0,	NULL },
  { "/_Window",			NULL,		NULL, 0,		"<Branch>" },
  { "/Window/_Show Control Panel", NULL,	show_control_panel, 0,	NULL },
  { "/Window/_Hide Control Panel", NULL,	hide_control_panel, 0,	NULL },
  { "/_Timing",			NULL,		NULL, 0,		"<Branch>" },
  { "/Timing/_Select Master Clock...", NULL,	select_master_clock, 0,	NULL },
  { "/_Help",			NULL,		NULL, 0,		"<Branch>" },
  { "/Help/_About...",		NULL,		about_callback, 0,	NULL },
  { "/Help/sep1",		NULL,		NULL, 0,		"<Separator>" },
  { "/Help/_Contents...",	NULL,		NULL, 0,		NULL },
};

PRIVATE GtkWidget *build_mainmenu(void) {
  GtkItemFactory *ifact;
  GtkAccelGroup *group;
  int nitems = sizeof(mainmenu_items) / sizeof(mainmenu_items[0]);

  group = gtk_accel_group_new();
  ifact = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",
			       group);

  gtk_item_factory_create_items(ifact, nitems, mainmenu_items, NULL);

  //gtk_accel_group_attach(group, GTK_OBJECT(mainwin));
  gtk_window_add_accel_group( GTK_WINDOW( mainwin), group );

  return gtk_item_factory_get_widget(ifact, "<main>");
}
#endif




//PRIVATE GtkWidget *build_mainmenu(void) {
//}


/**
 * \brief Create the Meshwindow
 *
 * The main Mesh window is created here.
 * After that one should load a file with
 * load_sheet_from_name() or create a new one.
 */


PRIVATE void create_mainwin(void) {
  GtkWidget *vb;
  //GtkWidget *hb;
  GtkWidget *frame, *statusbox;

  //Sheet *s1;

  mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_usize(mainwin, 500, 500);
  gtk_signal_connect(GTK_OBJECT(mainwin), "delete_event", GTK_SIGNAL_FUNC(exit_request), NULL);

  gtk_window_set_title(GTK_WINDOW(mainwin), "gAlan " VERSION);
  gtk_window_position(GTK_WINDOW(mainwin), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(mainwin), TRUE, TRUE, FALSE);
  gtk_window_set_wmclass(GTK_WINDOW(mainwin), "gAlan_mesh", "gAlan");

  vb = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(mainwin), vb);
  gtk_widget_show(vb);

  //hb = gtk_handle_box_new();
  //gtk_widget_show(hb);
  //gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, TRUE, 0);

  //mainmenu = build_mainmenu();
  //gtk_widget_show(mainmenu);
  //gtk_container_add(GTK_CONTAINER(hb), mainmenu);

  mainmenu = gtk_ui_manager_get_widget( ui_manager, "/MainMenu" ); 
  if( mainmenu ) {
      gtk_box_pack_start(GTK_BOX(vb), mainmenu, FALSE, TRUE, 0);
      gtk_widget_show( mainmenu );
  }

//  mainmenu = gtk_ui_manager_get_widget( ui_manager, "/MainToolBar" ); 
//  if( mainmenu ) {
//      gtk_box_pack_start(GTK_BOX(vb), mainmenu, FALSE, TRUE, 0);
//      gtk_widget_show( mainmenu );
//  }

  gtk_window_add_accel_group( GTK_WINDOW(mainwin), gtk_ui_manager_get_accel_group( ui_manager ) );

  upper_box = vb;

  //s1 = create_sheet();
  //s1->control_panel = control_panel_new( s1->name, TRUE, s1 );

  mainnotebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(vb), mainnotebook, TRUE, TRUE, 0);
  gtk_widget_show( mainnotebook );

  frame = gtk_frame_new( NULL );
  gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_OUT );
  statusbox = gtk_hbox_new( FALSE, 5 );
  statusbar = gtk_label_new("");
  gtk_box_pack_start( GTK_BOX(statusbox), statusbar, FALSE,FALSE, 0 );
  gtk_container_add( GTK_CONTAINER(frame), statusbox );
  gtk_box_pack_start(GTK_BOX(vb), frame, FALSE, FALSE, 0);
  //gtk_widget_show( statusbar );
  //gtk_widget_show( statusbox );
  gtk_widget_show_all( frame );

  //gtk_notebook_append_page( GTK_NOTEBOOK( mainnotebook ), s1->scrollwin, NULL );

  //gui_register_sheet( s1 );
}

PUBLIC GtkWindow *gui_get_mesh_window( void ) {
    return GTK_WINDOW(mainwin);
}

PRIVATE gint timeout_handler(gpointer data) {
  gen_clock_mainloop();
  return TRUE;	/* keep this timer running */
}

PRIVATE void gui_default_clock_handler(AClock *clock, AClockReason reason) {
  switch (reason) {
    case CLOCK_DISABLE:
      gtk_timeout_remove(timeout_tag);
      break;

    case CLOCK_ENABLE:
      timeout_tag = gtk_timeout_add(1000 / (44100/MAXIMUM_REALTIME_STEP),
				    timeout_handler,
				    NULL);
      break;

    default:
      g_warning("Unknown reason %d in default_clock_handler", reason);
      break;
  }
}


PRIVATE GtkActionEntry mainmenu_actions[] = {
  { "FileMenu",		NULL,		    "File"	    },
  { "FileNew",		GTK_STOCK_NEW,	    "New",		    "<control>N",	"Create a new file",	    G_CALLBACK(file_new_callback)	},
  { "FileOpen",		GTK_STOCK_OPEN,	    "Open",		    "<control>O",	"Open a file",		    G_CALLBACK(open_file)		},
  { "FileSave",		GTK_STOCK_SAVE,	    "Save",		    "<control>S",	"Save the File",	    G_CALLBACK(action_save_file)    	},  // 0
  { "FileSaveAs",	GTK_STOCK_SAVE_AS,  "Save As",		    NULL,		"Save File with new name",  G_CALLBACK(action_save_file_as)	},  // 1
  { "FileQuit",		GTK_STOCK_QUIT,	    "Quit",		    "<control>Q",	"Quit the bunch",	    G_CALLBACK(exit_request)		},

  { "SheetMenu",	NULL,		    "Sheet"	    },
  { "SheetNew",		GTK_STOCK_NEW,	    "New",		    NULL,		"Create New Sheet",	    G_CALLBACK(new_sheet)		},
  { "SheetSave",	GTK_STOCK_SAVE_AS,   "Save",		    NULL,		"Save the sheet only",	    G_CALLBACK(action_save_sheet)   	},  // 2
  { "SheetRename",	NULL,		    "Rename",		    NULL,		"Rename the sheet",	    G_CALLBACK(ren_sheet)		},
  { "SheetClone",	NULL,		    "Clone",		    NULL,		"Clone the sheet",	    G_CALLBACK(clone_sheet)		},
  { "SheetClear",	NULL,		    "Clear",		    NULL,		"Clear the sheet",	    G_CALLBACK(clear_sheet)		},
  { "SheetRemove",	GTK_STOCK_DELETE,    "Remove",		    NULL,		"Remove the Sheet",	    G_CALLBACK(del_sheet)		},
  { "SheetUnconnect",	NULL,		    "Unconnect",	    NULL,		"Unconnect Sheet",	    G_CALLBACK(unconnect_sheet)		},
  { "SheetRegister",	NULL,		    "Register",		    NULL,		"Register Sheet",	    G_CALLBACK(reg_sheet2)		},

  { "EditMenu",		NULL,		    "Edit"	    },
  { "EditPreferences",	GTK_STOCK_PREFERENCES, "Prefs",		    "<control>P",	"Edit Preferencences",	    G_CALLBACK(prefs_edit_prefs)	},
  { "EditDelete",	GTK_STOCK_DELETE,   "Delete",		    "Delete",		"Delete Current Selection", G_CALLBACK(action_del_selected)	}, // 0
  { "EditClone",	NULL,		    "Clone",		    NULL,		"Clone current Selection",  G_CALLBACK(action_clone_selected)	}, // 1
  { "EditCloneWConn",	NULL,		    "Clone W Conn",	    NULL,		"Clone with Connections",   G_CALLBACK(clone_selected)		},
  { "EditCloneNewSh",	NULL,		    "Clone on New Sheet",   NULL,		"Clone on new Sheet",	    G_CALLBACK(clone_selected_on_new_sheet)	},

  { "WindowsMenu",	NULL,		    "Windows"	    },
  { "ShowControlPanel", NULL,		    "Show Control Panel",   "<control>W",	"Show Control Panel",	    G_CALLBACK(show_control_panel)	},
  { "HideControlPanel", NULL,		    "Hide Control Panel",   NULL,		"Hide Control Panel",	    G_CALLBACK(hide_control_panel)	},

  { "ExtrasMenu",	NULL,		    "Extras",	    },
  { "SelectMasterClock", NULL,		    "Select Master Clock",  NULL,		"Select Master Clock",	    G_CALLBACK(select_master_clock)	},
  { "About",		GTK_STOCK_ABOUT,    "About",		    NULL,		"About",		    G_CALLBACK(about_callback)		},

  { "AddComp",		GTK_STOCK_ADD,	    "_Add..." },
  { "SwitchCPScreen",	NULL,		    "Switch Panel Screen",  NULL,		"Switch ControlPanel Screen",G_CALLBACK(action_switchcontrolpanel_screen)	}
};

PRIVATE guint n_mainmenu_actions = G_N_ELEMENTS (mainmenu_actions);

PRIVATE void setup_ui_manager(void) {
  component_actiongroup = gtk_action_group_new( "comp" );
  menu_actiongroup      = gtk_action_group_new( "menu" );

  ui_manager = gtk_ui_manager_new();

  gtk_ui_manager_insert_action_group( ui_manager, menu_actiongroup, 2 ); 
  gtk_ui_manager_insert_action_group( ui_manager, component_actiongroup, 0 ); 

  gtk_action_group_add_actions( menu_actiongroup, mainmenu_actions, n_mainmenu_actions, NULL );

  GError *err = NULL;
  int ret = gtk_ui_manager_add_ui_from_file( ui_manager, "/home/torbenh/galan.ui", &err );
  
  if( ret == 0 )
      printf( "Error loading ui file ;( Good Luck Without Menus\n" );
}


PUBLIC void init_gui(void) {

    setup_ui_manager();

  create_mainwin();
  gtk_widget_show(mainwin);

  gui_default_clock = gen_register_clock(NULL, "Default Clock", gui_default_clock_handler);
  gen_set_default_clock(gui_default_clock);
  gen_select_clock(gui_default_clock);
}

PUBLIC void gui_add_comps( void ) {

//  GtkWidget *mb = gtk_ui_manager_get_widget (ui_manager, "/comp");
//  gtk_box_pack_start (GTK_BOX (upper_box),
//	  mb,
//	  FALSE, FALSE, 0);
//  gtk_widget_show( mb );
}

PUBLIC void done_gui(void) {
  gen_set_default_clock(NULL);
  gen_deregister_clock(gui_default_clock);
  gen_stop_clock();	/* redundant */
}
