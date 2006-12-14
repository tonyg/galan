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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "global.h"
#include "msgbox.h"
#include "prefs.h"

#define HOMEDIR_GALANDIR	"/.galan"
#define HOMEDIR_SUFFIX		"/.galan/prefs"
#define SITE_PREFS_PATH		SITE_PKGLIB_DIR "/prefs"

typedef struct PrefEntry {
  char *key;
  union {
    char *value;
    GList *list;
  } d;
} PrefEntry;

PRIVATE GHashTable *prefs = NULL;
PRIVATE GHashTable *options = NULL;

PRIVATE void clone_prefs_entry(gpointer key, gpointer value, gpointer user_data) {
  GHashTable *newtab = user_data;
  PrefEntry *entry = value;
  PrefEntry *newentry = safe_malloc(sizeof(PrefEntry));

  newentry->key = safe_string_dup(entry->key);
  newentry->d.value = safe_string_dup(entry->d.value);
  g_hash_table_insert(newtab, newentry->key, newentry);
}

PRIVATE GHashTable *clone_prefs_table(GHashTable *tab) {
  GHashTable *newtab = g_hash_table_new(g_str_hash, g_str_equal);

  g_hash_table_foreach(tab, clone_prefs_entry, newtab);
  return newtab;
}

PRIVATE void free_prefs_entry(gpointer key, gpointer value, gpointer user_data) {
  PrefEntry *entry = value;
  free(entry->key);
  free(entry->d.value);
  free(entry);
}

PRIVATE void kill_prefs_table(GHashTable *tab) {
  g_hash_table_foreach(tab, free_prefs_entry, NULL);
}

PUBLIC char *prefs_get_item(const char *key) {
  PrefEntry *entry = g_hash_table_lookup(prefs, key);
  return entry ? entry->d.value : NULL;
}

PUBLIC void prefs_set_item(const char *key, const char *value) {
  PrefEntry *entry = safe_malloc(sizeof(PrefEntry));
  entry->key = safe_string_dup(key);
  entry->d.value = safe_string_dup(value);
  prefs_clear_item(key);
  g_hash_table_insert(prefs, entry->key, entry);
}

PUBLIC void prefs_clear_item(const char *key) {
  PrefEntry *entry = g_hash_table_lookup(prefs, key);

  if (entry != NULL) {
    g_hash_table_remove(prefs, key);
    free(entry->key);
    free(entry->d.value);
    free(entry);
  }
}

PUBLIC void prefs_register_option(const char *key, const char *value) {
  PrefEntry *entry = g_hash_table_lookup(options, key);

  if (entry == NULL) {
    entry = safe_malloc(sizeof(PrefEntry));
    entry->key = safe_string_dup(key);
    entry->d.list = NULL;
  }

  entry->d.list = g_list_append(entry->d.list, safe_string_dup(value));
  g_hash_table_insert(options, entry->key, entry);
}

PRIVATE void add_option_to_optlist(gpointer key, gpointer value, gpointer user_data) {
  PrefEntry *entry = value;
  GtkCList *list = user_data;
  char *texts[2] = { NULL, NULL };

  texts[0] = entry->key;
  gtk_clist_append(list, texts);
}

PRIVATE gboolean optlist_row_selected(GtkCList *optlist, gint row, gint column,
				  GdkEventButton *event, gpointer user_data) {
  GtkCombo *droplist = gtk_object_get_data(GTK_OBJECT(optlist), "droplist");
  PrefEntry *entry;
  gchar *option_name;

  gtk_clist_get_text(optlist, row, column, &option_name);
  entry = g_hash_table_lookup(options, option_name);

  if (entry != NULL) {
    gchar *current_value = prefs_get_item(option_name);

    /* Remove the data so that setting the text doesn't update the variable */
    gtk_object_remove_data(GTK_OBJECT(droplist->entry), "option_name");
    gtk_combo_set_popdown_strings(droplist, entry->d.list);
    gtk_entry_set_text(GTK_ENTRY(droplist->entry), current_value ? current_value : "");

    /* Reinstate it so that changes from now on take effect */
    gtk_object_set_data(GTK_OBJECT(droplist->entry), "option_name", option_name);
  }
  return FALSE;
}

PRIVATE gboolean droplist_entry_changed(GtkEntry *entry, gpointer data) {
  gchar *option_name = gtk_object_get_data(GTK_OBJECT(entry), "option_name");

  if (option_name != NULL) {
    const gchar *value_text = gtk_entry_get_text(entry);

    if (value_text[0] == '\0')
      prefs_clear_item(option_name);
    else
      prefs_set_item(option_name, value_text);
  }
  return TRUE;
}

PUBLIC void prefs_edit_prefs(void) {
  GHashTable *backup_prefs = clone_prefs_table(prefs);
  GtkWidget *ob = gtk_vbox_new(FALSE, 5);
  GtkWidget *ib = gtk_vbox_new(FALSE, 5);
  GtkWidget *hb;
  GtkWidget *label = gtk_label_new("[Note: Quit and restart to put changes into effect]");
  GtkWidget *frame = gtk_frame_new("Options");
  GtkWidget *optlist = gtk_clist_new(1);
  GtkWidget *droplist = gtk_combo_new();
  GtkWidget *scrollwin = gtk_scrolled_window_new(NULL,NULL);

  /* Configure the widgets */
  //gtk_widget_set_usize(optlist, 200, 100);
  gtk_clist_set_selection_mode(GTK_CLIST(optlist), GTK_SELECTION_SINGLE);
  gtk_clist_column_titles_hide(GTK_CLIST(optlist));
  gtk_clist_set_column_width(GTK_CLIST(optlist), 0, 200);
  g_hash_table_foreach(options, add_option_to_optlist, optlist);

  gtk_box_pack_start(GTK_BOX(ob), label, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(ob), frame, TRUE, TRUE, 0);
  gtk_widget_show(label);
  gtk_widget_show(frame);

  /* Nest and display the widgets */
  gtk_container_add(GTK_CONTAINER(frame), ib);
  gtk_widget_show(ib);

  gtk_container_add( GTK_CONTAINER( scrollwin ), optlist );
  gtk_box_pack_start(GTK_BOX(ib), scrollwin, TRUE, TRUE, 0);
  gtk_widget_show(optlist);
  gtk_widget_show(scrollwin);

  hb = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(ib), hb, FALSE, FALSE, 0);
  gtk_widget_show(hb);

  label = gtk_label_new("Value:");
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start(GTK_BOX(hb), label, FALSE, TRUE, 0);
  gtk_widget_show(label);

  gtk_box_pack_start(GTK_BOX(hb), droplist, FALSE, FALSE, 0);
  gtk_widget_show(droplist);

  /* Configure the data and signals */
  gtk_object_set_data(GTK_OBJECT(optlist), "droplist", droplist);

  gtk_signal_connect(GTK_OBJECT(optlist), "select_row",
		     GTK_SIGNAL_FUNC(optlist_row_selected), NULL);
  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(droplist)->entry), "changed",
		     GTK_SIGNAL_FUNC(droplist_entry_changed), NULL);

  if (popup_dialog("Adjust Preferences", MSGBOX_ACCEPT | MSGBOX_DISMISS, 0, MSGBOX_NONE,
		   ob, NULL, NULL) != MSGBOX_ACCEPT) {
    kill_prefs_table(prefs);
    prefs = backup_prefs;
  }
}

#define LINELEN 2048
PRIVATE void load_prefs_from(char *filename) {
  FILE *f = fopen(filename, "rt");
  char line[LINELEN];	/* %%% Yuck, a fixed size buffer */

  if (f == NULL)
    return;

  while (!feof(f)) {
    if (fgets(line, LINELEN, f) != NULL) {
      char *valpos = strchr(line, '=');
      int len;

      if (line[0] == '#' || valpos == NULL)
	continue;

      /* Extract value string + trim leading whitespace */
      *valpos = '\0';
      do {
	valpos++;
      } while (isspace(*valpos) && *valpos != '\0');

      /* Trim trailing whitespace */
      len = strlen(valpos);
      while (isspace(valpos[len-1])) {
	valpos[len-1] = '\0';
	len--;
      }

      prefs_set_item(line, valpos);
    }
  }

  fclose(f);
}

PRIVATE void save_pref_entry(gpointer key, gpointer value, gpointer user_data) {
  FILE *f = user_data;
  PrefEntry *entry = value;

  fprintf(f, "%s=%s\n", entry->key, entry->d.value);
}

PRIVATE gboolean save_prefs_to(char *filename) {
  FILE *f = fopen(filename, "wt");

  if (f == NULL)
    return FALSE;

  g_hash_table_foreach(prefs, save_pref_entry, f);
  return TRUE;
}

PRIVATE char *build_userprefs_path(char *homedir) {
  char *userprefs = safe_malloc(strlen(homedir) + strlen(HOMEDIR_SUFFIX) + 1);

  strcpy(userprefs, homedir);
  strcat(userprefs, HOMEDIR_SUFFIX);
  return userprefs;
}

PRIVATE void load_prefs(void) {
  char *homedir = getenv("HOME");

  load_prefs_from(SITE_PREFS_PATH);

  if (homedir != NULL) {
    char *userprefs = build_userprefs_path(homedir);
    load_prefs_from(userprefs);
    free(userprefs);
  }
}

PRIVATE void save_prefs(void) {
#ifdef G_OS_WIN32
  if (!save_prefs_to(SITE_PREFS_PATH))
    g_warning("Could not save preferences to %s", SITE_PREFS_PATH);
#else
  char *homedir = getenv("HOME");

  if (homedir != NULL) {
    char *userprefs = build_userprefs_path(homedir);

    if (!save_prefs_to(userprefs)) {
      char *usergalandir = safe_malloc(strlen(homedir) + strlen(HOMEDIR_GALANDIR) + 1);
      strcpy(usergalandir, homedir);
      strcat(usergalandir, HOMEDIR_GALANDIR);
      mkdir(usergalandir, 0777);
      free(usergalandir);
      if (!save_prefs_to(userprefs))
	g_warning("Could not save preferences to %s", userprefs);
    }

    free(userprefs);
  }
#endif
}

PUBLIC void init_prefs(void) {
  prefs = g_hash_table_new(g_str_hash, g_str_equal);
  options = g_hash_table_new(g_str_hash, g_str_equal);

  load_prefs();
}

PRIVATE void free_options_entry(gpointer key, gpointer value, gpointer user_data) {
  PrefEntry *entry = value;
  free(entry->key);
  g_list_foreach(entry->d.list, (GFunc) free, NULL);
  g_list_free(entry->d.list);
  free(entry);
}

PUBLIC void done_prefs(void) {
  save_prefs();

  g_hash_table_foreach(options, free_options_entry, NULL);
  kill_prefs_table(prefs);

  g_hash_table_destroy(options);
  g_hash_table_destroy(prefs);
}
