/*********************************************************************
 *     mix.h - header file for the mixer library
 *
 *      Copyright (C) 2000 Rui Sousa 
 ********************************************************************* 
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version. 
 * 
 *     This program is distributed in the hope that it will be useful, 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *     GNU General Public License for more details. 
 * 
 *     You should have received a copy of the GNU General Public 
 *     License along with this program; if not, write to the Free 
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, 
 *     USA. 
 ********************************************************************* 
*/


#ifndef _MIX_H
#define _MIX_H

#include <linux/types.h>

#define RECSRC_AC97	0x1
#define RECSRC_MIC	0x2
#define RECSRC_FX	0x3

struct mixer_private_ioctl {
        __u32 cmd;
        __u32 val[90];          /* in kernel space only a fraction of this is used */
};
#define CMD_WRITEPTR		_IOW('D', 2, struct mixer_private_ioctl) 
#define CMD_READPTR		_IOR('D', 3, struct mixer_private_ioctl)
#define CMD_SETRECSRC           _IOW('D', 4, struct mixer_private_ioctl) 
#define CMD_GETRECSRC           _IOR('D', 5, struct mixer_private_ioctl) 
#define CMD_GETVOICEPARAM       _IOR('D', 6, struct mixer_private_ioctl) 
#define CMD_SETVOICEPARAM       _IOW('D', 7, struct mixer_private_ioctl) 
#define CMD_SETGPOUT            _IOW('D', 14, struct mixer_private_ioctl) 
#define CMD_SETMCH_FX           _IOW('D', 17, struct mixer_private_ioctl)
#define CMD_SETPASSTHROUGH	_IOW('D', 18, struct mixer_private_ioctl)
#define CMD_PRIVATE3_VERSION	_IOW('D', 19, struct mixer_private_ioctl)
#define CMD_AC97_BOOST		_IOW('D', 20, struct mixer_private_ioctl)

#define SEND_A	0
#define SEND_B	1
#define SEND_C	2
#define SEND_D	3

#define MONO	0
#define LEFT	1
#define RIGHT	2	

/* the mixer program should declare one of this structures per mixer device found */
struct mix_settings {
	char dev_name[64];

	__u16 send_routing[3];
        __u32 send_amount[3];
	int recsrc;
	__u32 fxwc;

	int private_version; // The version # of the emu10k1 driver's interface
	int major_ver; // major version number of the emu10k1 driver
	int minor_ver; //minor version number of the emu10k1 driver
	int card_type; // 0=SBLive, 1=Audigy
	int iocfg;  // Value of iocfg register (Audigy only)
};


struct io_config{
	__u32 output;
	int num_out;
	__u32 input;
	int num_in;
	char name[32];
};


static const char mix_recsrc[][5] = {"ADC","Mic","FX"};

int mix_init(struct mix_settings *);
int mix_commit(struct mix_settings *);

void mix_print_settings(struct mix_settings *);

/* hardware interface */

/* gets the recording mixer settings directly from the hardware */
/* returns 0 on success, -1 otherwise */
int mix_hw_recording_get(struct mix_settings *);

/* sets the recording mixer settings directly to the hardware */
/* returns 0 on success, -1 otherwise */
int mix_hw_recording_set(struct mix_settings *);

/* gets the playback voice parameters directly from the hardware*/
/* returns 0 on success, -1 otherwise */
int mix_hw_voice_settings_get(struct mix_settings *);

/* sets the playback voice parameters directly to the hardware */
/* returns 0 on success, -1 otherwise */
int mix_hw_voice_settings_set(struct mix_settings *);


/* Recording FX bus selection */

/* returns 1 if bus "bus" is selected for recording, 0 otherwise */
int mix_check_rec_fx_bus(struct mix_settings *, unsigned int bus);

/* selects bus "bus" for recording */
/* returns 0 on success, -1 otherwise */
int mix_set_rec_fx_bus(struct mix_settings *, unsigned int bus);

/* deselects bus "bus" for recording */
/* returns 0 on success, -1 otherwise */
int mix_reset_rec_fx_bus(struct mix_settings *, unsigned int bus);

/* bus:  0 - 15
   all combinations of bus numbers selected for
   recording are valid: 0; 0, 2, 4; 3, 5, 9; ... */


/* Recording source selection */

/* returns a name string for source number n */
/* returns NULL if n is invalid */
const char *mix_rec_source_name(int n);

/* returns source number for source named name */
/* returns -1 if name isn't valid */
int mix_rec_source_number(const char *name);

/* select recording source "source" for recording */
int mix_set_rec_source(struct mix_settings *, const char *);

/* returns a string containing the name of the selected recording source */
const char *mix_get_rec_source(struct mix_settings *);

/* returns 1 if source n is selected for recording, 0 otherwise */
int mix_check_rec_source(struct mix_settings *, const char *);

/* source: RECSRC_AC97, RECSRC_MIC, RECSRC_FX or "AC97", "MIC", "FX"
   only one can be selected at a time */


/* default playback voice send amount setting */

/* returns send amount for channel send "send", -1 on error */
int mix_get_voice_send_amount(struct mix_settings *set, int type, unsigned int send);

/* set the send amount for channel send "send" */
/* returns 0 on success, -1 otherwise */
int mix_set_voice_send_amount(struct mix_settings *set, int type, unsigned int send,
		    unsigned int amount);

/* amount: 0x00 - 0xff */
/*   send: SEND_A, SEND_B, SEND_C, SEND_D */ 
/*   type: MONO, LEFT, RIGHT */

/* default playback voice send routing setting */

int mix_check_voice_send_bus(struct mix_settings *set, int type, unsigned int send, unsigned int bus);
/* returns the bus number for channel send "send", -1 on error */
int mix_get_voice_send_bus(struct mix_settings *set, int type, unsigned int send);

/* sets the bus number for channel send "send" */
/* returns 0 on success, -1 otherwise */
int mix_set_voice_send_bus(struct mix_settings *set, int type, unsigned int send, unsigned int bus);

/* bus: 0 - 15 */

// Sets shared jack in analog or digital mode on 5.1 SBLives.
// analog=0, digital=1, power-on default is analog.
int set_51(struct mix_settings *set,int mode);

/* Sets the start input fx number for multichannel playback */
int set_mch_fx(struct mix_settings *set, int fx);

/* Activates or deactivated the digital pass-through mode */
int set_passthrough(struct mix_settings *set, int active);

/* Enable the IR remote control on the 5.1's IR drive */
int enable_ir(struct mix_settings *set);


/* gets/prints information on the driver and card */
int mix_print_info (struct mix_settings *set);
int mix_get_info (struct mix_settings *set);

/* set frequency of output spdif on Audigy cards  */ 
int mix_set_spdif_freq(struct mix_settings *set, int);
/* returns the frequency currently se for the Audigy's spdif output */
int mix_get_spdif_freq(struct mix_settings *set);
/*turns on a 12dB on front analog output (headphone mode)*/
int mix_ac97_boost(struct mix_settings *set, int state);

#endif
