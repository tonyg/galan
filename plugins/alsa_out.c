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

//#include <sys/types.h>
//#include <sys/ioctl.h>
//#include <sys/soundcard.h>
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

#define ALSA_PCM_OLD_HW_PARAMS_API
#define ALSA_PCM_OLD_SW_PARAMS_API
#include "alsa/asoundlib.h"

#define SIG_LEFT_CHANNEL	0
#define SIG_RIGHT_CHANNEL	1

#define DEFAULT_FRAGMENT_EXPONENT	12

typedef signed short OUTPUTSAMPLE;

typedef struct ALSAData {
  snd_pcm_t *handle;
  int count;
  struct pollfd *ufds;
  gint input_tag;
  AClock *clock;
} ALSAData;

PRIVATE int instance_count = 0;
PRIVATE char device[256];

snd_pcm_format_t format = SND_PCM_FORMAT_S16;	 /* sample format */
int rate = SAMPLE_RATE;				 /* stream rate */
int channels = 2;				 /* count of channels */
int buffer_time = 1000000*2048/SAMPLE_RATE;	 /* ring buffer length in us */
int period_time = 1000000*1024/SAMPLE_RATE;	 /* period time in us */

snd_pcm_sframes_t buffer_size;
snd_pcm_sframes_t period_size;
snd_output_t *output = NULL;

/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err) {
    g_print( "xrun !!!....\n" );
	if (err == -EPIPE) {	/* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	/* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

PRIVATE void audio_play_fragment(snd_pcm_t *handle, SAMPLE *left, SAMPLE *right, int length) {
  OUTPUTSAMPLE *outbuf;
  int buflen = length * sizeof(OUTPUTSAMPLE) * 2;
  int i,err;

  if (length <= 0)
    return;

  outbuf = malloc(buflen);
  RETURN_UNLESS(outbuf != NULL);

  for (i = 0; i < length; i++) {
    outbuf[i<<1]	= (OUTPUTSAMPLE) MIN(MAX(left[i] * 32767, -32768), 32767);
    outbuf[(i<<1) + 1]	= (OUTPUTSAMPLE) MIN(MAX(right[i] * 32767, -32768), 32767);
  }


again:
  err = snd_pcm_writei(handle, outbuf, length);
  if( err < 0 ) {
      if (xrun_recovery(handle, err) < 0) {
	  printf("Write error: %s\n", snd_strerror(err));
	  exit(EXIT_FAILURE);
      }
      goto again;
  }

  //g_print( "len=%d, err=%d state=%d\n", length, err, snd_pcm_state(handle) );
  free(outbuf);
}

static int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
			snd_pcm_access_t access)
{
	int err, dir;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	err = snd_pcm_hw_params_set_rate_near(handle, params, rate, 0);
	if (err < 0) {
		printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
		return err;
	}
	if (err != rate) {
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return -EINVAL;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, buffer_time, &dir);
	if (err < 0) {
		printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
		return err;
	}
	buffer_size = snd_pcm_hw_params_get_buffer_size(params);
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, period_time, &dir);
	if (err < 0) {
		printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
		return err;
	}
	period_size = snd_pcm_hw_params_get_period_size(params, &dir);
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	//g_print( "bs=%d, ps=%d\n", buffer_size, period_size );
	return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is full */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size );
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size );
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* align all transfers to 1 sample */
	err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1);
	if (err < 0) {
		printf("Unable to set transfer align for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}


PRIVATE gboolean open_audiofd(ALSAData *d) {
  int err;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_sw_params_t *swparams;

  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_sw_params_alloca(&swparams);

  if ((err = snd_pcm_open(&(d->handle), device, SND_PCM_STREAM_PLAYBACK, 0 )) < 0) {
      printf("Playback open error: %s\n", snd_strerror(err));
      return FALSE;
  }

  if ((err = set_hwparams(d->handle, hwparams,SND_PCM_ACCESS_RW_INTERLEAVED )) < 0) {
      printf("Setting of hwparams failed: %s\n", snd_strerror(err));
      return FALSE;
  }
  if ((err = set_swparams(d->handle, swparams)) < 0) {
      printf("Setting of swparams failed: %s\n", snd_strerror(err));
      return FALSE;
  }

  return TRUE;
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  ALSAData *d = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      gdk_input_remove(d->input_tag);
      d->input_tag = -1;
      break;

    case CLOCK_ENABLE:
      if (d->input_tag == -1)
	d->input_tag = gdk_input_add(d->ufds[0].fd, GDK_INPUT_WRITE,
					(GdkInputFunction) gen_clock_mainloop, NULL);
      break;

    default:
      g_message("Unreachable code reached (oss_output)... reason = %d", reason);
      break;
  }
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  ALSAData *data = g->data;

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

      audio_play_fragment(data->handle, l_buf, r_buf, event->d.integer);
      free(l_buf);
      free(r_buf);

      break;
    }

    default:
      g_warning("oss_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  ALSAData *data;
  int err;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  data = safe_malloc(sizeof(ALSAData));



  if (open_audiofd(data) == FALSE) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open audio device, %s.", device);
    return 0;
  }

  data->count = snd_pcm_poll_descriptors_count (data->handle);
  if (data->count <= 0) {
      printf("Invalid poll descriptors count\n");
      return 0;
  }
  g_print( "poll count = %d\n", data->count );

  data->ufds = malloc(sizeof(struct pollfd) * data->count);
  if (data->ufds == NULL) {
      printf("No enough memory\n");
      return 0;
  }
  if ((err=snd_pcm_poll_descriptors(data->handle, data->ufds, data->count)) < 0) {
      printf("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
      return 0;
  }

  data->input_tag = -1;
  data->clock = gen_register_clock(g, "ALSA Output Clock", clock_handler);

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);
  gen_select_clock(data->clock);	/* a not unreasonable assumption? */

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  ALSAData *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data != NULL) {
    gen_deregister_clock(data->clock);
    if (data->input_tag != -1)
      gdk_input_remove(data->input_tag);
    //close(data->audiofd);

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
    prefer = value ? !strcmp(value, "ALSA") : FALSE;
  }
  prefs_register_option("output_plugin", "ALSA");

  {
    char *name = prefs_get_item("output_alsa_device");
    sprintf(device, "%s", name ? name : "plughw:0,0");
  }
  prefs_register_option("output_alsa_device", "hw:0,0");
  prefs_register_option("output_alsa_device", "plughw:0,0");

  prefs_register_option("output_alsa_period_size", "64" );
  prefs_register_option("output_alsa_period_size", "128" );
  prefs_register_option("output_alsa_period_size", "256" );
  prefs_register_option("output_alsa_period_size", "512" );
  prefs_register_option("output_alsa_period_size", "1024" );
  prefs_register_option("output_alsa_period_size", "2048" );
  prefs_register_option("output_alsa_period_size", "4096" );

  {
    char *name = prefs_get_item("output_alsa_period_size");
    int period_size;
    if (name == NULL || sscanf(name, "%d", &period_size) != 1) {
      period_size = 1024;
    }
    period_time = 1000000*period_size/SAMPLE_RATE;	 /* period time in us */
  }

  prefs_register_option("output_alsa_num_periods", "2" );
  prefs_register_option("output_alsa_num_periods", "3" );
  prefs_register_option("output_alsa_num_periods", "4" );

  {
    char *name = prefs_get_item("output_alsa_num_periods");
    int num_period;
    if (name == NULL || sscanf(name, "%d", &num_period) != 1) {
      num_period = 2;
    }
    buffer_time = period_time * num_period;
  }

  k = gen_new_generatorclass("audio_out", prefer, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, prefer, "Outputs/ALSA Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);
}

PUBLIC void init_plugin_alsa_out(void) {
  setup_class();
}
