/*********************************************************************
 *     dsp.c - dsp patch manager library
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>

#include "../emu10k1-include/dsp.h"
#include "../emu10k1-include/list.h"

#include "../emu10k1-include/internal.h"

__u32 addr_change[0x400];

__s32 constants[CONSTANTS_SIZE] = {
	0x0,
	0x1,
	0x2,
	0x3,
	0x4,
	0x8,
	0x10,
	0x20,
	0x100,
	0x10000,
	0x80000,
	0x10000000,
	0x20000000,
	0x40000000,
	0x80000000,
	0x7fffffff,
	0xffffffff,
	0xfffffffe,
	0xc0000000,
	0x4f1bbcdc,
	0x5a7ef9db,
	0x00100000
};

/* FIXME */
/* these should have better names (and probably dependent on the card model) */
char dsp_in_name[DSP_NUM_INPUTS][DSP_LINE_NAME_SIZE] =
	{ "Pcm L", "Pcm R", "fx2", "fx3", "Pcm1 L", "Pcm1 R", "fx6", "fx7",
	  "fx8", "fx9", "fx10", "fx11", "fx12", "fx13", "fx14", "fx15",
	  "Analog L", "Analog R", "CD-Spdif L", "CD-Spdif R", "in2l", "in2r", "Opt. Spdif L", "Opt. Spdif R",
	  "Line2/Mic2 L", "Line2/Mic2 R", "RCA Spdif L", "RCA Spdif R","RCA Aux L", "RCA Aux R", "in7l", "in7r"
};
char dsp_out_name[DSP_NUM_OUTPUTS][DSP_LINE_NAME_SIZE] =
    { "Front L", "Front R", "Digital L", "Digital R", "Digital Center", "Digital LFE", "Phones L", "Phones R",
	"Rear L", "Rear R", "ADC Rec L", "ADC Rec R", "Mic Rec", "out6r", "out7l", "out7r",
	"out8l", "Analog Center", "Analog LFE", "out9r", "out10l", "out10r", "out11l", "out11r",
	"out12l", "out12r", "out13l", "out13r", "out14l", "out14r", "out15l", "out15r"
};
char dsp_stereo_in_name[DSP_NUM_INPUTS/2][DSP_LINE_NAME_SIZE] =
	{ "Pcm", "fx2-3", "Pcm1", "fx6-7",
	  "fx8-9", "fx10-11", "fx12-13", "fx14-15",
	  "Analog", "CD-Spdif", "in2", "Opt. Spdif",
	  "Line2/Mic2", "RCA Spdif","RCA Aux", "in7"
};
char dsp_stereo_out_name[DSP_NUM_OUTPUTS/2][DSP_LINE_NAME_SIZE] =
    { "Front", "Digital", "Digital Ctr/lfe", "Phones",
	"Rear", "ADC Rec", "out6", "out7",
	"out8", "out9", "out10", "out11",
	"out12", "out13", "out14", "out15"
};
char *oss_mixer_name[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;


int find_control_gpr_addr(struct dsp_patch_manager *mgr, const char *patch_name, const char *gpr_name)
{
	struct dsp_rpatch *rpatch;
	struct dsp_patch *patch;
	struct list_head *entry;
	char s[DSP_PATCH_NAME_SIZE + 4];
	__u32 *gpr_used;
	int i;
	
	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (patch->id)
			sprintf(s, "%s %d", patch->name, patch->id);
		else
			sprintf(s, "%s", patch->name);

		if (!strcmp(s, patch_name)) {
			gpr_used = patch->gpr_used;
			goto match;
			
		}
	}
	
	rpatch = &mgr->rpatch;
	if (!strcmp(rpatch->name, patch_name)) {
		gpr_used = rpatch->gpr_used;
		goto match;
	}

	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (patch->id)
			sprintf(s, "%s %d", patch->name, patch->id);
		else
			sprintf(s, "%s", patch->name);

		if (!strcmp(s, patch_name)) {
			gpr_used = patch->gpr_used;
			goto match;
		}
		}

	return -1;

      match:

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		if ((mgr->gpr[i].type == GPR_TYPE_CONTROL)
		    && test_bit(i, gpr_used)
		    && !strcmp(mgr->gpr[i].name, gpr_name))
			return GPR_BASE + i;
	}

	return -1;
}

int get_constant_gpr(struct dsp_patch_manager *mgr, __s32 val, __u32 * used)
{
	int i;

	for (i = 0; i < CONSTANTS_SIZE; i++)
		/* if it's a hardwired constant use it */
		if (constants[i] == val)
			return CONSTANTS_BASE + i;

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* see if he have this global gpr already */
		if (mgr->gpr[i].type == GPR_TYPE_CONSTANT && mgr->gpr[i].value == val && mgr->gpr[i].usage < 255)
			goto match;
	}

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* assign a new free GPR */
		if (mgr->gpr[i].type == GPR_TYPE_NULL){
			mgr->gpr[i].usage=0;
			goto match;
		}
	}
	

	fprintf(stderr, "get_constant_gpr(): no available gprs\n");
	return -1;

      match:

	if (!test_and_set_bit(i, used)) {
		mgr->gpr[i].type = GPR_TYPE_CONSTANT;
		mgr->gpr[i].addr = i;
		mgr->gpr[i].usage++;
		mgr->gpr[i].value = val;
	}

	return GPR_BASE + i;
}

int get_dynamic_gpr(struct dsp_patch_manager *mgr, __u32 * used)
{
	int i;

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* overlap DYNAMIC gprs from different patches */
		if ((mgr->gpr[i].type == GPR_TYPE_DYNAMIC)
		    && !test_bit(i, used) && mgr->gpr[i].usage < 255)
			goto match;
	}

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* assign a new free GPR */
		if (mgr->gpr[i].type == GPR_TYPE_NULL){
			mgr->gpr[i].usage=0;
			goto match;
			
		}
		
	}

	fprintf(stderr, "get_dynamic_gpr(): no available gprs\n");
	return -1;

      match:

	set_bit(i, used);
	mgr->gpr[i].type = GPR_TYPE_DYNAMIC;
	mgr->gpr[i].addr = i;
	mgr->gpr[i].usage++;

	return i + GPR_BASE;
}

int get_static_gpr(struct dsp_patch_manager *mgr, __s32 val, __u32 * used)
{
	int i;

	/* these can't be shared so just get a new one */
	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* free */
		if (mgr->gpr[i].type == GPR_TYPE_NULL)
			goto match;
	}

	fprintf(stderr, "get_static_gpr(): no available gprs\n");
	return -1;

      match:

	set_bit(i, used);
	mgr->gpr[i].type = GPR_TYPE_STATIC;
	mgr->gpr[i].addr = i;
	mgr->gpr[i].usage=1;
	mgr->gpr[i].value = val;

	return GPR_BASE + i;
}

/* We don't match IO gprs here, we just don't waste any (maximum of 2 per line) */
int get_io_gpr(struct dsp_patch_manager *mgr, int line, int type, __u32 * used, __u32 * input)
{
	int i;

	line &= 0x1f;

	/* return an already allocated one (but not in use by this patch */
	if (mgr->io[line][0] >= 0) {
		if (!test_bit(mgr->io[line][0], used)) {
			i = mgr->io[line][0];
			goto match;
		}
	}

	if (mgr->io[line][1] >= 0) {
		if (!test_bit(mgr->io[line][1], used)) {
			i = mgr->io[line][1];
			goto match;
		}
	}

	/* get a free one */
	for (i = 0; i < DSP_NUM_GPRS; i++) {
		if (mgr->gpr[i].type == GPR_TYPE_NULL) {
			mgr->gpr[i].usage=0;
			if (mgr->io[line][0] < 0)
				mgr->io[line][0] = i;
			else if (mgr->io[line][1] < 0)
				mgr->io[line][1] = i;
			else {
				fprintf(stderr, "get_io_gpr(): more than two io gprs per line\n");
				return -1;
			}
			goto match;
		}
	}

	fprintf(stderr, "get_io_gpr(): no available gprs\n");
	return -1;

      match:

	set_bit(i, used);
	if (type == GPR_INPUT)
		set_bit(i, input);

	mgr->gpr[i].type = GPR_TYPE_IO;
	mgr->gpr[i].addr = i;
	mgr->gpr[i].line = line;
	mgr->gpr[i].usage++;
	mgr->gpr[i].value = 0;

	return GPR_BASE + i;
}

int get_control_gpr(struct dsp_patch_manager *mgr, __s32 val, __s32 min, __s32 max,
			   const char *name, __u32 * used, const char *patch_name)
{
	int i;

	//if another instance of same patch is loaded, we'll use it's control gpr:
	if(patch_name!=NULL)
		if( (i=find_control_gpr_addr(mgr, patch_name, name)) != -1){
			mgr->gpr[i-GPR_BASE].usage++;
			set_bit(i-GPR_BASE, used);
			return  i;
		}

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		/* free */
		if (mgr->gpr[i].type == GPR_TYPE_NULL){
			mgr->gpr[i].usage=0;
			mgr->gpr[i].mixer_id = SOUND_MIXER_NONE;
			goto match;
		}
	}

	fprintf(stderr, "get_control_gpr(): no available gprs\n");
	return -1;

      match:

	set_bit(i, used);
	mgr->gpr[i].type = GPR_TYPE_CONTROL;
	mgr->gpr[i].addr = i;
	mgr->gpr[i].value = val;
	mgr->gpr[i].min_val = min;
	mgr->gpr[i].max_val = max;
	mgr->gpr[i].usage++;
	strncpy(mgr->gpr[i].name, name, DSP_GPR_NAME_SIZE);
	mgr->gpr[i].name[DSP_GPR_NAME_SIZE - 1] = '\0';

	return GPR_BASE + i;
}

/* get an already allocated gpr by address */
int get_gpr(struct dsp_patch_manager *mgr, int addr, __u32 * used)
{
	if (test_bit(addr, used))
		return GPR_BASE + addr;
	else {
		if (mgr->gpr[addr].type == GPR_TYPE_NULL)
			return -1;

		if (mgr->gpr[addr].usage == 1 && mgr->gpr[addr].type == GPR_TYPE_STATIC)
			return -1;

		if (mgr->gpr[addr].usage == 255)
			return -1;

		set_bit(addr, used);
		mgr->gpr[addr].usage++;
	}

	return GPR_BASE + addr;
}

int free_gpr(struct dsp_patch_manager *mgr, int addr, __u32 * used)
{
	if (!test_bit(addr, used))
		return -1;

	clear_bit(addr, used);

	mgr->gpr[addr].usage--;

	if (mgr->gpr[addr].usage == 0) {
		if (mgr->gpr[addr].type == GPR_TYPE_IO) {
			if (mgr->io[mgr->gpr[addr].line][0] == addr)
				mgr->io[mgr->gpr[addr].line][0] = -1;
			else
				mgr->io[mgr->gpr[addr].line][1] = -1;
		}

		mgr->gpr[addr].type = GPR_TYPE_NULL;
	}

	return 0;
}

static void free_all_gprs(struct dsp_patch_manager *mgr, __u32 * used)
{
	int i;

	for (i = 0; i < DSP_NUM_GPRS; i++)
		free_gpr(mgr, i, used);
}



/* returns 1 if the code contains a particular address (val) */
static int search_code(struct dsp_patch *patch, int val)
{
	int i;

	for (i = 0; i < patch->code_size / 2; i++)
		if (((patch->code[i * 2 + 1] >> 10) & 0x3ff) == val ||
		    (patch->code[i * 2 + 1] & 0x3ff) == val ||
		    ((patch->code[i * 2] >> 10) & 0x3ff) == val || (patch->code[i * 2] & 0x3ff) == val)
			return 1;

	return 0;
}

void init_io_gprs_table(struct dsp_patch_manager *mgr)
{
	int line, i;

	for (i = 0; i < DSP_NUM_INPUTS; i++) {
		mgr->io[i][0] = -1;
		mgr->io[i][1] = -1;
	}

	/* construct table of possible io gprs for each line */
	for (i = 0; i < DSP_NUM_GPRS; i++)
		if (mgr->gpr[i].type == GPR_TYPE_IO) {
			line = mgr->gpr[i].line & 0x3f;
			if (mgr->io[line][0] == -1)
				mgr->io[line][0] = mgr->gpr[i].addr;
			else if (mgr->io[line][1] == -1)
				mgr->io[line][1] = mgr->gpr[i].addr;
			else {
				fprintf(stderr, "init_io_gprs_table(): more than 2 io gprs\n");
				exit(EXIT_FAILURE);
			}
		}
}



void determine_io(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int gpr0, gpr1;
	int i;

	for (i=0;i<DSP_NUM_INPUTS;i++){
		if(test_bit(i,&patch->input)){
			gpr0 = mgr->io[i][0];
			gpr1 = mgr->io[i][1];
			/* determine patch input */
			if (gpr0 != -1 && test_bit(gpr0, patch->gpr_input))
				patch->in_gprs[i] = GPR_BASE + gpr0;
			else if (gpr1 != -1 && test_bit(gpr1, patch->gpr_input))
				patch->in_gprs[i] = GPR_BASE + gpr1;
			else if (search_code(patch, i))
				patch->in_gprs[i] = i;
			else
				patch->in_gprs[i] = -1;
		}
	}
	
	for(i=0;i<DSP_NUM_OUTPUTS;i++){
		if(test_bit(i,&patch->output)){
			
			gpr0 = mgr->io[i][0];
			gpr1 = mgr->io[i][1];
			
			/* determine patch output */
			if (gpr0 != -1 && test_bit(gpr0, patch->gpr_used) && !test_bit(gpr0, patch->gpr_input))
				patch->out_gprs[i] = GPR_BASE + gpr0;
			else if (gpr1 != -1 && test_bit(gpr1, patch->gpr_used) && !test_bit(gpr1, patch->gpr_input))
				patch->out_gprs[i] = GPR_BASE + gpr1;
			else if (search_code(patch, OUTPUT_BASE + i))
				patch->out_gprs[i] = OUTPUT_BASE + i;
			else
				patch->out_gprs[i] = -1;
		}
	}
}

void init_addr_change_table(__u32 * table, __u32 min, __u32 max)
{
	int i;

	for (i = min; i < max; i++)
		table[i - min] = i;

}

void search_and_replace(__u32 * code, __u32 size, __u32 * table, __u32 min, __u32 max)
{
	__u32 operand[2];
	int i;

	for (i = 0; i < size; i++) {
		operand[0] = code[i] & 0x000003ff;
		operand[1] = (code[i] & 0x000ffc00) >> 10;

		if (operand[0] < max && operand[0] >= min)
			operand[0] = table[operand[0] - min];

		if (operand[1] < max && operand[1] >= min)
			operand[1] = table[operand[1] - min];

		code[i] = (code[i] & ~0x000fffff) | ((operand[1] & 0x3ff) << 10)
		    | (operand[0] & 0x3ff);
	}

	return;
}

int dsp_find_patch(struct dsp_patch_manager *mgr, const char *name, struct dsp_patch **patch_in)
{
	struct dsp_patch *patch;
	struct list_head *entry;
	char s[DSP_PATCH_NAME_SIZE + 4];

	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (patch->id)
			sprintf(s, "%s %d", patch->name, patch->id);
		else
			sprintf(s, "%s", patch->name);

		if (!strcmp(s, name))	{
			*patch_in=patch;
			return 0;
		}
	}
	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);

		if (patch->id)
			sprintf(s, "%s %d", patch->name, patch->id);
		else
			sprintf(s, "%s", patch->name);

		if (!strcmp(s, name)){
			*patch_in=patch;
			return 0;
		}
	}
	*patch_in=NULL;
	return -1;
}

int dsp_unload_patch(struct dsp_patch_manager *mgr, const char *name)
{
	struct dsp_patch *patch;


	if (!mgr->init)
		dsp_init(mgr);

	if(dsp_find_patch(mgr, name, &patch) < 0 )
		return -1;
	
	list_del(&patch->list);
	free_all_gprs(mgr, patch->gpr_used);
	mgr->code_free_start -= patch->code_size;
	mgr->traml_efree_start -= patch->traml_esize;
	mgr->traml_ifree_start -= patch->traml_isize;
	mgr->tramb_efree_start -= patch->tramb_esize;
	mgr->tramb_ifree_start -= patch->tramb_isize;
	free(patch->traml_iaddr);
	free(patch->traml_eaddr);
	free(patch->code);
	free(patch);

	return 0;
}

int dsp_set_patch_name(struct dsp_patch_manager *mgr, const char *patch, const char *new)
{
	return -1;
}

int dsp_set_control_gpr_value(struct dsp_patch_manager *mgr, const char *patch, const char *gpr, __s32 val)
{
	struct mixer_private_ioctl ctl;
	int addr,ret;

	if (mgr->init) {
		addr = find_control_gpr_addr(mgr, patch, gpr);
		if (addr < 0)
			return addr;

		addr -= GPR_BASE;

		if (val > mgr->gpr[addr].max_val)
			val = mgr->gpr[addr].max_val;
		else if (val < mgr->gpr[addr].min_val)
			val = mgr->gpr[addr].min_val;

		mgr->gpr[addr].value = val;
	}


	ctl.cmd = CMD_SETCTLGPR;
	memcpy((char *) ctl.val, patch, DSP_PATCH_NAME_SIZE);
	memcpy((char *) ctl.val + DSP_PATCH_NAME_SIZE, gpr, DSP_PATCH_NAME_SIZE);
	memcpy((char *) ctl.val + 2 * DSP_PATCH_NAME_SIZE, &val, sizeof(__s32));

	ret = ioctl(mgr->mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
	if (ret < 0) {
		perror("SOUND_MIXER_PRIVATE3");
		return ret;
	}

	return 0;
}

int dsp_get_control_gpr_value(struct dsp_patch_manager *mgr, const char *patch, const char *gpr, __s32 * val)
{
	struct mixer_private_ioctl ctl;
	int addr;
	int ret;

	if (mgr->init) {
		addr = find_control_gpr_addr(mgr, patch, gpr);
		if (addr < 0)
			return addr;

		addr -= GPR_BASE;

		*val = mgr->gpr[addr].value;
	} else {

		ctl.cmd = CMD_GETCTLGPR;
		memcpy(ctl.val, patch, DSP_PATCH_NAME_SIZE);
		memcpy((char *) ctl.val + DSP_PATCH_NAME_SIZE, gpr, DSP_PATCH_NAME_SIZE);

		ret = ioctl(mgr->mixer_fd, SOUND_MIXER_PRIVATE3, &ctl);
		if (ret < 0) {
			perror("SOUND_MIXER_PRIVATE3");
			return ret;
		}

		*val = (__s32) ctl.val[0];
	}

	return 0;
}

static void construct_input_buffer(struct dsp_patch_manager *mgr, __u32 * operand, __u32 * buffer_code,
				   int *buffer_size)
{
	int addr;
	int i;

	for (i = 1; i < 3; i++)
		if (operand[i] < OUTPUT_BASE && operand[i + 1] < OUTPUT_BASE &&
		    addr_change[operand[i]] < OUTPUT_BASE && addr_change[operand[i + 1]] < OUTPUT_BASE) {
			addr = get_dynamic_gpr(mgr, mgr->rpatch.gpr_used);
			addr_change[operand[i]] = addr;
			buffer_code[*buffer_size + 1] = 0x6 << 20 | addr << 10 | operand[i];
			buffer_code[*buffer_size] = 0x40 << 10 | 0x40;
			*buffer_size += 2;
		}
}

int construct_routing(struct dsp_patch_manager *mgr)
{
	struct dsp_rpatch *rpatch;
	struct list_head *entry;
	struct dsp_patch *patch;
	__u32 op, operand[4];
	char name[DSP_GPR_NAME_SIZE];
	__u32 buffer_code[DSP_NUM_INPUTS * 2];
	int buffer_size;
	int i, j;
	int route, route_v;
	int addr;

	/* release io and dynamic gprs of the routing patch */
	for (i = 0; i < DSP_NUM_GPRS; i++)
		if (test_bit(i, mgr->rpatch.gpr_used) &&
		    (mgr->gpr[i].type == GPR_TYPE_IO ||
		     mgr->gpr[i].type == GPR_TYPE_DYNAMIC)) free_gpr(mgr, i, mgr->rpatch.gpr_used);

	rpatch = &mgr->rpatch;

	for (i = 0; i < DSP_NUM_INPUTS; i++)
		if (rpatch->in[i] >= GPR_BASE) {
			get_gpr(mgr, rpatch->in[i] - GPR_BASE, rpatch->gpr_used);
			set_bit(rpatch->in[i] - GPR_BASE, rpatch->gpr_input);
		}

	for (i = 0; i < DSP_NUM_OUTPUTS; i++)
		rpatch->out[i] = -1;

	buffer_size = 0;
	mgr->code_free_start -= rpatch->code_size;
	rpatch->code_size = 0;

	init_addr_change_table(addr_change, 0, OUTPUT_BASE);

	for (i = 0; i < DSP_NUM_OUTPUTS; i++) {
		if (mgr->rpatch.route[i] != 0 || mgr->rpatch.route_v[i] != 0) {
			route = 0;
			route_v = 0;
			j = 1;
			op = 0x6;

			while (route < DSP_NUM_INPUTS || route_v < DSP_NUM_INPUTS) {

				/* find next active route */
				while (!test_bit(route, &mgr->rpatch.route[i])
				       && route < DSP_NUM_INPUTS)
					route++;

				while (!test_bit(route_v, &mgr->rpatch.route_v[i])
				       && route_v < DSP_NUM_INPUTS)
					route_v++;

				/* a complete op and there are more arguments */
				if (j == 4 && (route < DSP_NUM_INPUTS || route_v < DSP_NUM_INPUTS)) {
					/* if we need an output gpr use it also for temporary storage */
					if (rpatch->out[i] < 0)
						rpatch->out[i] =
						    get_io_gpr(mgr, i, GPR_OUTPUT, rpatch->gpr_used, rpatch->gpr_input);

					if (rpatch->out[i] < 0)
						return -1;

					operand[0] = rpatch->out[i];

					rpatch->code[rpatch->code_size + 1] = op << 20 | operand[0] << 10 | operand[1];


					rpatch->code[rpatch->code_size] = operand[2] << 10 | operand[3];

					rpatch->code_size += 2;

					construct_input_buffer(mgr, operand, buffer_code, &buffer_size);

					op = 0x6;
					operand[1] = rpatch->out[i];
					j = 2;
				}

				if ((j == 2 || route >= DSP_NUM_INPUTS) && route_v < DSP_NUM_INPUTS) {
					if (j == 1)
						operand[1] = 0x40;

					operand[2] = rpatch->in[route_v];

					sprintf(name, "Vol %s:%s", dsp_in_name[route_v], dsp_out_name[i]);

					/* find control gpr for this route */
					/* if there isn't any create one */
					addr = find_control_gpr_addr(mgr, rpatch->name, name);
					if (addr < 0) {
						addr =
						    get_control_gpr(mgr, 0x7fffffff, 0, 0x7fffffff,
								    name, rpatch->gpr_used, NULL);
						if (addr < 0)
							return -1;
					}

					op = 0x0;
					operand[3] = addr;
					j = 4;
					route_v++;

				} else if (route < DSP_NUM_INPUTS) {
					operand[j++] = rpatch->in[route];
					route++;
				}
			}

			while (j < 4)
				operand[j++] = 0x40;

			addr = rpatch->out[i];
			rpatch->out[i] = OUTPUT_BASE + i;
			
			list_for_each(entry, &mgr->opatch_list) {

				patch = list_entry(entry, struct dsp_patch, list);
				//Reminder: check this to make sure its ok:
				if(patch_uses_output(patch,i)){
					
					if (patch->in_gprs[i] > 0) {
						if (addr < 0)
							rpatch->out[i] = get_io_gpr(mgr, i,
										    GPR_OUTPUT, rpatch->gpr_used,
										    rpatch->gpr_input);
						else
							rpatch->out[i] = addr;
						break;
					}
				}
				   
			}

			operand[0] = rpatch->out[i];

			rpatch->code[rpatch->code_size + 1] = op << 20 | operand[0] << 10 | operand[1];

			rpatch->code[rpatch->code_size] = operand[2] << 10 | operand[3];
			rpatch->code_size += 2;

			construct_input_buffer(mgr, operand, buffer_code, &buffer_size);
		}
	}

	search_and_replace(rpatch->code, rpatch->code_size, addr_change, 0, OUTPUT_BASE);

	if (buffer_size + rpatch->code_size > DSP_CODE_SIZE) {
		fprintf(stderr, "routing patch to big\n");
		return -1;
	}

	memmove(rpatch->code + buffer_size, rpatch->code, rpatch->code_size * sizeof(__u32));
	memcpy(rpatch->code, buffer_code, buffer_size * sizeof(__u32));
	rpatch->code_size += buffer_size;
	mgr->code_free_start += rpatch->code_size;

	if (mgr->code_free_start > DSP_CODE_SIZE) {
		fprintf(stderr, "no free dsp code memory\n");
		return -1;
	}

	return 0;
}


int get_input_name(const char *input)
{
	int i;

	for (i = 0; i < DSP_NUM_INPUTS; i++)
		if (!strcasecmp(input, dsp_in_name[i]))
			return i;

	return -1;
}

int get_output_name(const char *output)
{
	int i;

	for (i = 0; i < DSP_NUM_OUTPUTS; i++)
		if (!strcasecmp(output, dsp_out_name[i]))
			return i;

	return -1;
}

int get_stereo_input_name(const char *input)
{
	int i;

	for (i = 0; i < DSP_NUM_INPUTS/2; i++)
		if (!strcasecmp(input, dsp_stereo_in_name[i]))
			return i*2;
	return -1;
}

int get_stereo_output_name(const char *output)
{
	int i;

	for (i = 0; i < DSP_NUM_OUTPUTS/2; i++)
		if (!strcasecmp(output, dsp_stereo_out_name[i]))
			return i*2;
	return -1;
}

static int get_stereo_route_name(struct dsp_patch_manager *mgr, const char *name, int *in1, int *in2, int *out1, int *out2)
{
	char route_name[2 *DSP_LINE_NAME_SIZE +2],*out_ptr;
	int ret=0;

	strcpy(route_name,name);
	strtok_r(route_name,":" , &out_ptr);

	if((ret = get_input_name(route_name)) >= 0){
		*in1=ret;
		*in2=-1;
	}else if((ret = get_stereo_input_name(route_name)) >= 0){
		*in1=ret;
		*in2=ret+1;
	}else
		return -1;

	if((ret = get_output_name(out_ptr)) >= 0){
		*out1=ret;
		*out2=-1;
	}else if ( (ret = get_stereo_output_name(out_ptr)) >= 0 ){	
		*out1=ret;
		*out2=ret+1;
	}else
		return -1;
	
	
	return 0;
}

static int get_route_name(struct dsp_patch_manager *mgr, const char *name, int *in, int *out)
{
	char route_name[2 * DSP_LINE_NAME_SIZE + 2];

	for (*in = 0; *in < DSP_NUM_INPUTS; (*in)++)
		for (*out = 0; *out < DSP_NUM_OUTPUTS; (*out)++) {
			sprintf(route_name, "%s:%s", dsp_in_name[*in], dsp_out_name[*out]);
			if (!strcmp(name, route_name))
				return 0;
		}

	return -1;
}

int dsp_check_input(struct dsp_patch_manager *mgr, int in)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;
	int i;

	if (in < 0 || in > DSP_NUM_INPUTS)
		return 0;

	for (i = 0; i < DSP_NUM_OUTPUTS; i++)
		if (test_bit(in, &rpatch->route[i]) || test_bit(in, &rpatch->route_v[i]))
			return 1;

	return 0;
}

int dsp_check_input_name(struct dsp_patch_manager *mgr, const char *input)
{
	int in;

	in = get_input_name(input);
	return dsp_check_input(mgr, in);
}

int dsp_check_output(struct dsp_patch_manager *mgr, int out)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;

	if (out < 0 || out > DSP_NUM_OUTPUTS)
		return 0;

	if (rpatch->route[out] || rpatch->route_v[out])
		return 1;

	return 0;
}

int dsp_check_output_name(struct dsp_patch_manager *mgr, const char *output)
{
	int out;

	out = get_output_name(output);
	return dsp_check_output(mgr, out);
}

int dsp_check_route(struct dsp_patch_manager *mgr, int in, int out)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;

	if (in < 0 || in > DSP_NUM_INPUTS || out < 0 || out > DSP_NUM_OUTPUTS)
		return 0;

	if (test_bit(in, &rpatch->route[out]) || test_bit(in, &rpatch->route_v[out]))
		return 1;

	return 0;
}

int dsp_check_route_volume(struct dsp_patch_manager *mgr, int in, int out)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;

	if (in < 0 || in > DSP_NUM_INPUTS || out < 0 || out > DSP_NUM_OUTPUTS)
		return 0;

	if (test_bit(in, &rpatch->route_v[out]))
		return 1;

	return 0;
}

int dsp_check_route_name(struct dsp_patch_manager *mgr, const char *name)
{
	int in, out;

	get_route_name(mgr, name, &in, &out);
	return dsp_check_route(mgr, in, out);
}

int dsp_input_unused( struct list_head *list, int line)
{
	struct list_head *entry;
	struct dsp_patch *patch;

	list_for_each(entry, list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (test_bit(line,&patch->input))
			return 0;
	}
	return 1;
}

int dsp_output_unused( struct list_head *list, int line)
{
	struct list_head *entry;
	struct dsp_patch *patch;

	list_for_each(entry, list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (test_bit(line,&patch->output))
			return 0;
	}
	return 1;
}

	
int dsp_del_route(struct dsp_patch_manager *mgr, int in, int out)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;
	char s[2 * DSP_LINE_NAME_SIZE + 5];
	int addr;
	int i;

	if (in < 0 || in > DSP_NUM_INPUTS || out < 0 || out > DSP_NUM_OUTPUTS)
		return -1;

	if (!mgr->init)
		dsp_init(mgr);

	if (test_and_clear_bit(in, &rpatch->route[out])) {
		/* don't remove the route if it is the last one connected to a dsp program */
		if ( ( rpatch->route[out] || rpatch->route_v[out] || dsp_output_unused(&mgr->opatch_list,out) )) {

			if (dsp_input_unused(&mgr->ipatch_list,in))
			{
				
				return 0;
			}
			
			for (i = 0; i < DSP_NUM_OUTPUTS; i++)
				if ( test_bit(in, &rpatch->route[i]) || test_bit(in, &rpatch->route_v[i]))
					return 0;
		}

		set_bit(in, &rpatch->route[out]);
		return -2;
	} else if (test_and_clear_bit(in, &rpatch->route_v[out])) {
		sprintf(s, "Vol %s:%s", dsp_in_name[in], dsp_out_name[out]);
		addr = find_control_gpr_addr(mgr, rpatch->name, s) - GPR_BASE;
		if (addr < 0)
			return -2;

		if (rpatch->route_v[out] || rpatch->route[out] ||dsp_output_unused(&mgr->opatch_list,out)) {

			if (dsp_input_unused(&mgr->ipatch_list,in)){
				free_gpr(mgr, addr, rpatch->gpr_used);
				return 0;
			}

			for (i = 0; i < DSP_NUM_OUTPUTS; i++)
				if (test_bit(in, &rpatch->route[i]) || test_bit(in, &rpatch->route_v[i])) {
					free_gpr(mgr, addr, rpatch->gpr_used);
					return 0;
				}
		}

		set_bit(in, &rpatch->route_v[out]);
		return -2;
	}

	return -3;
}

int dsp_del_route_name(struct dsp_patch_manager *mgr, const char *name)
{
	int in1, in2, out1, out2, ret;

	if(!get_stereo_route_name(mgr, name, &in1, &in2, &out1, &out2)){
		if((ret=dsp_del_route(mgr, in1, out1)))
			return ret;
		if(in2<0 && out2>0 )
				return dsp_del_route(mgr, in1, out2);
		else if(in2 > 0)
			if( out2>0)
				return dsp_del_route(mgr, in2, out2); 
			else
				return dsp_del_route(mgr, in2, out1);
		else
			return 0;
		}
	return -1;
}

int dsp_add_route(struct dsp_patch_manager *mgr, int in, int out)
{
	struct dsp_rpatch *rpatch = &mgr->rpatch;
	char s[2 * DSP_LINE_NAME_SIZE + 5];
	int addr;

	if (in < 0 || in > DSP_NUM_INPUTS || out < 0 || out > DSP_NUM_OUTPUTS)
		return -1;

	if (!mgr->init)
		dsp_init(mgr);

	set_bit(in, &rpatch->route[out]);

	if (test_bit(in, &rpatch->route_v[out])) {
		sprintf(s, "Vol %s:%s", dsp_in_name[in], dsp_out_name[out]);
		addr = find_control_gpr_addr(mgr, rpatch->name, s) - GPR_BASE;
		if (addr < 0)
			return -1;

		free_gpr(mgr, addr, rpatch->gpr_used);
		clear_bit(in, &rpatch->route_v[out]);
	}

	return 0;
}

int dsp_add_route_name(struct dsp_patch_manager *mgr, const char *name)
{
	int in1, in2, out1, out2, ret;

	if(!get_stereo_route_name(mgr, name, &in1, &in2, &out1, &out2)){

		if((ret=dsp_add_route(mgr, in1, out1)))
			return ret;
		if(in2<0 && out2>0 )
				return dsp_add_route(mgr, in1, out2);
		else if(in2 >= 0)
			if( out2>=0)
				return dsp_add_route(mgr, in2, out2); 
			else
				return dsp_add_route(mgr, in2, out1);
		else
			return 0;
		}
	return -1;

}


int dsp_add_route_v(struct dsp_patch_manager *mgr, int in, int out)
{
	char gpr_name[DSP_GPR_NAME_SIZE];

	if (in < 0 || in > DSP_NUM_INPUTS || out < 0 || out > DSP_NUM_OUTPUTS)
		return -1;

	if (!mgr->init)
		dsp_init(mgr);

	if (!test_and_set_bit(in, &mgr->rpatch.route_v[out])) {
		sprintf(gpr_name, "Vol %s:%s", dsp_in_name[in], dsp_out_name[out]);
		if (get_control_gpr(mgr, 0x7fffffff, 0, 0x7fffffff, gpr_name, mgr->rpatch.gpr_used, NULL) < 0)
			return -1;
	}

	clear_bit(in, &mgr->rpatch.route[out]);
	return 0;
}

int dsp_add_route_v_name(struct dsp_patch_manager *mgr, const char *name)
{	
	int in1, in2, out1, out2, ret;

	if(!get_stereo_route_name(mgr, name, &in1, &in2, &out1, &out2)){

		if((ret=dsp_add_route_v(mgr, in1, out1)))
			return ret;
		if(in2<0 && out2>0 )
				return dsp_add_route_v(mgr, in1, out2);
		else if(in2 >= 0)
			if( out2>=0)
				return dsp_add_route_v(mgr, in2, out2); 
			else
				return dsp_add_route_v(mgr, in2, out1);
		else
			return 0;
		}
	return -1;
}

void dsp_print_inputs_name(void)
{
	int i;

	printf("Inputs:\n");

	for (i = 0; i < DSP_NUM_INPUTS; i++)
		printf("  %s\n", dsp_in_name[i]);
}
void dsp_print_outputs_name(void)
{
	int i;

	printf("Outputs:\n");

	for (i = 0; i < DSP_NUM_OUTPUTS; i++)
		printf("  %s\n", dsp_out_name[i]);
}

int dsp_set_oss_control_gpr(struct dsp_patch_manager *mgr, const char *patch_name, const char *gpr_name, const char *mixer_name)
{
	int addr, i, mix, ch;

	static char channel_name[2][3] = { "_l", "_r" };

	for (mix = 0; mix < SOUND_MIXER_NRDEVICES; mix++) {
		i = strlen(oss_mixer_name[mix]);

		if (strncmp(mixer_name, oss_mixer_name[mix], i) == 0) {
			ch = 0;
			if (mixer_name[i] == '\0')
				goto match;

			for (; ch < 2; ch++) {
				if (strcmp(&mixer_name[i], channel_name[ch]) == 0)
					goto match;
			}
		}
	}
	
	return -1;

 match:
	if (!mgr->init)
		dsp_init(mgr);

	for (i = 0; i < DSP_NUM_GPRS; i++) {
		if ((mgr->gpr[i].mixer_id == mix) &&
		    (mgr->gpr[i].mixer_ch == ch))
			mgr->gpr[i].mixer_id = SOUND_MIXER_NONE;
	}

	if((*patch_name==0) &&(*gpr_name==0))
		return 0;
	
	addr = find_control_gpr_addr(mgr, patch_name, gpr_name);
	if (addr > 0) {
		mgr->gpr[addr - GPR_BASE].mixer_id = mix;
		mgr->gpr[addr - GPR_BASE].mixer_ch = ch;
		return 0;
	}else
		return -2;
}

void dsp_unload_all(struct dsp_patch_manager *mgr)
{
	int i;

	for (i = 0; i < DSP_NUM_INPUTS; i++)
		INIT_LIST_HEAD(&mgr->ipatch_list);

	for (i = 0; i < DSP_NUM_OUTPUTS; i++)
		INIT_LIST_HEAD(&mgr->opatch_list);

	mgr->traml_efree_start = 0;
	mgr->traml_ifree_start = 0;

	mgr->tramb_efree_start = 0;
	mgr->tramb_ifree_start = 0;

	mgr->code_free_start = 0;
	mgr->icode_free_start = DSP_CODE_SIZE;

	memset(&mgr->rpatch, 0, sizeof(struct dsp_rpatch));
	strcpy(mgr->rpatch.name, "Routing");
	mgr->rpatch.code = (__u32 *) malloc(DSP_CODE_SIZE * sizeof(__u32));

	for (i = 0; i < DSP_NUM_GPRS; i++){
		mgr->gpr[i].type = GPR_TYPE_NULL;
		mgr->gpr[i].usage=0;
	}

	init_io_gprs_table(mgr);

	mgr->init = 1;
}
