/*********************************************************************
 *     debug.c - dsp patch manager library - debug functions
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

#include "../emu10k1-include/dsp.h"
#include "../emu10k1-include/list.h"
#include "../emu10k1-include/internal.h"
#include <sys/soundcard.h>

const char gpr_types[6][10]={
	"NULL",
	"IO",
	"STATIC",
	"DYNAMIC",
	"CONTROL",
	"CONSTANT"
};

extern char  dsp_in_name[DSP_NUM_INPUTS][DSP_LINE_NAME_SIZE];
extern char dsp_out_name[DSP_NUM_OUTPUTS][DSP_LINE_NAME_SIZE] ;

extern char *oss_mixer_name[SOUND_MIXER_NRDEVICES];
extern char dsp_in_name[DSP_NUM_INPUTS][DSP_LINE_NAME_SIZE];
extern char dsp_out_name[DSP_NUM_OUTPUTS][DSP_LINE_NAME_SIZE];
int get_control_gpr_info(struct dsp_patch_manager *mgr, const char *patch_name, const char *gpr_name,
			 struct dsp_gpr *gpr)
{
	int addr;

	addr = find_control_gpr_addr(mgr, patch_name, gpr_name);
	if (addr < 0)
		return -1;

	memcpy(gpr, &mgr->gpr[addr - GPR_BASE], sizeof(struct dsp_gpr));

	return 0;
}

static void print_patch_control_gprs(struct dsp_patch_manager *mgr, __u32 * used)
{
	int i;

	for (i = 0; i < DSP_NUM_GPRS; i++)
		if (mgr->gpr[i].type == GPR_TYPE_CONTROL && test_bit(i, used)) {
			printf("    name: %s\n", mgr->gpr[i].name);
			printf("    addr: %#04x\n", mgr->gpr[i].addr);
			if (mgr->gpr[i].mixer_id < SOUND_MIXER_NRDEVICES)
				printf("    mixer: %s_%c\n", oss_mixer_name[mgr->gpr[i].mixer_id],
				       mgr->gpr[i].mixer_ch ? 'r': 'l');

			printf("    value:  %#010x (%#010x - %#010x)\n\n", mgr->gpr[i].value,
			       mgr->gpr[i].min_val, mgr->gpr[i].max_val);
		}
}


int dsp_print_control_gprs_patch( struct dsp_patch_manager *mgr, const char *patch_name)
{
	struct dsp_patch *patch=NULL;

	if( dsp_find_patch(mgr, patch_name,&patch) < 0)
		return -1;
	printf("Control GPR's:\n");
	if (patch->id)
			printf("  Patch: %s %d\n", patch->name, patch->id);
		else
			printf("  Patch: %s\n", patch->name);
	
	print_patch_control_gprs(mgr,  patch->gpr_used);
	return 0;
}

void dsp_print_control_gpr_list(struct dsp_patch_manager *mgr)
{
	struct dsp_rpatch *rpatch;
	struct dsp_patch *patch;
	struct list_head *entry;
	
	if (!mgr->init)
		dsp_init(mgr);

	printf("Control GPR's:\n");

	
	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (patch->id)
			printf("  Patch: %s %d\n", patch->name, patch->id);
		else
			printf("  Patch: %s\n", patch->name);
		print_patch_control_gprs(mgr, patch->gpr_used);
	}

	rpatch = &mgr->rpatch;
	printf("  Patch: %s\n", rpatch->name);
	print_patch_control_gprs(mgr, rpatch->gpr_used);

	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		if (patch->id)
			printf("  Patch: %s %d\n", patch->name, patch->id);
		else
			printf("  Patch: %s\n", patch->name);

		print_patch_control_gprs(mgr, patch->gpr_used);
	}
}

void dsp_print_route_list(struct dsp_patch_manager *mgr)
{
	struct dsp_rpatch *rpatch;
	int i, j;

	if (!mgr->init)
		dsp_init(mgr);

	rpatch = &mgr->rpatch;

	printf("\n  Routes:\n");

	for (j = 0; j < DSP_NUM_INPUTS; j++)
		for (i = 0; i < DSP_NUM_OUTPUTS; i++)
			if (test_bit(j, &rpatch->route[i]))
				printf("    %s:%s\n", dsp_in_name[j], dsp_out_name[i]);
			else if (test_bit(j, &rpatch->route_v[i]))
				printf("    %s:%s  (with volume control)\n", dsp_in_name[j], dsp_out_name[i]);

}

void print_patch_code(__u32 * code, __u32 start, __u32 size)
{
	int i;

	printf("\n  code:\n");
	printf("    start: %#05x\n    size: %#05x\n", start, size);
	for (i = 0; i < size / 2; i++) {
		printf
		    ("    %#05x   %#010x  %#010x %2d %#05x %#05x %#05x %#05x\n",
		     DSP_CODE_BASE + start + 2 * i, code[i * 2 + 1],
		     code[i * 2], (code[i * 2 + 1] >> 20) & 0xf,
		     (code[i * 2 + 1] >> 10) & 0x3ff, code[i * 2 + 1] & 0x3ff,
		     (code[i * 2] >> 10) & 0x3ff, code[i * 2] & 0x3ff);
	}
}

void print_patch_gprs(struct dsp_patch_manager *mgr, __u32 * used, __u32 * input, int io)
{
	int i;

	printf("\n  gprs:");
	for (i = 0; i < DSP_NUM_GPRS; i++)
		if (test_bit(i, used)) {
			printf("\n");
			printf("    addr: %#05x", GPR_BASE + i);
			printf("   usage: %#04x", mgr->gpr[i].usage);
			printf("   type: %s", gpr_types[mgr->gpr[i].type]);

			if (mgr->gpr[i].type == GPR_TYPE_CONTROL) {
				printf("\n      name:  %-24s", mgr->gpr[i].name);
				if (mgr->gpr[i].mixer_id < SOUND_MIXER_NRDEVICES)
					printf("  mixer: %s_%c", oss_mixer_name[mgr->gpr[i].mixer_id], 
					       mgr->gpr[i].mixer_ch ? 'r': 'l');
				printf("\n      range: %#010x - %#010x", mgr->gpr[i].min_val, mgr->gpr[i].max_val);
				
			}

			if (mgr->gpr[i].type == GPR_TYPE_IO) {
				if (test_bit(i, input))
					printf(" (Input) ");
				else
					printf(" (Output)");
				if(io)
					printf("   line: %s", dsp_in_name[mgr->gpr[i].line] );
				else
					printf("   line: %s", dsp_out_name[mgr->gpr[i].line] );
				
			}

			if (mgr->gpr[i].type == GPR_TYPE_CONTROL ||
			    mgr->gpr[i].type == GPR_TYPE_STATIC || mgr->gpr[i].type == GPR_TYPE_CONSTANT)
				printf("   value: %#010x", mgr->gpr[i].value);

			
		}
}

void print_patch_tram(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int i;

	if (patch->tramb_isize || patch->tramb_esize || patch->traml_isize || patch->traml_esize)
		printf("\n  tram:\n");

	if (patch->tramb_isize || patch->tramb_esize)
		printf("    blocks:\n");

	if (patch->tramb_isize)
		printf("      internal: %#07x - %#07x\n", patch->tramb_istart,
		       patch->tramb_istart + patch->tramb_isize);

	if (patch->tramb_esize)
		printf("      external: %#07x - %#07x\n", patch->tramb_estart,
		       patch->tramb_estart + patch->tramb_esize);

	if (patch->traml_isize || patch->traml_esize)
		printf("    lines:\n");

	if (patch->traml_isize) {
		printf("      data internal: %#05x - %#05x\n",
		       TRAML_IDATA_BASE + patch->traml_istart,
		       TRAML_IDATA_BASE + patch->traml_istart + patch->traml_isize - 1);

		printf("      address internal: %#05x - %#05x\n",
		       TRAML_IADDR_BASE + patch->traml_istart,
		       TRAML_IADDR_BASE + patch->traml_istart + patch->traml_isize - 1);

		for (i = 0; i < patch->traml_isize; i++)
			printf("        %#05x %#06x type: %s Align bit: %s\n",
			       TRAML_IADDR_BASE + patch->traml_istart + i,
			       patch->traml_iaddr[i] & 0xfffff,
			       (patch->traml_iaddr[i]& TANKMEMADDRREG_READ)?"Read, ":"Write,",
			        (patch->traml_iaddr[i]& TANKMEMADDRREG_ALIGN)?"1":"0");
	}

	if (patch->traml_esize) {
		printf("      data external: %#05x - %#05x\n",
		       TRAML_EDATA_BASE + patch->traml_estart,
		       TRAML_EDATA_BASE + patch->traml_estart + patch->traml_esize - 1);

		printf("      address external: %#05x - %#05x\n",
		       TRAML_EADDR_BASE + patch->traml_estart,
		       TRAML_EADDR_BASE + patch->traml_estart + patch->traml_esize - 1);

		for (i = 0; i < patch->traml_esize; i++)
			printf("        %#05x %#07x type: %s Align bit:%s \n",
			       TRAML_EADDR_BASE + patch->traml_estart + i,
			       patch->traml_eaddr[i] & 0xfffff,
			       (patch->traml_eaddr[i]& TANKMEMADDRREG_READ)?"Read, ":"Write,",
				(patch->traml_eaddr[i]& TANKMEMADDRREG_ALIGN)?"1":"0");
	}
}

void print_in_patch(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int i;
	printf("\n  Name: %s", patch->name);
	if (patch->id)
		printf(" %d", patch->id);
	printf("\n");
	
	//printf("    input line :");
	printf("    lines :");
	for( i=0;i<DSP_NUM_INPUTS;i++)
		if(test_bit(i,&patch->input))
			printf(" \"%s\" ",dsp_in_name[i]);
	printf("\n");
	//for now, this is useless
	/*
	printf("    output line:");
	for(i=0;i<DSP_NUM_OUTPUTS;i++)
		if(test_bit(i,&patch->output))
			printf(" \"%s\" ",dsp_in_name[i]);
	printf("\n");
	*/
	
	print_patch_gprs(mgr, patch->gpr_used, patch->gpr_input,1);
	print_patch_code(patch->code, patch->code_start, patch->code_size);
	print_patch_tram(mgr, patch);
}

void print_out_patch(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	int i;
	printf("\n  Name: %s", patch->name);
	if (patch->id)
		printf(" %d", patch->id);
	printf("\n");

	printf("    input line :");
	for( i=0;i<DSP_NUM_INPUTS;i++)
		if(test_bit(i,&patch->input))
			printf(" \"%s\" ",dsp_out_name[i]);
	printf("\n");

	printf("    output line:");
	for(i=0;i<DSP_NUM_OUTPUTS;i++)
		if(test_bit(i,&patch->output))
			printf(" \"%s\" ",dsp_out_name[i]);
	printf("\n");
	
	print_patch_gprs(mgr, patch->gpr_used, patch->gpr_input,0);
	print_patch_code(patch->code, patch->code_start, patch->code_size);
	print_patch_tram(mgr, patch);
}
void print_summary(struct dsp_patch_manager *mgr)
{
	
	int i,j=DSP_NUM_GPRS,k=-1;
	
	for (i = 0; i < DSP_NUM_GPRS; i++) {
		if (mgr->gpr[i].type == GPR_TYPE_NULL)
			j--;
		else
			k=i;
	}
	printf("\
Usage Summary:
--------
Instructions : %d
GPRS:          %d (last used is %d)

Tram
 Internal
   buffers:    %d
   space  :    0x%06x
 External
   buffers:    %d
   space  :    0x%06x
",
	       mgr->code_free_start/2, j,k,
	       mgr->traml_ifree_start,  mgr->tramb_ifree_start,
	       mgr->traml_efree_start,  mgr->tramb_efree_start);
	
}
void print_debug(struct dsp_patch_manager *mgr)
{
	struct dsp_rpatch *rpatch;
	struct dsp_patch *patch;
	struct list_head *entry;

	print_summary(mgr);
	
	printf("\nInput patches:\n");
	
	list_for_each(entry, &mgr->ipatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		print_in_patch(mgr, patch);

	}

	rpatch = &mgr->rpatch;

	printf("\nRouting patch:\n");

	print_patch_gprs(mgr, rpatch->gpr_used, rpatch->gpr_input,2);
	dsp_print_route_list(mgr);
	print_patch_code(rpatch->code, rpatch->code_start, rpatch->code_size);

	printf("\nOutput patches:\n");
	
	list_for_each(entry, &mgr->opatch_list) {
		patch = list_entry(entry, struct dsp_patch, list);
		print_out_patch(mgr, patch);

	}
}

void dsp_print_patch_list(struct dsp_patch_manager *mgr)
{
	struct dsp_patch *patch;
	struct list_head *entry;
	int i,j;
	char tmp[10];

	if (!mgr->init)
		dsp_init(mgr);
	printf("Input patches                   Attached to:\n");
	printf("-------------                   -----------\n");
	if (!list_empty(&mgr->ipatch_list)) {
		list_for_each(entry, &mgr->ipatch_list) {
			patch = list_entry(entry, struct dsp_patch, list);
			tmp[0]='\0';
			if (patch->id)
				sprintf(tmp," %d", patch->id);
			printf("%s%s%n", patch->name,tmp,&j);
			j=DSP_PATCH_NAME_SIZE - j;
			printf("%*s",j, " " );
			for(i=0;i<DSP_NUM_OUTPUTS;i++)
				if(test_bit(i,&patch->output))
					printf("\"%s\" ",dsp_in_name[i]);
			printf("\n");
		}
	}
	

	printf("\nOutput patches                  Attached to:\n");
	printf("--------------                  -----------\n");
	if (!list_empty(&mgr->opatch_list)) {
		list_for_each(entry, &mgr->opatch_list) {
			patch = list_entry(entry, struct dsp_patch, list);
			tmp[0]='\0';
			if (patch->id)
				sprintf(tmp," %d", patch->id);
			printf("%s%s%n", patch->name,tmp,&j);
			j=DSP_PATCH_NAME_SIZE - j;
			printf("%*s",j, " " );

			for(i=0;i<DSP_NUM_OUTPUTS;i++)
				if(test_bit(i,&patch->output))
					printf("\"%s\" ",dsp_out_name[i]);
			printf("\n");
		}
	}
	
}
void dsp_set_debug(struct dsp_patch_manager *mgr)
{
	if (!mgr->init)
		dsp_init(mgr);

	mgr->debug = 1;
}

void dsp_print_oss_mixers(void)
{
	int i;

	printf("OSS Mixers:\n");

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		printf("  %s\n", oss_mixer_name[i]);
}
