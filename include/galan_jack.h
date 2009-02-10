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


#ifndef GALAN_JACK_H
#define GALAN_JACK_H

#include <jack/jack.h>
#include "generator.h"
#include "objectstore.h"

typedef void (*jack_process_handler_t)(Generator *g, jack_nframes_t nframes);
typedef void (*transport_frame_event_handler_t)( Generator *g, SAMPLETIME frame, SAMPLETIME numframes, double bpm );

extern jack_client_t *galan_jack_get_client(void);
extern void midilearn_set_target_control( Control *c );
extern void midilearn_remove_control( Control *c );
extern void unpickle_midi_map_array(ObjectStoreDatum *array, ObjectStore *db);
extern ObjectStoreDatum *midi_map_pickle(ObjectStore *db);

extern void galan_jack_register_process_handler( Generator *g, jack_process_handler_t handler );
extern void galan_jack_deregister_process_handler(Generator *g, jack_process_handler_t func);

extern void galan_jack_register_transport_clock( Generator *g, transport_frame_event_handler_t handler );
extern void galan_jack_deregister_transport_clock( Generator *g, transport_frame_event_handler_t handler );
extern SAMPLETIME galan_jack_get_timestamp( void );

extern void init_jack( void );
extern void run_jack( void );
extern void done_jack( void );
#endif
