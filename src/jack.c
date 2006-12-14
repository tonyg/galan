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
#include "control.h"
#include "msgbox.h"

#include <jack/jack.h>
#ifdef HAVE_JACKMIDI_H
#include <jack/miditypes.h>
#endif


PRIVATE jack_client_t *jack_client = NULL;


PUBLIC jack_client_t *galan_jack_get_client(void) {
    return jack_client;
}

PUBLIC void init_jack(void) {

    jack_client = jack_client_open( "galan", 0, NULL );
}

PUBLIC void done_jack(void) {
    
    jack_client_close( jack_client );
}

