/*********************************************************************
 *     mix.c - a library for setting special emu10k1 mixer settings 
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

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include "../emu10k1-include/mix.h"
#include <string.h>

#define MIXER_DEV_NAME "/dev/mixer"

void mix_print_settings(struct mix_settings *set)
{

	printf("Recording Source: %s\n", mix_rec_source_name(set->recsrc));
	if (set->recsrc == mix_rec_source_number("FX")) {
		int bus;
		printf("  Selected bus[es]:");
		for (bus = 0; bus < 16; bus++)
			if (mix_check_rec_fx_bus(set, bus))
				printf(" 0x%1x", bus);
		printf("\n");
	}

	printf("\nMono Voice:\n");
	printf("                A    B    C    D\n");
	printf("  Send Route:   0x%1x  0x%1x  0x%1x  0x%1x\n",
	       mix_get_voice_send_bus(set, MONO, SEND_A),
	       mix_get_voice_send_bus(set, MONO, SEND_B),
	       mix_get_voice_send_bus(set, MONO, SEND_C), mix_get_voice_send_bus(set, MONO, SEND_D));
	printf("  Send Amount:  0x%02x 0x%02x 0x%02x 0x%02x\n",
	       mix_get_voice_send_amount(set, MONO, SEND_A),
	       mix_get_voice_send_amount(set, MONO, SEND_B),
	       mix_get_voice_send_amount(set, MONO, SEND_C), mix_get_voice_send_amount(set, MONO, SEND_D));


	printf("\nStereo Voice:\n");
	printf("                Left                    Right\n");
	printf("          A    B    C    D        A    B    C    D\n");
	printf
	    ("  Send Route:     0x%1x  0x%1x  0x%1x  0x%1x      0x%1x  0x%1x  0x%1x  0x%1x\n",
	     mix_get_voice_send_bus(set, LEFT, SEND_A), mix_get_voice_send_bus(set, LEFT, SEND_B),
	     mix_get_voice_send_bus(set, LEFT, SEND_C), mix_get_voice_send_bus(set, LEFT, SEND_D),
	     mix_get_voice_send_bus(set, RIGHT, SEND_A), mix_get_voice_send_bus(set, RIGHT, SEND_B),
	     mix_get_voice_send_bus(set, RIGHT, SEND_C), mix_get_voice_send_bus(set, RIGHT, SEND_D));

	printf
	    ("  Send Amount:  0x%02x 0x%02x 0x%02x 0x%02x       0x%02x 0x%02x 0x%02x 0x%02x\n",
	     mix_get_voice_send_amount(set, LEFT, SEND_A),
	     mix_get_voice_send_amount(set, LEFT, SEND_B),
	     mix_get_voice_send_amount(set, LEFT, SEND_C),
	     mix_get_voice_send_amount(set, LEFT, SEND_D),
	     mix_get_voice_send_amount(set, RIGHT, SEND_A),
	     mix_get_voice_send_amount(set, RIGHT, SEND_B),
	     mix_get_voice_send_amount(set, RIGHT, SEND_C), mix_get_voice_send_amount(set, RIGHT, SEND_D));

	return;
}


int mix_hw_recording_get(struct mix_settings *set)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_GETRECSRC;

	if ((fd = open(set->dev_name, O_RDONLY, 0)) < 0) {
		perror(set->dev_name);
		return fd;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);

	set->recsrc = ctl.val[0];
	set->fxwc = ctl.val[1];

	return 0;
}

int mix_hw_recording_set(struct mix_settings *set)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_SETRECSRC;
	ctl.val[0] = set->recsrc;
	ctl.val[1] = set->fxwc;

	if ((fd = open(set->dev_name, O_WRONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);
	return 0;
}

int mix_hw_voice_settings_get(struct mix_settings *set)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_GETVOICEPARAM;

	if ((fd = open(set->dev_name, O_RDONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);

	set->send_routing[0] = ctl.val[0];
	set->send_amount[0] = ctl.val[1];

	set->send_routing[1] = ctl.val[2];
	set->send_amount[1] = ctl.val[3];

	set->send_routing[2] = ctl.val[4];
	set->send_amount[2] = ctl.val[5];

	return 0;
}

int mix_hw_voice_settings_set(struct mix_settings *set)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_SETVOICEPARAM;

	ctl.val[0] = set->send_routing[0];
	ctl.val[1] = set->send_amount[0];

	ctl.val[2] = set->send_routing[1];
	ctl.val[3] = set->send_amount[1];

	ctl.val[4] = set->send_routing[2];
	ctl.val[5] = set->send_amount[2];

	if ((fd = open(set->dev_name, O_RDONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);

	return 0;
}

int mix_get_voice_send_amount(struct mix_settings *set, int type, unsigned int send)
{
	send &= 0x3;

	if (type > 2)
		return -1;

	return (set->send_amount[type] >> (8 * send)) & 0xff;
}

int mix_set_voice_send_amount(struct mix_settings *set, int type, unsigned int send, unsigned int amount)
{
	send &= 0x3;
	amount &= 0xff;

	if (type > 2)
		return -1;

	set->send_amount[type] &= ~(0xff << (8 * send));
	set->send_amount[type] |= amount << (8 * send);

	return 0;
}

int mix_check_voice_send_bus(struct mix_settings *set, int type, unsigned int send, unsigned int bus)
{
	send &= 0x3;
	bus &= 0xf;

	if (type > 2)
		return -1;

	if (((set->send_routing[type] >> (4 * send)) & 0xf) == bus)
		return 1;

	return 0;
}

int mix_get_voice_send_bus(struct mix_settings *set, int type, unsigned int send)
{
	send &= 0x3;

	if (type > 2)
		return -1;

	return (set->send_routing[type] >> (4 * send)) & 0xf;
}

int mix_set_voice_send_bus(struct mix_settings *set, int type, unsigned int send, unsigned int bus)
{
	send &= 0x3;
	bus &= 0xf;

	if (type > 2)
		return -1;

	set->send_routing[type] &= ~(0xf << (4 * send));
	set->send_routing[type] |= bus << (4 * send);

	return 0;
}

int mix_check_rec_fx_bus(struct mix_settings *set, unsigned int bus)
{
	bus &= 0xf;

	if (set->fxwc & (1 << bus))
		return 1;

	return 0;
}

int mix_set_rec_fx_bus(struct mix_settings *set, unsigned int bus)
{
	bus &= 0xf;

	set->fxwc |= (1 << bus);
	set->fxwc &= 0xffff;

	return 0;
}

int mix_reset_rec_fx_bus(struct mix_settings *set, unsigned int bus)
{
	bus &= 0xf;

	set->fxwc &= ~(1 << bus);

	return 0;
}

const char *mix_rec_source_name(int number)
{
	switch (number) {
	case RECSRC_AC97:
		return mix_recsrc[0];
	case RECSRC_MIC:
		return mix_recsrc[1];
	case RECSRC_FX:
		return mix_recsrc[2];
	default:
		return (char *) NULL;
	}
}

int mix_rec_source_number(const char *name)
{
	if (!strcmp(name, mix_recsrc[0]))
		return RECSRC_AC97;

	if (!strcmp(name, mix_recsrc[1]))
		return RECSRC_MIC;

	if (!strcmp(name, mix_recsrc[2]))
		return RECSRC_FX;

	return -1;
}

int mix_set_rec_source(struct mix_settings *set, const char *source)
{
	int ret;

	ret = mix_rec_source_number(source);
	if (ret < 0)
		return ret;

	set->recsrc = ret;

	return 0;
}

const char *mix_get_rec_source(struct mix_settings *set)
{
	return mix_rec_source_name(set->recsrc);
}

int mix_check_rec_source(struct mix_settings *set, const char *source)
{
	if (set->recsrc == mix_rec_source_number(source))
		return 1;

	return 0;
}

/* mixer settings initialization */
 /*FIXME*/
/* autodetect emu10k1 mixer devices */
int mix_init(struct mix_settings *set)
{
	int ret;

	if(set->dev_name[0]==0)
		strcpy(set->dev_name, MIXER_DEV_NAME);
	ret = mix_hw_recording_get(set);
	if (ret < 0)
		return ret;

	ret = mix_hw_voice_settings_get(set);
	if (ret < 0)
		return ret;
	ret = mix_get_info(set);
	if (ret <0)
		return ret;

	return 0;
}

int mix_commit(struct mix_settings *set)
{
	int ret;

	ret = mix_hw_recording_set(set);
	if (ret < 0)
		return ret;

	ret = mix_hw_voice_settings_set(set);
	if (ret < 0)
		return ret;

	return 0;
}
int set_51(struct mix_settings *set,int mode){

	int fd;
	struct mixer_private_ioctl ctl;
	
	ctl.cmd = CMD_SETGPOUT;
	ctl.val[1] = mode;// set gpout to 0 or 1
	
	if ((fd = open(set->dev_name, O_WRONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}
	
	if(set->card_type){
		ctl.val[0] = 6; // the gpout pin to switch [0-8]
		if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return -1;
		}
	}
	ctl.val[0] = 2; // the gpout pin to switch [0-2]
	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}
	
	close(fd);
	return 0;
}

int set_mch_fx(struct mix_settings *set, int fx)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_SETMCH_FX;
	ctl.val[0] = fx;

	if ((fd = open(set->dev_name, O_WRONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);
	return 0;
}

int set_passthrough(struct mix_settings *set, int active)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_SETPASSTHROUGH;
	ctl.val[0] = active;

	if ((fd = open(set->dev_name, O_WRONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	close(fd);
	return 0;
}

int enable_ir(struct mix_settings *set)
{
	int fd;
	struct mixer_private_ioctl ctl;

	if ((fd = open(set->dev_name, O_WRONLY, 0)) < 0) {
		perror(set->dev_name);
		return -1;
	}

	ctl.cmd = CMD_SETGPOUT;
	ctl.val[0] = 0; // set 400 
	ctl.val[1] = 1;


	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}
	sleep(1);
	
	ctl.val[0] = 1; // set 800
	ctl.val[1] = 1;


	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	ctl.val[0] = 1; //  clear 800
	ctl.val[1] = 0;


	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	ctl.val[0] = 0; //  clear 400
	ctl.val[1] = 0;


	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	
	close(fd);
	return 0;	

}
int mix_get_info (struct mix_settings *mix)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd = CMD_PRIVATE3_VERSION;
	ctl.val[1] = ctl.val[2] = ctl.val[3]=0;

	if ((fd = open(mix->dev_name, O_WRONLY, 0)) < 0) {
		perror(mix->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}
	mix->private_version=ctl.val[0];
	mix->major_ver=ctl.val[1];
	mix->minor_ver=ctl.val[2];
	mix->card_type=ctl.val[3];
	mix->iocfg=ctl.val[4];
	close(fd);
	return 0;
}
int mix_print_info (struct mix_settings *mix)
{
	if((mix->major_ver==0)&&(mix->minor_ver==0))
		printf("Driver version: N/A\n");
	else	
		printf("Driver version: %d.%d\n",mix->major_ver, mix->minor_ver);
	printf("mixer interface version: %d\n", mix->private_version);
	printf("Card type: %s\n", mix->card_type==0 ? "SBLive":"Audigy");
	if(mix->card_type){
		//printf("IOCFG: %x\n",iocfg); 
		if(mix->iocfg&0xe100)
			printf("Output Connected:\n%s%s%s%s \n",
			       mix->iocfg&0x8000?"\tAnalog Rear\n":"",
			       mix->iocfg&0x4000?"\tAnalog Front\n":"",
			       mix->iocfg&0x2000?"\tDigital/Analog center-sub\n":"",
			       mix->iocfg&0x0100?"\tAudigy Drive Headphones":"\b"
			);
		else
			printf("No Outputs connected\n");
		printf("Output mode: %s\n", mix->iocfg&0x44?"Digital":"Analog");
		printf("Output Spdif Frequency: %d\n",  mix_get_spdif_freq(mix));
			
	}
	return 0;
	
}
int mix_get_spdif_freq(struct mix_settings *mix)
{
	int fd;
	struct mixer_private_ioctl ctl;

	ctl.cmd =CMD_READPTR;
	ctl.val[0] = 0x76;
	ctl.val[1] = 0x00;
	if(!mix->card_type)
		return -1;
	if ((fd = open(mix->dev_name, O_RDONLY, 0)) < 0) {
		perror(mix->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		close(fd);
		return -1;
	}
	close(fd);
	if(ctl.val[2] & 0x40)
		return 44;
	if(ctl.val[2] & 0x80)
		return 96;
	else
		return 48;
}

int mix_set_spdif_freq(struct mix_settings *mix, int freq)
{
	int fd;
	struct mixer_private_ioctl ctl;

	if(!mix->card_type)
		return -1;
	ctl.cmd =CMD_WRITEPTR;
	ctl.val[0] = 0x76;
	ctl.val[1] = 0x00;
        switch(freq) {
	case 44:
		ctl.val[2] = 0x40;
		break;
	case 96:
		ctl.val[2] = 0x80;
		break;
	case 48:
	default:
		ctl.val[2] = 0x00;
		break;
	}

	if ((fd = open(mix->dev_name, O_WRONLY, 0)) < 0) {
		perror(mix->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
int mix_ac97_boost(struct mix_settings *mix, int boost)
{
	int fd;
	struct mixer_private_ioctl ctl;

	if(mix->card_type) //we only do this for SBLive
		return -1;
	ctl.cmd =CMD_AC97_BOOST;
	ctl.val[0] = boost;

	if ((fd = open(mix->dev_name, O_WRONLY, 0)) < 0) {
		perror(mix->dev_name);
		return -1;
	}

	if (ioctl(fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		printf("feature not available with this driver\n");
		close(fd);
		return -1;
	}
	close(fd);

	return 0;

}

