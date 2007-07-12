/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 * Copyright (C) 2006 Torben Hohn
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
#include <lash/lash.h>

#include "global.h"
#include "generator.h"
#include "control.h"
#include "msgbox.h"



PRIVATE lash_client_t * lash_client = NULL;


PUBLIC lash_client_t *galan_lash_get_client(void) {
    return lash_client;
}

PUBLIC void init_lash( int argc, char **argv ) {

    lash_args_t *lash_args;
    int flags = LASH_Config_File;

    lash_args = lash_extract_args(&argc, &argv);

    lash_client =
	lash_init(lash_args, "galan", flags, LASH_PROTOCOL(2, 0));

    if (!lash_client) {
	fprintf(stderr, "%s: could not initialise lash\n", __FUNCTION__);
	fprintf(stderr, "%s: running galan without lash session-support\n", __FUNCTION__);
	fprintf(stderr, "%s: to enable lash session-support launch the lash server prior galan\n", __FUNCTION__);
    }

    if (lash_enabled(lash_client)) {
	lash_event_t *event = lash_event_new_with_type(LASH_Client_Name);
	lash_event_set_string(event, "galan");
	lash_send_event(lash_client, event);
    }
}

PUBLIC void done_lash(void) {
    
    // XXX: abmelden...
    if (lash_enabled(lash_client)) {
	lash_event_t *event = lash_event_new_with_type(LASH_Quit);
	lash_send_event(lash_client, event);
    }
}

