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
  snd_async_handler_t *async_handler;
  AClock *clock;
} ALSAData;

PRIVATE int instance_count = 0;
PRIVATE char device[256];

snd_pcm_format_t format = SND_PCM_FORMAT_S16;	 /* sample format */
int rate = SAMPLE_RATE;				 /* stream rate */
int channels = 2;				 /* count of channels */
int buffer_time = 1000000/SAMPLE_RATE*4096;	 /* ring buffer length in us */
int period_time = 1000000*1024/SAMPLE_RATE;	 /* period time in us */

snd_pcm_sframes_t buffer_size;
snd_pcm_sframes_t period_size;
snd_output_t *output = NULL;

/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
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

  g_print( "writing %d frames\n", length );

  err = snd_pcm_writei(handle, outbuf, length);
  if( err < 0 ) {
      if (xrun_recovery(handle, err) < 0) {
	  printf("Write error: %s\n", snd_strerror(err));
	  exit(EXIT_FAILURE);
      }
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

  if ((err = snd_pcm_open(&(d->handle), device, SND_PCM_STREAM_PLAYBACK, SND_PCM_ASYNC )) < 0) {
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

PRIVATE void async_callback( snd_async_handler_t *handler ) {
    ALSAData *data = snd_async_handler_get_callback_private( handler );

    gint avail = snd_pcm_avail_update( data->handle );
    g_print( "in callback...\n" );

    gen_clock_mainloop_have_remaining( avail );
}


PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  ALSAData *d = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      snd_async_del_handler(d->async_handler);
      d->async_handler = NULL;
      break;

    case CLOCK_ENABLE:
      if (d->async_handler == NULL)
      {
	  int err;

	  err = snd_async_add_pcm_handler(&d->async_handler, d->handle, async_callback, d);
	  if (err < 0) {
	      printf("Unable to register async handler\n");
	      return;
	  }
	  //g_print( "Pre Start State1: %d\n", snd_pcm_state( d->handle ) );
	  //gen_clock_mainloop_have_remaining( period_size );
	  //g_print( "Pre Start State2: %d\n", snd_pcm_state( d->handle ) );
	  //gen_clock_mainloop_have_remaining( period_size );
//	  for (count = 0; count < 2; count++) {
//	      generate_sine(areas, 0, period_size, &data.phase);
//	      err = snd_pcm_writei(handle, samples, period_size);
//	      if (err < 0) {
//		  printf("Initial write error: %s\n", snd_strerror(err));
//		  exit(EXIT_FAILURE);
//	      }
//	      if (err != period_size) {
//		  printf("Initial write error: written %i expected %li\n", err, period_size);
//		  exit(EXIT_FAILURE);
//	      }
//	  }
//

	  g_print( "Pre Start State3: %d\n", snd_pcm_state( d->handle ) );
	  //err = snd_pcm_start(d->handle);
	  //if (err < 0) {
	  //    printf("Start error: %s\n", snd_strerror(err));
	  //    return;
	  //}

	  /* because all other work is done in the signal handler,
	     suspend the process */
      }
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


  data->async_handler = NULL;
  data->clock = gen_register_clock(g, "ALSA Output Callback Clock", clock_handler);

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);
  //gen_select_clock(data->clock);	/* a not unreasonable assumption? */

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
    prefer = value ? !strcmp(value, "ALSA Callback") : FALSE;
  }
  prefs_register_option("output_plugin", "ALSA Callback");

  {
    char *name = prefs_get_item("output_alsa_callback_device");
    sprintf(device, "%s", name ? name : "plughw:0,0");
  }
  prefs_register_option("output_alsa_callback_device", "hw:0,0");
  prefs_register_option("output_alsa_callback_device", "plughw:0,0");


  k = gen_new_generatorclass("audio_out_cb", prefer, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, prefer, "Outputs/ALSA Output (callback) ",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);
}

PUBLIC void init_plugin_alsa_out_callback(void) {
  setup_class();
}



