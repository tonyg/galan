/*********************************************************************
 *     file.c - dsp patch manager library - patch file reading functions
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
#include <stdlib.h>
#include "../emu10k1-include/internal.h"

extern __u32 addr_change[0x400];


#define FILE_HEAD "emu10k1-dsp-file"
#define FILE_FORMAT_VERSION 1

/* gives an increasing id to patches with the same name */
static int set_patch_id(struct dsp_patch_manager *mgr, struct dsp_patch *patch)
{
	struct dsp_patch *patch1;
	struct list_head *entry;
	int table[255];

	patch->id = 0;


	list_for_each(entry, &mgr->ipatch_list) {
		patch1 = list_entry(entry, struct dsp_patch, list);

		if (!strcmp(patch1->name, patch->name)) {
			table[patch1->id] = 1;
			while (table[patch->id] == 1) {
				patch->id++;
				if (patch->id == 255)
					return -1;
			}
		}
		}

	
	list_for_each(entry, &mgr->opatch_list) {
		patch1 = list_entry(entry, struct dsp_patch, list);

		if (!strcmp(patch1->name, patch->name)) {
			table[patch1->id] = 1;
			while (table[patch->id] == 1) {
				patch->id++;
				if (patch->id == 255)
					return -1;
			}
		}
		}

	return 0;
}
static int read_patch_code(struct dsp_patch *patch, FILE * fp)
{
	patch->code = (__u32 *) malloc(patch->code_size * sizeof(__u32));

	if (patch->code == NULL) {
		fprintf(stderr, "read_patch_code(): no memory available\n");
		return -1;
	}

	fread(patch->code, sizeof(__u32), patch->code_size, fp);

	return 0;
}

/* FIXME */
/* Still missing the ALIGN and CLEAR bits */
static inline void read_tram_lines(__u32 tramb_start, __u32 * traml_addr,
				   __u8 * traml_size, __u16 base, __u32 type, FILE * fp)
{
	int i;
	__u8 lines;
	__u8 data_addr;
	__u32 addr_val;

	fread(&lines, sizeof(__u8), 1, fp);
	for (i = 0; i < lines; i++) {
		fread(&data_addr, sizeof(__u8), 1, fp);

		addr_change[TRAML_IDATA_BASE - GPR_BASE + data_addr] = base + *traml_size;

		addr_change[TRAML_IADDR_BASE - GPR_BASE + data_addr] =
		    base + (TRAML_IADDR_BASE - TRAML_IDATA_BASE) + *traml_size;

		fread(&addr_val, sizeof(__u32), 1, fp);
		
		traml_addr[*traml_size] = ((tramb_start + addr_val) & 0xfffff) | (type << 20);
		(*traml_size)++;
	}
}

/* FIXME restore tramb_?start on failure */
static int read_tram_block(struct dsp_patch_manager *mgr, struct dsp_patch *patch, __u32 * traml_iaddr,
			   __u32 * traml_eaddr, FILE * fp)
{
	int i;
	__u8 blocks;
	__u32 size;
	__u8 tmp;
	__u32 type;
	/* tram blocks and lines */
	fread(&blocks, sizeof(__u8), 1, fp);
	for (i = 0; i < blocks; i++) {
		fread(&size, sizeof(__u32), 1, fp);

		type  = 0x30000000 & size;
		size &= 0x001fffff;

		if (size == 0) {
			fprintf(stderr, "tram block of 0 size\n");
			return -1;
		}

		if ( (size <= TRAMB_ISIZE - mgr->tramb_ifree_start ) && (type!=TRAM_EXTERNAL)) {
			tmp = patch->traml_isize;

			/* write lines */

			read_tram_lines(patch->tramb_isize,
					traml_iaddr, &patch->traml_isize, TRAML_IDATA_BASE, TRAML_TYPE_WRITE, fp);

			/* read lines */
			read_tram_lines(patch->tramb_isize,
					traml_iaddr, &patch->traml_isize, TRAML_IDATA_BASE, TRAML_TYPE_READ, fp);

			/* a block with no tram lines */
			if (tmp == patch->traml_isize) {
				fprintf(stderr, "tram block with no tram lines\n");
				return -1;
			}

			/* no free tram space */
			if (mgr->traml_ifree_start + patch->traml_isize > TRAML_ISIZE) {
				fprintf(stderr, "no free internal tram lines\n");
				return -1;
			}

			patch->tramb_isize += size;
			mgr->tramb_ifree_start += size;

		} else if ((size <= TRAMB_ESIZE - mgr->tramb_efree_start)&&(type!=TRAM_INTERNAL)) {
			tmp = patch->traml_esize;

			/* write lines */

			read_tram_lines(patch->tramb_esize, traml_eaddr,
					&patch->traml_esize, TRAML_EDATA_BASE, TRAML_TYPE_WRITE, fp);

			/* read lines */

			read_tram_lines(patch->tramb_esize, traml_eaddr,
					&patch->traml_esize, TRAML_EDATA_BASE, TRAML_TYPE_READ, fp);

			/* a block with no tram lines */
			if (tmp == patch->traml_esize) {
				fprintf(stderr, "tram block with no tram lines\n");
				return -1;
			}

			/* no free tram space */
			if (mgr->traml_efree_start + patch->traml_esize > TRAML_ESIZE) {
				fprintf(stderr, "no free external tram lines\n");
				return -1;
			}
			patch->tramb_esize += size;
			mgr->tramb_efree_start += size;

		} else {
			fprintf(stderr, "no free tram blocks\n");
			return -1;
		}
	}

	mgr->traml_ifree_start += patch->traml_isize;
	mgr->traml_efree_start += patch->traml_esize;

	return 0;
}

/* FIXME */
/* only single input patches supported */
static int read_patch_header(struct dsp_patch_manager *mgr, struct dsp_patch *patch, FILE * fp, char *name, int num_in, int num_out)
{
	char tmp[sizeof(FILE_HEAD)];
	__u8 count, addr;
	__u32 traml_iaddr[TRAML_ISIZE];
	__u32 traml_eaddr[TRAML_ESIZE];
	int ret, i, j;
	__u8 input[DSP_NUM_INPUTS],output[DSP_NUM_OUTPUTS];


	//Read file format info:
	fread(tmp, sizeof(char), sizeof(FILE_HEAD), fp);
	if(strcmp(tmp,FILE_HEAD)){
		fprintf(stderr,"Error: File is not a proper dsp file\n" );
		return -1;
	}
	//Read format version	
	i=0;
	fread(&i, sizeof(__u16),1 , fp);
	
	if( i < FILE_FORMAT_VERSION)
		fprintf(stderr,"Warning: file was created with a newer version of the emu-tools\n");
	   

	   
	/* patch name */
	fread(patch->name, sizeof(char), DSP_PATCH_NAME_SIZE, fp);

	//If user specifies a different patch name, we change it now:
	if(name[0]!='\0'){
		strncpy(patch->name,name, DSP_LINE_NAME_SIZE);
		patch->name[DSP_LINE_NAME_SIZE - 1] = '\0';
	}else
		//we copy back the name in the patch
		strncpy(name, patch->name, DSP_LINE_NAME_SIZE);
		
	/* input/output gprs */
	fread(&count, sizeof(__u8), 1, fp);

	// FIXME:
	// We'll need to change the file format to support non-symetric I/O
	if(num_in!=count){
		fprintf(stderr,"Patch requires %d inputs, %d were specified\n",count,num_in);
		goto err0;
	}

	
	for (i = 0; i < count; i++) {
		fread(&(input[i]), sizeof(__u8), 1, fp);
		fread(&(output[i]), sizeof(__u8), 1, fp);
	}

	j=0;
	for(i=0;i<DSP_NUM_INPUTS;i++){
		if(patch_uses_input(patch,i)){
			addr_change[input[j]] = get_io_gpr(mgr, i , GPR_INPUT, patch->gpr_used, patch->gpr_input);
			j++;
		}
	}

	j=0;
	for(i=0;i<DSP_NUM_OUTPUTS;i++){
		if(patch_uses_output(patch,i)){
			addr_change[output[j]] = get_io_gpr(mgr, i , GPR_OUTPUT, patch->gpr_used, patch->gpr_input);
			j++;
		}
	}

	/* dynamic gprs */
	fread(&count, sizeof(__u8), 1, fp);
	for (i = 0; i < count; i++) {
		fread(&addr, sizeof(__u8), 1, fp);
		addr_change[addr] = get_dynamic_gpr(mgr, patch->gpr_used);
	}

	/* static gprs */
	fread(&count, sizeof(__u8), 1, fp);
	for (i = 0; i < count; i++) {
		__s32 value;
		fread(&addr, sizeof(__u8), 1, fp);
		fread(&value, sizeof(__s32), 1, fp);
		addr_change[addr] = get_static_gpr(mgr, value, patch->gpr_used);
	}

	/* control gprs */
	fread(&count, sizeof(__u8), 1, fp);
	for (i = 0; i < count; i++) {
		__s32 value, min, max;
		char name[DSP_GPR_NAME_SIZE];

		fread(&addr, sizeof(__u8), 1, fp);
		fread(&value, sizeof(__s32), 1, fp);
		fread(&min, sizeof(__s32), 1, fp);
		fread(&max, sizeof(__s32), 1, fp);

		if (min >= max) {
			fprintf(stderr, "invalid range for control gpr %x %x\n", min, max);
			return -1;
		}

		if (value > max)
			value = max;
		else if (value < min)
			value = min;

		fread(name, sizeof(char), DSP_GPR_NAME_SIZE, fp);

		addr_change[addr] = get_control_gpr(mgr, value, min, max, name, patch->gpr_used, patch->name);
	}

	/* constant gprs */
	fread(&count, sizeof(__u8), 1, fp);
	for (i = 0; i < count; i++) {
		__s32 value;

		fread(&addr, sizeof(__u8), 1, fp);
		fread(&value, sizeof(__s32), 1, fp);
		addr_change[addr] = get_constant_gpr(mgr, value, patch->gpr_used);
	}
	/* here lines and blocks, internal and external, all start from 0 */
	/* this is fixed later on in set_tram_addr() */

	patch->tramb_istart = 0;
	patch->tramb_isize = 0;
	patch->tramb_estart = 0;
	patch->tramb_esize = 0;

	patch->traml_istart = 0;
	patch->traml_isize = 0;

	patch->traml_estart = 0;
	patch->traml_esize = 0;

	/* tram tablelookup blocks and lines */
	ret = read_tram_block(mgr, patch, traml_iaddr, traml_eaddr, fp);
	if (ret < 0)
		return ret;

	/* tram delaylines blocks and lines */
	ret = read_tram_block(mgr, patch, traml_iaddr, traml_eaddr, fp);
	if (ret < 0)
		return ret;

	if (patch->traml_isize != 0) {
		patch->traml_iaddr = (__u32 *) malloc(patch->traml_isize * sizeof(__u32));
		if (patch->traml_iaddr == NULL) {
			fprintf(stderr, "read_patch_header(): no memory available\n");
			return -1;
		}
		memcpy(patch->traml_iaddr, traml_iaddr, patch->traml_isize * sizeof(__u32));
	}

	if (patch->traml_esize != 0) {
		patch->traml_eaddr = (__u32 *) malloc(patch->traml_esize * sizeof(__u32));
		if (patch->traml_eaddr == NULL) {
			fprintf(stderr, "read_patch_header(): no memory available\n");
			goto err0;
		}
		memcpy(patch->traml_eaddr, traml_eaddr, patch->traml_esize * sizeof(__u32));
	}

	fread(&patch->code_size, sizeof(__u16), 1, fp);

	if (patch->code_size == 0) {
		fprintf(stderr, "patch with zero code size\n");
		goto err1;
	}

	mgr->code_free_start += patch->code_size;

	if (mgr->code_free_start > DSP_CODE_SIZE) {
		fprintf(stderr, "no free dsp code memory\n");
		goto err2;
	}

	return 0;

      err2:
	mgr->code_free_start -= patch->code_size;

      err1:
	if (patch->traml_esize != 0)
		free(patch->traml_eaddr);

      err0:
	if (patch->traml_isize != 0)
		free(patch->traml_iaddr);

	return -1;
}

/* FIXME */
/* support precise patch positioning in a line (patches in general don't commute) */
	
int dsp_read_patch(struct dsp_patch_manager *mgr, const char *file, int input[DSP_NUM_INPUTS],
		   int output[DSP_NUM_OUTPUTS], int num_in, int num_out, int io, char *name, int placement)
{
	struct dsp_patch *patch;
	struct list_head *list;
	FILE *fp;
	int i, j;

	if(num_in!=num_out)
		fprintf(stderr,"Error: Patches with an unequal number of ins and outs are not yet supported\n");

	if (!mgr->init)
		dsp_init(mgr);

	patch = (struct dsp_patch *) malloc(sizeof(struct dsp_patch));
	if (patch == NULL) {
		perror("patch");
		goto err0;
	}
	memset(patch, 0, sizeof(struct dsp_patch));

	
	if ((fp = fopen(file, "r")) == NULL) {
		
		
		perror(file);
		goto err1;
	}

	if(io==1){
		list= &mgr->ipatch_list;
		/* only load a patch if there is a route connecting it */
		for(i=0;i<num_in;i++){
			set_bit(input[i], &patch->input);
			set_bit(output[i], &patch->output);
			
			for (j = 0; j < DSP_NUM_OUTPUTS; j++)
				if ( test_bit(input[i], &mgr->rpatch.route_v[j]) || test_bit(input[i], &mgr->rpatch.route[j])  ) 
					goto match1;
			goto err2;
match1:
		}
		goto match;
	}else{
		list=&mgr->opatch_list;
		/* only load a patch if there is a route connecting it */	
		for(i=0;i<num_out;i++){
			set_bit(input[i], &patch->input);
			set_bit(output[i], &patch->output);
			if ( !( mgr->rpatch.route_v[input[i]] || mgr->rpatch.route[input[i]]))
				goto err2;


			
		}
		goto match;
	}
	

	
      err2:
	fclose(fp);
      err1:
	free(patch);
      err0:
	return -1;

      match:

	if (read_patch_header(mgr, patch, fp, name, num_in, num_out) < 0)
		goto err2;

	read_patch_code(patch, fp);
	search_and_replace(patch->code, patch->code_size, addr_change, GPR_BASE, TRAML_EADDR_BASE + TRAML_ESIZE);

	set_patch_id(mgr, patch);
	determine_io(mgr, patch);

	if(placement==0)
		list_add(&patch->list, list);
	else
		list_add_tail(&patch->list, list);

		
	fclose(fp);
	return 0;
}
