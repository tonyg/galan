/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 * Copyright (C) 1999 Elliot Lee
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
/* esd_output.c - gAlan filter to output to the Enlightened (NOT!) sound daemon */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <esd.h>

#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"
#include "prefs.h"

#define SIG_LEFT_CHANNEL	0
#define SIG_RIGHT_CHANNEL	1

typedef signed short OUTPUTSAMPLE;

typedef struct Data {
  int audiofd;
  gint input_tag;
  AClock *clock;
} Data;

PRIVATE int instance_count = 0;

PRIVATE void audio_play_fragment(int audiofd, SAMPLE *left, SAMPLE *right, int length) {
  OUTPUTSAMPLE *outbuf;
  int buflen = length * sizeof(OUTPUTSAMPLE) * 2;
  int i;

  if (length <= 0)
    return;

  outbuf = malloc(buflen);
  RETURN_UNLESS(outbuf != NULL);

  for (i = 0; i < length; i++) {
    outbuf[i<<1]	= (OUTPUTSAMPLE) MIN(MAX(left[i] * 32767, -32768), 32767);
    outbuf[(i<<1) + 1]	= (OUTPUTSAMPLE) MIN(MAX(right[i] * 32767, -32768), 32767);
  }

  write(audiofd, outbuf, buflen);
  free(outbuf);
}

PRIVATE int open_audiofd(void) {
  return esd_play_stream_fallback(ESD_BITS16|ESD_STEREO, SAMPLE_RATE, NULL, "esd_output");
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  Data *data = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      gdk_input_remove(data->input_tag);
      data->input_tag = -1;
      break;

    case CLOCK_ENABLE:
      if (data->input_tag == -1)
	data->input_tag = gdk_input_add(data->audiofd, GDK_INPUT_WRITE,
					(GdkInputFunction) gen_clock_mainloop, NULL);
      break;

    default:
      g_message("Unreachable code reached (esd_output)... reason = %d", reason);
      break;
  }
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  Data *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
      SAMPLE *l_buf, *r_buf;
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      l_buf = safe_malloc(bufbytes);
      r_buf = safe_malloc(bufbytes);

      if (!gen_read_realtime_input(g, SIG_LEFT_CHANNEL, -1, l_buf, event->d.integer))
	memset(l_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT_CHANNEL, -1, r_buf, event->d.integer))
	memset(r_buf, 0, bufbytes);

      audio_play_fragment(data->audiofd, l_buf, r_buf, event->d.integer);
      free(l_buf);
      free(r_buf);

      break;
    }

    default:
      g_warning("esd_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  Data *data;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  data = safe_malloc(sizeof(Data));

  data->audiofd = open_audiofd();

  if (data->audiofd < 0) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK, "Could not open ESound audio subsystem.");
    return 0;
  }

  data->input_tag = -1;
  data->clock = gen_register_clock(g, "ESD Output Clock", clock_handler);

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);
  gen_select_clock(data->clock);	/* a not unreasonable assumption? */

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data != NULL) {
    gen_deregister_clock(data->clock);
    if (data->input_tag != -1)
      gdk_input_remove(data->input_tag);
    close(data->audiofd);

    free(data);
  }

  instance_count--;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Left Channel", SIG_FLAG_REALTIME },
  { "Right Channel", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k;
  gboolean prefer;

  {
    char *value = prefs_get_item("output_plugin");
    prefer = value ? !strcmp(value, "ESound") : FALSE;
  }
  prefs_register_option("output_plugin", "ESound");

  k = gen_new_generatorclass("audio_out", prefer, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, prefer, "Outputs/ESound Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);
}

PUBLIC void init_plugin_esd_output(void) {
  setup_class();
}
