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

#define SIG_LEFT_FRONT_CHANNEL	0
#define SIG_RIGHT_FRONT_CHANNEL	1
#define SIG_LEFT_REAR_CHANNEL	2
#define SIG_RIGHT_REAR_CHANNEL	3

#define DEFAULT_FRAGMENT_EXPONENT	12

typedef signed short OUTPUTSAMPLE;

typedef struct EmuData {
  int audiofd1, audiofd2;
  gint input_tag;
  AClock *clock;
} EmuData;

PRIVATE int instance_count = 0;
PRIVATE int audio_fragment_exponent = DEFAULT_FRAGMENT_EXPONENT;

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

PRIVATE int open_audiofd(int nr) {
  int i;
  int audiofd;

  audiofd = open(nr==0 ? "/dev/dsp" : "/dev/dsp1", O_WRONLY);
  RETURN_VAL_UNLESS(audiofd != -1, -1);

  i = (4 << 16) | audio_fragment_exponent;	/* 4 buffers */
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &i) != -1, -1);

  i = AFMT_S16_LE;
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SETFMT, &i) != -1, -1);

  i = 1;
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_STEREO, &i) != -1, -1);

  i = 44100;
  RETURN_VAL_UNLESS(ioctl(audiofd, SNDCTL_DSP_SPEED, &i) != -1, -1);

  return audiofd;
}

PRIVATE void clock_handler(AClock *clock, AClockReason reason) {
  EmuData *data = clock->gen->data;

  switch (reason) {
    case CLOCK_DISABLE:
      gdk_input_remove(data->input_tag);
      data->input_tag = -1;
      break;

    case CLOCK_ENABLE:
      if (data->input_tag == -1)
	data->input_tag = gdk_input_add(data->audiofd1, GDK_INPUT_WRITE,
					(GdkInputFunction) gen_clock_mainloop, NULL);
      break;

    default:
      g_message("Unreachable code reached (emu_output)... reason = %d", reason);
      break;
  }
}

PRIVATE void realtime_handler(Generator *g, AEvent *event) {
  EmuData *data = g->data;

  switch (event->kind) {
    case AE_REALTIME: {
      SAMPLE *lf_buf, *rf_buf, *lr_buf, *rr_buf;
      int bufbytes = event->d.integer * sizeof(SAMPLE);

      lf_buf = safe_malloc(bufbytes);
      rf_buf = safe_malloc(bufbytes);
      lr_buf = safe_malloc(bufbytes);
      rr_buf = safe_malloc(bufbytes);

      if (!gen_read_realtime_input(g, SIG_LEFT_FRONT_CHANNEL, -1, lf_buf, event->d.integer))
	memset(lf_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT_FRONT_CHANNEL, -1, rf_buf, event->d.integer))
	memset(rf_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_LEFT_REAR_CHANNEL, -1, lr_buf, event->d.integer))
	memset(lr_buf, 0, bufbytes);

      if (!gen_read_realtime_input(g, SIG_RIGHT_REAR_CHANNEL, -1, rr_buf, event->d.integer))
	memset(rr_buf, 0, bufbytes);

      audio_play_fragment(data->audiofd1, lf_buf, rf_buf, event->d.integer);
      audio_play_fragment(data->audiofd2, lr_buf, rr_buf, event->d.integer);
      free(lf_buf);
      free(rf_buf);
      free(lr_buf);
      free(rr_buf);

      break;
    }

    default:
      g_warning("emu_output module doesn't care for events of kind %d.", event->kind);
      break;
  }
}

PRIVATE int init_instance(Generator *g) {
  EmuData *data;

  instance_count++;
  if (instance_count > 1)
    /* Not allowed more than one of these things. */
    return 0;

  data = safe_malloc(sizeof(EmuData));

  data->audiofd1 = open_audiofd(0);

  if (data->audiofd1 < 0) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open audio device, %s.", "/dev/dsp");
    return 0;
  }

  data->audiofd2 = open_audiofd(1);

  if (data->audiofd2 < 0) {
    free(data);
    popup_msgbox("Error", MSGBOX_OK, 120000, MSGBOX_OK,
		 "Could not open audio device, %s.", "/dev/dsp1");
    return 0;
  }

  data->input_tag = -1;
  data->clock = gen_register_clock(g, "Emu Output Clock", clock_handler);

  g->data = data;

  gen_register_realtime_fn(g, realtime_handler);
  gen_select_clock(data->clock);	/* a not unreasonable assumption? */

  return 1;
}

PRIVATE void destroy_instance(Generator *g) {
  EmuData *data = g->data;

  gen_deregister_realtime_fn(g, realtime_handler);

  if (data != NULL) {
    gen_deregister_clock(data->clock);
    if (data->input_tag != -1)
      gdk_input_remove(data->input_tag);
    close(data->audiofd1);
    close(data->audiofd2);

    free(data);
  }

  instance_count--;
}

PRIVATE InputSignalDescriptor input_sigs[] = {
  { "Left Front Channel", SIG_FLAG_REALTIME },
  { "Right Front Channel", SIG_FLAG_REALTIME },
  { "Left Rear Channel", SIG_FLAG_REALTIME },
  { "Right Rear Channel", SIG_FLAG_REALTIME },
  { NULL, }
};

PRIVATE void setup_class(void) {
  GeneratorClass *k;

  {
    char *name = prefs_get_item("output_emu_fragment_size");

    if (name == NULL || sscanf(name, "%d", &audio_fragment_exponent) != 1) {
      audio_fragment_exponent = DEFAULT_FRAGMENT_EXPONENT;
    }

    audio_fragment_exponent = MAX(audio_fragment_exponent, 7);
  }
  prefs_register_option("output_emu_fragment_size", "7");
  prefs_register_option("output_emu_fragment_size", "8");
  prefs_register_option("output_emu_fragment_size", "9");
  prefs_register_option("output_emu_fragment_size", "10");
  prefs_register_option("output_emu_fragment_size", "11");
  prefs_register_option("output_emu_fragment_size", "12");
  prefs_register_option("output_emu_fragment_size", "13");
  prefs_register_option("output_emu_fragment_size", "14");
  prefs_register_option("output_emu_fragment_size", "15");
  prefs_register_option("output_emu_fragment_size", "16");

  k = gen_new_generatorclass("emu_out", FALSE, 0, 0,
			     input_sigs, NULL, NULL,
			     init_instance, destroy_instance,
			     (AGenerator_pickle_t) init_instance, NULL);

  gencomp_register_generatorclass(k, FALSE, "Outputs/Emu10k1 Output",
				  PIXMAPDIRIFY("oss_output.xpm"),
				  NULL);
}

#include "emu10k1-include/dsp.h"

//PRIVATE void dspstuff( void ) {
//    struct dsp_patch_manager mgr;
//    int inputs[2] = {0,1};
//    int outputs[2] = {0,1};
//    mgr.mixer_fd = open( "/dev/mixer", O_RDWR, 0 );
//    mgr.dsp_fd = open( "/dev/dsp", O_RDWR, 0 );
//    dsp_init( &mgr );
//    dsp_unload_all( &mgr );
//    dsp_add_route( &mgr, 0, 0 );
//    dsp_add_route( &mgr, 1, 1 );
//    dsp_read_patch( &mgr, "/usr/local/share/emu10k1/chorus_2.bin", inputs, outputs, 2,2,1,"brr",0 );
//    dsp_load( &mgr );
//}

extern void init_emuiocomp( void );
extern void init_emupatchcomp( void );
extern void done_emuiocomp( void );
PUBLIC void init_plugin_emu10k1(void) {
  setup_class();
  init_emuiocomp();
  init_emupatchcomp();

  //dspstuff();
}
