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

/*=======================================================================*/
/* Variables for GTK gui */

PRIVATE GtkWidget *mainwin;
PRIVATE GtkWidget *mainmenu;

PRIVATE char *current_filename = NULL;
PRIVATE guint timeout_tag;
PRIVATE AClock *gui_default_clock = NULL;

/*=======================================================================*/
/* GTK gui Callbacks */

PRIVATE gboolean exit_request(GtkWidget *w, GdkEvent *ev, gpointer data) {
  /* %%% should check that the file is saved here */
  gtk_main_quit();
  return TRUE;
}

PRIVATE void about_callback(void) {
  popup_msgbox("About gAlan", MSGBOX_OK, 0, MSGBOX_OK,
	       "gAlan %s\n"
	       "Copyright Tony Garnock-Jones (C) 1999\n"
	       "A modular sound-processing tool\n(Graphical Audio LANguage)\n"
	       "\n"
	       "gAlan comes with ABSOLUTELY NO WARRANTY; for details, see the file\n"
	       "\"COPYING\" that came with the gAlan distribution.\n"
	       "This is free software, distributed under the GNU General Public\n"
	       "License. Please see \"COPYING\" or http://www.gnu.org/copyleft/gpl.txt\n"
	       "\n"
	       "SITE_PKGLIB_DIR = %s\n"
	       "\n"
	       "NOTE: This is BETA software\n",
	       VERSION,
	       SITE_PKGLIB_DIR
	       );
}

PRIVATE void file_new_callback(gpointer userdata, guint action, GtkWidget *widget) {
  if (current_filename != NULL) {
    free(current_filename);
    current_filename = NULL;
  }
  sheet_clear();
}

PRIVATE void load_new_sheet(GtkWidget *widget, GtkWidget *fs) {
  FILE *f;
  char *newname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  f = fopen(newname, "rb");

  if (f == NULL || !sheet_loadfrom(f)) {
    popup_msgbox("Error Loading File", MSGBOX_OK, 120000, MSGBOX_OK,
		 "File not found, or file format error: %s",
		 newname);
    return;
  }

  fclose(f);

  if (current_filename != NULL)
    free(current_filename);
  current_filename = safe_string_dup(newname);
  gtk_widget_destroy(fs);
}

PRIVATE void open_file(gpointer userdata, guint action, GtkWidget *widget) {
  GtkWidget *fs = gtk_file_selection_new("Open Sheet");

  if (current_filename != NULL)
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), current_filename);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), "clicked",
		     GTK_SIGNAL_FUNC(load_new_sheet), fs);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fs));

  gtk_window_set_modal(GTK_WINDOW(fs), TRUE);
  gtk_widget_show(fs);
}

PRIVATE void save_file_to(char *filename) {
  FILE *f = fopen(filename, "wb");
  if (f == NULL)
    return;

  sheet_saveto(f);
  fclose(f);
}

PRIVATE void save_sheet_callback(GtkWidget *widget, GtkWidget *fs) {
  char *newname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs));

  save_file_to(newname);

  if (current_filename != NULL)
    free(current_filename);
  current_filename = safe_string_dup(newname);
  gtk_widget_destroy(fs);
}

PRIVATE void save_file(gpointer userdata, guint action, GtkWidget *widget) {
  gboolean reuse_filename = (action == 0);

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
    save_file_to(current_filename);
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
  { "/_Edit",			NULL,		NULL, 0,		"<Branch>" },
  { "/Edit/_Preferences...",	"<control>P",	prefs_edit_prefs, 0,	NULL },
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

  gtk_accel_group_attach(group, GTK_OBJECT(mainwin));
  return gtk_item_factory_get_widget(ifact, "<main>");
}

PRIVATE void create_mainwin(void) {
  GtkWidget *vb;
  GtkWidget *hb;
  /* GtkWidget *pb; */
  /* GtkWidget *hs; */

  mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_usize(mainwin, 500, 500);
  gtk_signal_connect(GTK_OBJECT(mainwin), "delete_event", GTK_SIGNAL_FUNC(exit_request), NULL);

  gtk_window_set_title(GTK_WINDOW(mainwin), "gAlan " VERSION);
  gtk_window_position(GTK_WINDOW(mainwin), GTK_WIN_POS_CENTER);
  gtk_window_set_policy(GTK_WINDOW(mainwin), TRUE, TRUE, FALSE);
  gtk_window_set_wmclass(GTK_WINDOW(mainwin), "gAlan_mesh", "gAlan");

  vb = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vb);
  gtk_container_add(GTK_CONTAINER(mainwin), vb);

  hb = gtk_handle_box_new();
  gtk_widget_show(hb);
  gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, TRUE, 0);

  mainmenu = build_mainmenu();
  gtk_widget_show(mainmenu);
  gtk_container_add(GTK_CONTAINER(hb), mainmenu);

  /*
  hb = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hb);
  gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, TRUE, 0);
  */

  /*
  pb = gtk_toggle_button_new_with_label("Play");
  gtk_widget_show(pb);
  gtk_box_pack_end(GTK_BOX(hb), pb, FALSE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(pb), "toggled", GTK_SIGNAL_FUNC(do_master_play), NULL);
  */

  /*
  pb = gtk_combo_new();
  gtk_widget_show(pb);
  gtk_box_pack_start(GTK_BOX(hb), pb, TRUE, TRUE, 10);
  */

  /*
  hs = gtk_hseparator_new();
  gtk_widget_show(hs);
  gtk_box_pack_start(GTK_BOX(vb), hs, FALSE, TRUE, 0);
  */

  create_sheet(GTK_BOX(vb));
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

/*=======================================================================*/
/* External entrypoint */

PUBLIC void init_gui(void) {
  create_mainwin();
  gtk_widget_show(mainwin);

  gui_default_clock = gen_register_clock(NULL, "Default Clock", gui_default_clock_handler);
  gen_set_default_clock(gui_default_clock);
  gen_select_clock(gui_default_clock);
}

PUBLIC void done_gui(void) {
  gen_set_default_clock(NULL);
  gen_deregister_clock(gui_default_clock);
  gen_stop_clock();	/* redundant */
}
