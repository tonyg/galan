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
#include <stdarg.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "msgbox.h"

PRIVATE gboolean exit_popup_mainloop = FALSE;
PRIVATE MsgBoxResponse action_taken = MSGBOX_NONE;
PRIVATE MsgBoxResponse default_action = MSGBOX_NONE;

PRIVATE void button_clicked(GtkWidget *button, MsgBoxResponse response) {
  action_taken = response;
  exit_popup_mainloop = TRUE;
}

PRIVATE gboolean timeout_handler(GtkWidget *dlg) {
  action_taken = default_action;
  exit_popup_mainloop = TRUE;
  return FALSE;
}

PRIVATE void make_button(GtkBox *box, MsgBoxResponse buttons, MsgBoxResponse def,
			 MsgBoxResponse mask, char *label) {
  GtkWidget *button;

  if (buttons & mask) {
    button = gtk_button_new_with_label(label);
    gtk_widget_show(button);
    gtk_box_pack_start(box, button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       GTK_SIGNAL_FUNC(button_clicked), (gpointer) mask);

    if (mask == def) {
      GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default(button);
    }
  }
}

PUBLIC MsgBoxResponse popup_dialog(char *title, MsgBoxResponse buttons,
				   gint timeout_millis, MsgBoxResponse def,
				   GtkWidget *contents,
				   MsgBoxResponseHandler handler, gpointer userdata) {
  GtkWidget *dlg;
  GtkWidget *hbox;
  gboolean save_exit_popup_mainloop = exit_popup_mainloop;
  MsgBoxResponse save_action_taken = action_taken;
  MsgBoxResponse save_default_action = default_action;
  MsgBoxResponse result;

  dlg = gtk_dialog_new();
  gtk_signal_connect(GTK_OBJECT(dlg), "destroy",
		     GTK_SIGNAL_FUNC(button_clicked), (gpointer) MSGBOX_CANCEL);
  gtk_window_set_title(GTK_WINDOW(dlg), title);
  gtk_window_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);

  hbox = gtk_hbox_new(TRUE, 5);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), hbox, TRUE, TRUE, 5);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(hbox), contents, TRUE, TRUE, 5);
  gtk_widget_show(contents);

  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_OK, "Ok");
  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_ACCEPT, "Accept");
  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_YES, "Yes");
  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_NO, "No");
  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_CANCEL, "Cancel");
  make_button(GTK_BOX(GTK_DIALOG(dlg)->action_area), buttons, def, MSGBOX_DISMISS, "Dismiss");

  if (def != MSGBOX_NONE && timeout_millis > 0) {
    default_action = def;
    gtk_timeout_add(timeout_millis, (GtkFunction) timeout_handler, dlg);
  }

  exit_popup_mainloop = FALSE;
  action_taken = MSGBOX_NONE;

  gtk_widget_show(dlg);

  while (!exit_popup_mainloop)
    gtk_main_iteration();

  if (handler != NULL)
    handler(action_taken, userdata);

  gtk_widget_hide(dlg);
  /* %%% I don't think this following is necessary?: gtk_widget_unref(dlg); */

  result = action_taken;
  exit_popup_mainloop = save_exit_popup_mainloop;
  action_taken = save_action_taken;
  default_action = save_default_action;

  return result;
}

PUBLIC MsgBoxResponse popup_msgbox(char *title, MsgBoxResponse buttons,
				   gint timeout_millis, MsgBoxResponse def,
				   char *format, ...) {
  va_list vl;
  char textbuf[2048];	/* Yuck, a fixed-size buffer. Stack overflows, anyone? */
  GtkWidget *label;

  va_start(vl, format);
  vsprintf(textbuf, format, vl);
  va_end(vl);

  label = gtk_label_new(textbuf);

  return popup_dialog(title, buttons, timeout_millis, def, label, NULL, NULL);
}
