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

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <windows.h>
#include <mmsystem.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"

#define SIG_LEFT_CHANNEL	0
#define SIG_RIGHT_CHANNEL	1

typedef signed short OUTPUTSAMPLE;

typedef struct Data {
  HWAVEOUT hwo;
  gboolean open;
  guint idle_tag;
  AClock *clock;
} Data;

#define MIN_BLOCKS		8		/* Minimum usable number of WAVEHDRs */
#define DEFAULT_NUM_BLOCKS	16		/* Default number of WAVEHDRs used */
#define MAX_BLOCKS		32		/* Maximum number of available WAVEHDRs */

PRIVATE int num_blocks = DEFAULT_NUM_BLOCKS;	/* Number of WAVEHDRs to actually use */

PRIVATE int instance_count = 0;			/* Only one instance allowed. */

PRIVATE int next = 0;				/* Index of next available WAVEHDR */
PRIVATE int pending = 0;			/* Number of WAVEHDRs in system queue */
PRIVATE WAVEHDR hdr[MAX_BLOCKS];		/* WAVEHDRs == system audio buffers */

PRIVATE void audio_play_fragment(HWAVEOUT hwo, SAMPLE *left, SAMPLE *right, int length) {
  OUTPUTSAMPLE *outbuf;
  int i;

  if (length <= 0)
    return;

  outbuf = (OUTPUTSAMPLE *) hdr[next].lpData;	/* Copy data directly into WAVEHDR */
  for (i = 0; i < length; i++) {
    outbuf[i<<1]	= (OUTPUTSAMPLE) MIN(MAX(left[i] * 32767, -32768), 32767);
    outbuf[(i<<1) + 1]	= (OUTPUTSAMPLE) MIN(MAX(right[i] * 32767, -32768), 32767);
  }

  hdr[next].dwBufferLength = length * sizeof(OUTPUTSAMPLE) * 2;
  waveOutWrite(hwo, &hdr[next], sizeof(hdr[next]));
  pending++;	/* Mark it as 'in system queue' */
  next = (next + 1) % num_blocks;
}

PRIVATE void CALLBACK win_callback(HWAVEOUT hwo, UINT uMsg,
				   DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
  if (uMsg == WOM_DONE) {
    pending--;	/* Notify rest of app that it's time to calculate another buffer's worth */
  }
}

PRIVATE gboolean open_audiofd(LPHWAVEOUT phwo) {
  WAVEFORMATEX format;
  int i;

  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = SAMPLE_RATE;
  format.nAvgBytesPerSec = SAMPLE_RATE * 2 * sizeof(OUTPUTSAMPLE);
  format.nBlockAlign = sizeof(OUTPUTSAMPLE) * 2;
  format.wBitsPerSample = sizeof(OUTPUTSAMPLE) * 8;
  format.cbSize = 0;

  RETURN_VAL_UNLESS(waveOutOpen(phwo, WAVE_MAPPER, &format,
				(DWORD) win_callback, 0, CALLBACK_FUNCTION)
		    == MMSYSERR_NOERROR,
		    FALSE);

  for (i = 0; i < num_blocks; i++) {
    hdr[i].lpData = calloc(MAXIMUM_REALTIME_STEP * 2, sizeof(OUTPUTSAMPLE));
    hdr[i].dwBufferLength = MAXIMUM_REALTIME_STEP * sizeof(OUTPUTSAMPLE) * 2;
    hdr[i].dwBytesRecorded = 0;
    hdr[i].dwUser = 0;
    hdr[i].dwFlags = 0;
    hdr[i].dwLoops = 0;

    /* Two MS-reserved fields */
    hdr[i].lpNext = NULL;
    hdr[i].reserved = 0;

    RETURN_VAL_UNLESS(waveOutPrepareHeader(*phwo, &hdr[i], sizeof(hdr[i]))
		      == MMSYSERR_NOERROR,
		      FALSE);
  }

  return TRUE;
}

PRIVATE gboolean close_audiofd(HWAVEOUT hwo) {
  int i;

  RETURN_VAL_UNLESS(waveOutReset(hwo) == MMSYSERR_NOERROR, FALSE);

  for (i = 0; i < num_blocks; i++) {
    LPSTR buf = hdr[i].lpData;
    if (waveOutUnprepareHeader(hwo, &hdr[i], sizeof(hdr[i])) != MMSYSERR_NOERROR)
      g_warning("waveOutUnprepareHeader of hdr[%d] failed", i);
    free(buf);
  }

  RETURN_VAL_UNLESS(waveOutClose(hwo) == MMSYSERR_NOERROR, FALSE);

  return TRUE;
}

/* This function acts as the master clock on Windows. */
PRIVATE gint idle_handler(gpointer data) {
  while (pending < num_blocks)
    gen_clock_mainloop();

  return TRUE;
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  Data *data = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      if (data->open) {
	gtk_idle_remove(data->idle_tag);
	close_audiofd(data->hwo);
	data->open = FALSE;
      }
      break;

    case CLOCK_ENABLE:
      if (!data->open) {
	data->open = open_audiofd(&data->hwo);
	if (data->open)
	  data->idle_tag = gtk_idle_add(idle_handler, NULL);
      }
      break;

    default:
      g_message("Unreachable code reached (win_output)... reason = %d", reason);
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

      if (data->open)
        audio_play_fragment(data->hwo, l_buf, r_buf, event->d.integer);
      free(l_buf);
      free(r_buf);

      break;
    }

    default:
      g_warning("win_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE gboolean init_instance(Generator *g) {
  Data *data;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  data = safe_malloc(sizeof(Data));

  data->hwo = 0;
  data->open = FALSE;
  data->clock = gen_register_clock(g, "Win32 Output Clock", clock_handler);

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);
  gen_select_clock(data->clock);	/* a not unreasonable assumption? */

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data != NULL) {
    gen_deregister_clock(data->clock);
    if (data->open)
      close_audiofd(data->hwo);
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

  {
    char *value = prefs_get_item("output_win_num_blocks");

    if (value == NULL || sscanf(value, "%d", &num_blocks) != 1) {
      num_blocks = DEFAULT_NUM_BLOCKS;
    }

    num_blocks = MAX(MIN_BLOCKS, MIN(MAX_BLOCKS, num_blocks));
  }

  {
    int i;
    char tmp[20];

    for (i = MIN_BLOCKS; i <= MAX_BLOCKS; i++) {
      sprintf(tmp, "%d", i);
      prefs_register_option("output_win_num_blocks", tmp);
    }
  }

  k = gen_new_generatorclass("audio_out", FALSE, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Win32 Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);
}

PUBLIC void init_plugin_win_output(void) {
  setup_class();
}
