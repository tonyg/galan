/*********************************************************************
 *      driver_access.c - functions needed to r/w to the driver
 *      Copyright (C) 2000-2001 Rui Sousa 
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <errno.h>
#include <fcntl.h>

#include "../emu10k1-include/dsp.h"
#include "../emu10k1-include/list.h"

#include "../emu10k1-include/internal.h"

/*
Theory of operation:

Patch information is stored in the driver as per the kdsp_patch struct,
the routing patch as per dsp_kernel_rpatch, and gpr usage information as
per dsp_kernel_gpr.

All other information must be re-constructed from the state of the emu10k1
itself.

dsp_init() is called to create user space structures and linked list containing
the state of the emu10k1.


 */


extern __u32 addr_change[0x400];

int get_driver_version( int mixer_fd)
{
	struct mixer_private_ioctl ctl;
	int ret;
	
	ctl.cmd = CMD_GETGPR2OSS;
	ctl.val[0] = 0;
	ctl.val[1] = 0;

	ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
	if (ret < 0) 
		// this is a very old driver (<0.14)
		return ret;

	ctl.cmd = CMD_PRIVATE3_VERSION;
	ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
	if (ret < 0) {
		// this is versions 0.14 or 0.15
		return 0;
	}
	//it's a newer driver with a private3_version number
	return ctl.val[0];
}

int dsp_load_reg(int offset, __u32 value, struct  dsp_patch_manager *mgr)
{
	copr_buffer buf;
	int ret;

	if((offset<0x000 || offset>=0x1000 ) && offset != 0x52)
		return -EINVAL;


	buf.command = CMD_WRITE;
	buf.offs =  offset;
	buf.len = 1;

	memcpy(buf.data, &value,  sizeof(__u32));

	ret = ioctl(mgr->dsp_fd, SNDCTL_COPR_LOAD, &buf);
	return ret;
}



//we check each patch and see wether they use io gpr0 or gpr1
static void determine_io_gprs(struct dsp_patch_manager *mgr)
{
	struct dsp_patch *patch;
	struct list_head *entry;



	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		determine_io(mgr, patch);
	}

	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		determine_io(mgr, patch);
	}
}

static void match_io(struct dsp_patch_manager *mgr, struct dsp_patch *patch, int *in, int gpr0, int gpr1, int i)
{
#ifdef DEBUG	
	printf("entering match_io() patch->name=%s \n\tpatch->in_gprs[i]=%x, out_gprs[i]=%x\n",patch->name,patch->in_gprs[i],patch->out_gprs[i]);
		printf("\tgpr0=%x, gpr1=%x, in=%x\n",gpr0,gpr1,*in);
#endif
	if (patch->in_gprs[i] != *in) {

		if (patch->in_gprs[i] == -1 && patch->out_gprs[i] == -1)
			return;

		
		/* match the input */
		addr_change[patch->in_gprs[i]] = *in;

		if (patch->in_gprs[i] >= GPR_BASE)
			clear_bit(patch->in_gprs[i] - GPR_BASE, patch->gpr_input);

		if (*in >= GPR_BASE)
			set_bit(*in - GPR_BASE, patch->gpr_input);

		if (patch->out_gprs[i] == *in) {
			//		printf("\tNeed to swap io gprs\n");
			/* the output and input are now swapped */
			if (patch->in_gprs[i] < GPR_BASE) {
				/* the input was not an io gpr */
				if (*in == GPR_BASE + gpr1)
					patch->in_gprs[i] = GPR_BASE + gpr0;
				else
					patch->in_gprs[i] = GPR_BASE + gpr1;

				get_gpr(mgr, patch->in_gprs[i] - GPR_BASE, patch->gpr_used);

				addr_change[patch->out_gprs[i]] = patch->in_gprs[i];
			
				*in = patch->in_gprs[i];
			}else{
				
			addr_change[patch->out_gprs[i]] = patch->in_gprs[i];
			*in = patch->in_gprs[i];
			patch->in_gprs[i]=patch->out_gprs[i];
			patch->out_gprs[i]=*in;	
			}
			
		} else {

			if (patch->in_gprs[i] >= GPR_BASE)
				free_gpr(mgr, patch->in_gprs[i] - GPR_BASE, patch->gpr_used);

			if (*in >= GPR_BASE)
				get_gpr(mgr, *in - GPR_BASE, patch->gpr_used);

			*in = patch->out_gprs[i];
		}
	} else {

		if (patch->out_gprs[i] != -1){	
			/* already matched */

			*in = patch->out_gprs[i];
		}else{

		}
		
		
	}
#ifdef DEBUG	
	printf("  exiting match_io(),\n\tpatch->in_gprs[i]=%x, out_gprs[i]=%x\n",patch->in_gprs[i],patch->out_gprs[i]);
	printf("\tgpr0=%x, gpr1=%x, in=%x\n",gpr0,gpr1,*in);
#endif
}


//FIXME: this needs to be changed to support cases were the
//input and output lines are unequal.
static void set_io_gprs(struct dsp_patch_manager *mgr)
{
	struct dsp_patch *patch,*last_patch;
	struct list_head *entry;
	int in, i, gpr0, gpr1;

	for (i = 0; i < DSP_NUM_INPUTS; i++) {
		in = i;

		gpr0 = mgr->io[i][0];
		gpr1 = mgr->io[i][1];

		list_for_each(entry, &mgr->ipatch_list) {
			patch = list_entry(entry, struct dsp_patch, list);
			if(patch_uses_input(patch,i)){

				init_addr_change_table(addr_change, 0, GPR_BASE + GPR_SIZE);

				match_io(mgr, patch, &in, gpr0, gpr1, i);
				
				search_and_replace(patch->code, patch->code_size, addr_change, 0, GPR_BASE + GPR_SIZE);
			}
		}			
		mgr->rpatch.in[i] = in;
	}

	construct_routing(mgr);

	for (i = 0; i < DSP_NUM_OUTPUTS; i++) {
		in = mgr->rpatch.out[i];

		
		gpr0 = mgr->io[i][0];
		gpr1 = mgr->io[i][1];
		
		last_patch=0;
		list_for_each(entry, &mgr->opatch_list) {
			patch = list_entry(entry, struct dsp_patch, list);
			if(patch_uses_output(patch,i)){
				init_addr_change_table(addr_change, 0, GPR_BASE + GPR_SIZE);

				match_io(mgr, patch, &in, gpr0, gpr1,i);

				search_and_replace(patch->code, patch->code_size, addr_change, 0, GPR_BASE + GPR_SIZE);
				last_patch=patch;
			}
		}
		
		
		if (last_patch!=0) {
			/* last patch in the line */
			init_addr_change_table(addr_change, 0, GPR_BASE + GPR_SIZE);

			addr_change[last_patch->out_gprs[i]] = OUTPUT_BASE + i;
			if (last_patch->out_gprs[i] >= GPR_BASE)
				free_gpr(mgr, last_patch->out_gprs[i] - GPR_BASE, last_patch->gpr_used);
			search_and_replace(last_patch->code, last_patch->code_size, addr_change, 0, GPR_BASE + GPR_SIZE);
			
			
		}
		
			
	}
}


static void set_code_start(struct dsp_patch_manager *mgr)
{
	struct dsp_patch *patch;
	struct list_head *entry;
	int code_start;


	code_start = 0;

	
	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		patch->code_start = code_start;
		code_start += patch->code_size;
	}

	mgr->rpatch.code_start = code_start;
	code_start += mgr->rpatch.code_size;

	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		patch->code_start = code_start;
		code_start += patch->code_size;
	}
}

// returns instruction # if the code contains a particular address (val), else returns -1
static  int search_code2(struct dsp_patch *patch, int val)
{
	int i;

	for (i = 0; i < patch->code_size / 2; i++)
		if (((patch->code[i * 2 + 1] >> 10) & 0x3ff) == val ||
		    (patch->code[i * 2 + 1] & 0x3ff) == val ||
		    ((patch->code[i * 2] >> 10) & 0x3ff) == val || (patch->code[i * 2] & 0x3ff) == val)
			return i;
	
	return -1;
}

//FIXME: still need verify align bit for reads
static int set_tram_align(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int instr, tram_buff, j;
	for (j = 0; j < patch->traml_isize; j++) {
		tram_buff=j + patch->traml_istart ;
		if ((instr = search_code2(patch, tram_buff+TRAML_IDATA_BASE)) == -1)
			//we shouln't be in here
			continue;

		instr+=patch->code_start/2;
		if( (patch->traml_iaddr[j]&TANKMEMADDRREG_WRITE ) && ((instr +2 )>(3 * tram_buff)) )
			patch->traml_iaddr[j] |= TANKMEMADDRREG_ALIGN;
		else if( (patch->traml_iaddr[j]&TANKMEMADDRREG_READ ) && ((instr)<(3 * tram_buff)) )
			patch->traml_iaddr[j] |= TANKMEMADDRREG_ALIGN;
		else
			patch->traml_iaddr[j] &= ~TANKMEMADDRREG_ALIGN;
	}

	for (j = 0; j < patch->traml_esize; j++) {
		tram_buff=j + patch->traml_estart ;
		if ((instr = search_code2(patch, tram_buff+TRAML_EDATA_BASE)) == -1)
			continue;
		
		instr+=patch->code_start/2;
		if( (patch->traml_eaddr[j]&TANKMEMADDRREG_WRITE ) && ((instr +2 )>(4 * tram_buff)) )
			patch->traml_eaddr[j] |= TANKMEMADDRREG_ALIGN;
		if( (patch->traml_eaddr[j]&TANKMEMADDRREG_WRITE ) && ((instr - 0x17e )>(4 * tram_buff)) )
			patch->traml_eaddr[j] |= TANKMEMADDRREG_ALIGN;

		else
			patch->traml_eaddr[j] &= ~TANKMEMADDRREG_ALIGN;
	}
	return 0;
}




static void set_tram_addr(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int j;

	/* keep internal block in internal tram */
	for (j = 0; j < patch->traml_isize; j++) {
		addr_change[patch->traml_istart + j] = TRAML_IDATA_BASE + mgr->traml_ifree_start + j;
		
		addr_change[TRAML_IADDR_BASE - TRAML_IDATA_BASE + patch->traml_istart + j] =
			TRAML_IADDR_BASE + mgr->traml_ifree_start + j;

		patch->traml_iaddr[j] += mgr->tramb_ifree_start - patch->tramb_istart;
	}
	
	patch->tramb_istart = mgr->tramb_ifree_start;
	patch->traml_istart = mgr->traml_ifree_start;

	/* keep external block in external tram */
	for (j = 0; j < patch->traml_esize; j++) {
		addr_change[TRAML_EDATA_BASE - TRAML_IDATA_BASE + patch->traml_estart + j] =
			TRAML_EDATA_BASE + mgr->traml_efree_start + j;

		addr_change[TRAML_EADDR_BASE - TRAML_IDATA_BASE + patch->traml_estart + j] =
			TRAML_EADDR_BASE + mgr->traml_efree_start + j;

		patch->traml_eaddr[j] += mgr->tramb_efree_start - patch->tramb_estart;
	}

	patch->tramb_estart = mgr->tramb_efree_start;
	patch->traml_estart = mgr->traml_efree_start;

	
	search_and_replace(patch->code, patch->code_size, addr_change, TRAML_IDATA_BASE,
			   TRAML_EADDR_BASE + TRAML_ESIZE);


	mgr->tramb_ifree_start += patch->tramb_isize;
	mgr->traml_ifree_start += patch->traml_isize;
	mgr->tramb_efree_start += patch->tramb_esize;
	mgr->traml_efree_start += patch->traml_esize;


	return;
	
}


static void set_tram(struct dsp_patch_manager *mgr)
{
	struct dsp_patch *patch;
	struct list_head *entry;

	mgr->traml_ifree_start = 0;
	mgr->traml_efree_start = 0;

	mgr->tramb_ifree_start = 0;
	mgr->tramb_efree_start = 0;



	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		set_tram_addr(mgr, patch);
		set_tram_align(mgr,patch);
	}

	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		set_tram_addr(mgr, patch);
		set_tram_align(mgr,patch);
	}
}


static int load_patch_code(__u32 * code, __u16 start, __u16 size, int fd)
{
	copr_buffer buf;
	int ret;

	buf.command = CMD_WRITE;
	buf.offs = DSP_CODE_BASE + start;
	buf.len = size < 0x200 ? size : 0x200;

	memcpy(buf.data, code, buf.len * sizeof(__u32));

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	if (buf.len < size) {
		buf.offs = DSP_CODE_BASE + start + buf.len;
		buf.len = size - buf.len;

		memcpy(buf.data, &code[buf.offs - DSP_CODE_BASE], buf.len * sizeof(__u32));

		ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
		if (ret < 0) {
			perror("SNDCTL_COPR_LOAD");
			return ret;
		}
	}

	return 0;
}
static int load_gprs(struct dsp_patch_manager *mgr, int audio_fd, int mixer_fd)
{
	copr_buffer buf;
	struct mixer_private_ioctl ctl;
	int i, j, ret;
	int vol_gprs[SOUND_MIXER_NRDEVICES][2];

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	{
		vol_gprs[i][0] = -1;
		vol_gprs[i][1] = -1;
	}

	ctl.cmd = CMD_SETGPR;

	buf.command = CMD_WRITE;
	buf.len = 1;

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		ctl.val[0] = i;
		memcpy(&ctl.val[1], &mgr->gpr[i], sizeof(struct dsp_kernel_gpr));

		ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
		if (ret < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return ret;
		}

		buf.offs = GPR_BASE + i;

		((__u32 *) buf.data)[0] = mgr->gpr[i].value;

		ret = ioctl(audio_fd, SNDCTL_COPR_LOAD, &buf);
		if (ret < 0) {
			perror("SNDCTL_COPR_LOAD");
			return ret;
		}

		if ((mgr->gpr[i].type == GPR_TYPE_CONTROL) &&
		    (mgr->gpr[i].mixer_id < SOUND_MIXER_NRDEVICES))
		{
			vol_gprs[mgr->gpr[i].mixer_id][mgr->gpr[i].mixer_ch] = i;
		}
	}

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	{
		for (j = 0; j < 2; j++)
		{
			ctl.cmd = CMD_SETGPR2OSS;
			ctl.val[0] = i;
			ctl.val[1] = j;
			ctl.val[2] = vol_gprs[i][j];

			ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
			if (ret < 0) {
				perror("SOUND_MIXER_PRIVATE3");
				return ret;
			}
		}
	}

	return 0;
}

static int load_patch_tramls(struct dsp_patch *patch, int fd)
{
	copr_buffer buf;
	int ret;

	buf.command = CMD_WRITE;
	buf.offs = TRAML_IADDR_BASE + patch->traml_istart;
	buf.len = patch->traml_isize;

	memcpy(buf.data, patch->traml_iaddr, buf.len * sizeof(__u32));

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	buf.command = CMD_WRITE;
	buf.offs = TRAML_EADDR_BASE + patch->traml_estart;
	buf.len = patch->traml_esize;

	memcpy(buf.data, patch->traml_eaddr, buf.len * sizeof(__u32));

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	return 0;
}

static int clean_free_tram_buffers(struct dsp_patch_manager *mgr, int fd)
{
	copr_buffer buf;
	int i;
	int ret;

	
	for(i=0; i<TRAML_ISIZE;i++)
		((__u32 *)buf.data)[i]=0;

	//free internal tram buffers
	buf.command = CMD_WRITE;
	buf.offs =  TRAML_IADDR_BASE + mgr->traml_ifree_start;
	buf.len =  TRAML_ISIZE - mgr->traml_ifree_start;


	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	//free external tram buffers
	buf.offs =  TRAML_EADDR_BASE + mgr->traml_efree_start;
	buf.len =  TRAML_ESIZE-mgr->traml_efree_start;

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	return 0;
}

static int clean_free_dsp(struct dsp_patch_manager *mgr, int fd)
{
	copr_buffer buf;
	int i, size;
	int ret;

	size = mgr->icode_free_start - mgr->code_free_start;

	if (size <= 0)
		return 0;

	for (i = 0; i < (size < 1000 ? size : 1000) / 2; i++) {
		((__u32 *) buf.data)[2 * i] = 0x40 << 10 | 0x40;
		((__u32 *) buf.data)[2 * i + 1] = 6 << 20 | 0x40 << 10 | 0x40;
	}

	buf.command = CMD_WRITE;
	buf.offs = DSP_CODE_BASE + mgr->code_free_start;
	buf.len = size < 0x200 ? size : 0x200;

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	if (buf.len < size) {
		buf.offs = DSP_CODE_BASE + mgr->code_free_start + buf.len;
		buf.len = size - buf.len;

		ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
		if (ret < 0) {
			perror("SNDCTL_COPR_LOAD");
			return ret;
		}
	}

	return 0;
}

static int load_patches(struct dsp_patch_manager *mgr, int audio_fd, int mixer_fd)
{
	struct mixer_private_ioctl ctl;
	struct list_head *entry;
	struct dsp_patch *patch;
	struct dsp_rpatch *rpatch;
	int id, ret;

	set_io_gprs(mgr);
	
	set_code_start(mgr);
	set_tram(mgr);


	id = 0;

	ctl.cmd = CMD_SETPATCH;
	ctl.val[0] = id;

	/* routing patch */
	rpatch = &mgr->rpatch;

	memcpy(ctl.val + 1, rpatch, sizeof(struct dsp_kernel_rpatch));

	ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
	if (ret < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return ret;
	}

	load_patch_code(rpatch->code, rpatch->code_start, rpatch->code_size, audio_fd);


	/* FIXME */
	/* don't load a patch if it wasn't modified */

	/* input patches */


	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);

		id++;
		ctl.val[0] = id;

		memcpy(ctl.val + 1, patch, sizeof(struct kdsp_patch));

		ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
		if (ret < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return ret;
		}

		ret = load_patch_code(patch->code, patch->code_start, patch->code_size, audio_fd);
		if (ret < 0)
			return ret;

		load_patch_tramls(patch, audio_fd);
	}

	/* output patches */


	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);

		id++;
		ctl.val[0] = id;

		memcpy(ctl.val + 1, patch, sizeof(struct kdsp_patch));

		ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
		if (ret < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return ret;
		}

		ret = load_patch_code(patch->code, patch->code_start, patch->code_size, audio_fd);
		if (ret < 0)
			return ret;

		load_patch_tramls(patch, audio_fd);
	}

	/* delimiter patch */

	id++;
	ctl.val[0] = id;

	((struct dsp_patch *) (ctl.val + 1))->code_size = 0;

	ret = ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
	if (ret < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return ret;
	}

	clean_free_dsp(mgr, audio_fd);
	clean_free_tram_buffers(mgr, audio_fd);
	return 0;
}

static int set_external_tram(struct dsp_patch_manager *mgr, int fd)
{
	__u32 size;
	int ret;

	/* size here is in bytes */
	size = 2 * mgr->tramb_efree_start;

	if (!size)
		return 0;

	ret = ioctl(fd, SOUND_MIXER_PRIVATE4, &size);
	if (ret < 0) {
		perror("SOUND_MIXER_PRIVATE4");
		return ret;
	}

	return 0;
}


int dsp_load(struct dsp_patch_manager *mgr)
{

	int i;
	int ret;

	if (!mgr->init)
		return -1;

	ret = set_external_tram(mgr, mgr->mixer_fd);
	if (ret < 0)
		return ret;


	ret = load_patches(mgr, mgr->dsp_fd, mgr->mixer_fd);
	if (ret < 0)
		return ret;


	ret = load_gprs(mgr, mgr->dsp_fd, mgr->mixer_fd);
	if (ret < 0)
		return ret;


	/* release io and dynamic gprs of the routing patch */
	/* this is done here so that we can call dsp_load() several times */
	for (i = 0; i < DSP_NUM_GPRS; i++)
		if (test_bit(i, mgr->rpatch.gpr_used) &&
		    (mgr->gpr[i].type == GPR_TYPE_IO ||
		     mgr->gpr[i].type == GPR_TYPE_DYNAMIC)) free_gpr(mgr, i, mgr->rpatch.gpr_used);


	return 0;

}
//generic code getting function, caller should have already malloc memory
static int get_code(__u32 *code, int start, int size, int audio_fd)
{
	copr_buffer buf;
	int ret;

	buf.command = CMD_READ;
	buf.offs = DSP_CODE_BASE + start;
	buf.len = size < 0x200 ? size : 0x200;

	ret = ioctl(audio_fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	
	memcpy(code, buf.data, buf.len * sizeof(__u32));

	if (size > buf.len) {
		buf.offs = DSP_CODE_BASE + start + buf.len;
		buf.len = size - buf.len;

		ret = ioctl(audio_fd, SNDCTL_COPR_LOAD, &buf);
		if (ret < 0) {
			perror("SNDCTL_COPR_LOAD");
			return ret;
		}

		memcpy(&(code[buf.offs - DSP_CODE_BASE]), buf.data, buf.len * sizeof(__u32));
	}
	return 0;
}

static int get_patch_code(struct dsp_patch *patch, int fd)
{
	copr_buffer buf;
	int ret;

	patch->code = (__u32 *) malloc(patch->code_size * sizeof(__u32));
	if (patch->code == NULL) {
		perror("patch code");
		return -1;
	}

	buf.command = CMD_READ;
	buf.offs = DSP_CODE_BASE + patch->code_start;
	buf.len = patch->code_size < 0x200 ? patch->code_size : 0x200;

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	memcpy(patch->code, buf.data, buf.len * sizeof(__u32));

	if (patch->code_size > buf.len) {
		buf.offs = DSP_CODE_BASE + patch->code_start + buf.len;
		buf.len = patch->code_size - buf.len;

		ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
		if (ret < 0) {
			perror("SNDCTL_COPR_LOAD");
			return ret;
		}

		memcpy(&(patch->code)[buf.offs - DSP_CODE_BASE], buf.data, buf.len * sizeof(__u32));
	}

	return 0;
}

static int get_patch_tramls(struct dsp_patch *patch, int fd)
{
	copr_buffer buf;
	int ret;

	patch->traml_iaddr = (__u32 *) malloc(patch->traml_isize * sizeof(__u32));

	if (patch->traml_iaddr == NULL) {
		perror("tram internal lines");
		return -1;
	}

	buf.command = CMD_READ;
	buf.offs = TRAML_IADDR_BASE + patch->traml_istart;
	buf.len = patch->traml_isize;

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	memcpy(patch->traml_iaddr, buf.data, buf.len * sizeof(__u32));

	patch->traml_eaddr = (__u32 *) malloc(patch->traml_esize * sizeof(__u32));

	if (patch->traml_eaddr == NULL) {
		perror("tram external lines");
		return -1;
	}

	buf.command = CMD_READ;
	buf.offs = TRAML_EADDR_BASE + patch->traml_estart;
	buf.len = patch->traml_esize;

	ret = ioctl(fd, SNDCTL_COPR_LOAD, &buf);
	if (ret < 0) {
		perror("SNDCTL_COPR_LOAD");
		return ret;
	}

	memcpy(patch->traml_eaddr, buf.data, buf.len * sizeof(__u32));

	return 0;
}

static int get_gprs(struct dsp_patch_manager *mgr, int audio_fd, int mixer_fd)
{
	copr_buffer buf;
	struct mixer_private_ioctl ctl;
	int i, j;

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		ctl.cmd = CMD_GETGPR;
		ctl.val[0] = i;

		if (ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return -1;
		}

		memcpy(&mgr->gpr[i], &ctl, sizeof(struct dsp_kernel_gpr));

		buf.command = CMD_READ;
		buf.len = 1;
		buf.offs = GPR_BASE + i;

		if (ioctl(audio_fd, SNDCTL_COPR_LOAD, &buf) < 0) {
			perror("SNDCTL_COPR_LOAD");
			return -1;
		}

		mgr->gpr[i].value = ((__u32 *) buf.data)[0];
		mgr->gpr[i].addr = i;
		mgr->gpr[i].mixer_id = SOUND_MIXER_NONE;
	}

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	{
		for (j = 0; j < 2; j++)
		{
			ctl.cmd = CMD_GETGPR2OSS;
			ctl.val[0] = i;
			ctl.val[1] = j;

			if (ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
				perror("SOUND_MIXER_PRIVATE3");
				return -1;
			}

			mgr->gpr[ctl.val[2]].mixer_id = i;
			mgr->gpr[ctl.val[2]].mixer_ch = j;
		}
	}

	return 0;
}

static int get_patches(struct dsp_patch_manager *mgr, int audio_fd, int mixer_fd)
{
	struct mixer_private_ioctl ctl;
	struct dsp_patch *patch;
	struct dsp_rpatch *rpatch;
	int i;
	
	/* input/ouput list initialization */
	
	INIT_LIST_HEAD(&mgr->ipatch_list);
	INIT_LIST_HEAD(&mgr->opatch_list);
		
	mgr->traml_efree_start = 0;
	mgr->traml_ifree_start = 0;

	mgr->tramb_efree_start = 0;
	mgr->tramb_ifree_start = 0;

	mgr->code_free_start = 0;

	ctl.cmd = CMD_GETPATCH;

	/* get routing patch, id is always 0 */
	ctl.val[0] = 0;

	if (ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return -1;
	}

	rpatch = &mgr->rpatch;

	memcpy(rpatch, &ctl, sizeof(struct dsp_kernel_rpatch));

	mgr->code_free_start += rpatch->code_size;

	/* always allocate extra space allowing the routing patch to grow later */
	rpatch->code = (__u32 *) malloc(DSP_CODE_SIZE * sizeof(__u32));
	if (rpatch->code == NULL) {
		perror("routing patch code");
		return -1;
	}
	
	get_code(rpatch->code, rpatch->code_start, rpatch->code_size,  audio_fd);

	/* get all other paches, stop when the code size is 0 */
	for (i = 1;; i++) {
		ctl.cmd = CMD_GETPATCH;
		ctl.val[0] = i;

		if (ioctl(mixer_fd, SOUND_MIXER_PRIVATE3, &ctl) < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return -1;
		}

		patch = (struct dsp_patch *) malloc(sizeof(struct dsp_patch));

		if (patch == NULL) {
			perror("patch");
			return -1;
		}

		memcpy(patch, &ctl, sizeof(struct kdsp_patch));

		if (patch->code_size == 0) {
			free(patch);
			break;
		}

		if (get_patch_code(patch, audio_fd) < 0)
			return -1;

		if (get_patch_tramls(patch, audio_fd) < 0)
			return -1;
		
		if (patch->code_start < rpatch->code_start)
			list_add_tail(&patch->list, &mgr->ipatch_list);
		else {
			list_add_tail(&patch->list, &mgr->opatch_list);

		}

		mgr->code_free_start += patch->code_size;

		mgr->traml_ifree_start += patch->traml_isize;
		mgr->traml_efree_start += patch->traml_esize;

		mgr->tramb_ifree_start += patch->tramb_isize;
		mgr->tramb_efree_start += patch->tramb_esize;

	}

	return 0;
}


/* get all the dsp code information from the driver */
int dsp_init(struct dsp_patch_manager *mgr)
{

	int ret;

	ret = get_driver_version( mgr->mixer_fd);
	if (ret < 0){
		perror("SOUND_MIXER_PRIVATE3: You're probably using an older incompatible driver");
		return ret;
	}
	
	ret = get_patches(mgr, mgr->dsp_fd, mgr->mixer_fd);
	if (ret < 0)
		return ret;



	mgr->icode_free_start = mgr->code_free_start;

	ret = get_gprs(mgr, mgr->dsp_fd, mgr->mixer_fd);
	if (ret < 0)
		return ret;

	
	init_io_gprs_table(mgr);
	determine_io_gprs(mgr);

	mgr->init = 1;

	return 0;

}
