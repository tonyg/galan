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
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "global.h"
#include "generator.h"
#include "comp.h"
#include "control.h"
#include "gencomp.h"
#include "msgbox.h"
#include "prefs.h"

#define DEFAULT_FRAGMENT_EXPONENT	12

#define SIG_OUTPUT		0

typedef signed short OUTPUTSAMPLE;

typedef struct Data {
  int audiofd;
  gboolean preread;
} Data;

PRIVATE int instance_count = 0;
PRIVATE char dspname[256];
PRIVATE int audio_fragment_exponent = DEFAULT_FRAGMENT_EXPONENT;
PRIVATE gboolean eightbit_in = FALSE;

PRIVATE int open_audiofd(void) {
  int i;
  int audiofd;

  audiofd = open(dspname, O_RDONLY);
  if (audiofd == -1) {
    perror("open_audiofd/oss_input");
    return -1;
  }
  RETURN_VAL_UNLESS(audiofd != -1, -1);

  i = (4 << 16) | audio_fragment_exponent;	/* 4 buffers */
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &i) != -1, -1);

  i = eightbit_in ? AFMT_S8 : AFMT_S16_NE;

  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SETFMT, &i) != -1, -1);

  i = 0;
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_STEREO, &i) != -1, -1);

  i = SAMPLE_RATE;
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SPEED, &i) != -1, -1);

  return audiofd;
}

PRIVATE gboolean init_instance(Generator *g) {
  Data *data;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  data = safe_malloc(sizeof(Data));

  data->audiofd = open_audiofd();

  data->preread = FALSE;
  if (data->audiofd < 0) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open audio device, %s.", dspname);
    return 0;
  }

  g->data = data;

  return TRUE;
}

PRIVATE void destroy_instance(Generator *g) {
  Data *data = g->data;

  if (data != NULL) {
    close(data->audiofd);

    free(data);
  }

  instance_count--;
}

PRIVATE gboolean output_generator(Generator *g, SAMPLE *buf, int buflen) {
  Data *data = g->data;
  int i;

  if( eightbit_in )
  {
	  gint8 samp[MAXIMUM_REALTIME_STEP];

	  if( ! data->preread ) {
		  data->preread = TRUE;
		  return FALSE;
	  }

	  if (read(data->audiofd, samp, sizeof(gint8) * buflen) < sizeof(gint8) * buflen) {
		  printf("."); fflush(stdout);
	  }

	  for (i = 0; i < buflen; i++)
		  buf[i] = samp[i] / 128.0;
  }
  else
  {
	  gint16 samp[MAXIMUM_REALTIME_STEP];

	  if( ! data->preread ) {
		  data->preread = TRUE;
		  return FALSE;
	  }

	  if (read(data->audiofd, samp, sizeof(gint16) * buflen) < sizeof(gint16) * buflen) {
		  printf("."); fflush(stdout);
	  }

	  for (i = 0; i < buflen; i++)
		  buf[i] = samp[i] / 32768.0;
  }

  return TRUE;
}

PRIVATE OutputSignalDescriptor output_sigs[] = {
  { "Output", SIG_FLAG_REALTIME, { output_generator, } },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k;
  gboolean prefer;

  {
    char *value = prefs_get_item("input_plugin");
    prefer = value ? !strcmp(value, "OSS") : FALSE;
  }
  prefs_register_option("input_plugin", "OSS");

  {
    char *name = prefs_get_item("input_oss_device");
    sprintf(dspname, "%s", name ? name : "/dev/dsp");
  }
  prefs_register_option("input_oss_device", "/dev/dsp");
  prefs_register_option("input_oss_device", "/dev/dsp0");
  prefs_register_option("input_oss_device", "/dev/dsp1");

  {
    char *name = prefs_get_item("input_oss_fragment_size");

    if (name == NULL || sscanf(name, "%d", &audio_fragment_exponent) != 1) {
      audio_fragment_exponent = DEFAULT_FRAGMENT_EXPONENT;
    }

    audio_fragment_exponent = MAX(audio_fragment_exponent, 7);
  }
  prefs_register_option("input_oss_fragment_size", "7");
  prefs_register_option("input_oss_fragment_size", "8");
  prefs_register_option("input_oss_fragment_size", "9");
  prefs_register_option("input_oss_fragment_size", "10");
  prefs_register_option("input_oss_fragment_size", "11");
  prefs_register_option("input_oss_fragment_size", "12");
  prefs_register_option("input_oss_fragment_size", "13");
  prefs_register_option("input_oss_fragment_size", "14");
  prefs_register_option("input_oss_fragment_size", "15");
  prefs_register_option("input_oss_fragment_size", "16");

  {
    char *num_bits = prefs_get_item("input_oss_bits");

    if (num_bits == NULL || ( strcmp( num_bits, "16" ) && strcmp( num_bits, "8" ) ) )
	    eightbit_in = FALSE;
    else
	    eightbit_in = strcmp(num_bits, "8" ) ? FALSE : TRUE;
  }
  prefs_register_option("input_oss_bits", "16");
  prefs_register_option("input_oss_bits", "8");

  k = gen_new_generatorclass("audio_in", prefer, 0, 0,
			     NULL, output_sigs, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, prefer, "Sources/OSS Input",
				  NULL,
				  NULL);
}

PUBLIC void init_plugin_oss_input(void) {
  setup_class();
}
