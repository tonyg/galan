/*********************************************************************
 *      dsp.h
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

#ifndef _DSP_H
#define _DSP_H

#include <string.h>
#include <stdint.h>

#include <linux/types.h>
#include <linux/bitops.h>

#include "list.h"

#define DSP_NUM_INPUTS 0x20
#define DSP_NUM_OUTPUTS 0x20
#define DSP_NUM_GPRS 0x100

#define DSP_PATCH_NAME_SIZE 32
#define DSP_GPR_NAME_SIZE	32 
#define DSP_LINE_NAME_SIZE	16

struct dsp_gpr {
	__u8 type;                      /* gpr type, STATIC, DYNAMIC, IO, CONTROL */
	char name[DSP_GPR_NAME_SIZE];       /* gpr value, only valid for control gprs */
	__s32 min_val, max_val;         /* value range for this gpr, only valid for control gprs */
	__u8 line;                    /* which input/output line is the gpr attached, only valid for IO gprs */
	__u8 usage;			/* number of patches using this gpr */
	__s32 value;
	__u8 addr;
	__u8 mixer_id;
	__u8 mixer_ch;
};


extern char dsp_in_name[DSP_NUM_INPUTS][DSP_LINE_NAME_SIZE];
extern char dsp_out_name[DSP_NUM_OUTPUTS][DSP_LINE_NAME_SIZE];

struct dsp_rpatch {
	char name[DSP_PATCH_NAME_SIZE];
	__u16 code_start;
	__u16 code_size;

	__u32 gpr_used[DSP_NUM_GPRS / 32];
	__u32 gpr_input[DSP_NUM_GPRS / 32];
	__u32 route[DSP_NUM_OUTPUTS];	/* bitmap of active routes whithout volume control */
	__u32 route_v[DSP_NUM_OUTPUTS];	/* bitmap of active routes with volume control */

	__u32 *code;
	int in[DSP_NUM_INPUTS];
	int out[DSP_NUM_OUTPUTS];
};

struct dsp_patch {
        char name[DSP_PATCH_NAME_SIZE];
        __u8 id;                /* to distinguish patches with the same name */
        __u32 input;             /* Line patch is attached to */
	__u32 output;
	__u16 code_start;
        __u16 code_size;

        __u32 gpr_used[DSP_NUM_GPRS / 32];      /* bitmap of used gprs */
        __u32 gpr_input[DSP_NUM_GPRS / 32];     /* bitmap of inputs gprs */
        __u8 traml_istart;      /* starting addr of the internal tram lines used */
        __u8 traml_isize;       /* number of internal tram lines used */

        __u8 traml_estart;
        __u8 traml_esize;

        __u16 tramb_istart;     /* starting addr of the internal tram memory used */
        __u16 tramb_isize;      /* amount of internal memory used */
        __u32 tramb_estart;
        __u32 tramb_esize;

		
        struct list_head list;  /* line list of patches */

        __u32 *code;
        __u32 *traml_iaddr;
        __u32 *traml_eaddr;

	__s16 in_gprs[DSP_NUM_INPUTS],out_gprs[DSP_NUM_OUTPUTS]; 
};

struct dsp_patch_manager {
	struct list_head ipatch_list;
	struct list_head opatch_list;
	struct dsp_rpatch rpatch;

	struct dsp_gpr gpr[DSP_NUM_GPRS];

	/* FIXME */
	/* if NUM_INPUTS < NUM_OUTPUTS this is wrong */
	int io[DSP_NUM_INPUTS][2];		/* list of io gprs, per line */

	__u8 traml_ifree_start;  //number of internal tram buffers used
	__u8 traml_efree_start;  //number of external tram buffers used
	__u16 tramb_ifree_start; //size of internal tram space used
	__u32 tramb_efree_start; //size of external tram space used
	__u16 code_free_start;   //number of instructions used ( *2)
	__u16 icode_free_start;  // ?
	int debug;
	int init;
	
	int dsp_fd;
	int mixer_fd;
};

enum {
	GPR_TYPE_NULL = 0,
	GPR_TYPE_IO,
	GPR_TYPE_STATIC,
	GPR_TYPE_DYNAMIC,
	GPR_TYPE_CONTROL,
	GPR_TYPE_CONSTANT
};


#define DBG			0x52
#define DBG_ZC                  0x80000000      /* zero tram counter */
#define DBG_SATURATION_OCCURED  0x02000000      /* saturation control */
#define DBG_SATURATION_ADDR     0x01ff0000      /* saturation address */
#define DBG_SINGLE_STEP         0x00008000      /* single step mode */
#define DBG_STEP                0x00004000      /* start single step */
#define DBG_CONDITION_CODE      0x00003e00      /* condition code */
#define DBG_SINGLE_STEP_ADDR    0x000001ff      /* single step address */


// a raw register writting function (offset > xFF or the DBG register)
int dsp_load_reg(int offset, __u32 value,  struct dsp_patch_manager *mgr);


#define dsp_stop_proc(mgr)	dsp_load_reg(DBG ,DBG_SINGLE_STEP,mgr);
#define dsp_start_proc(mgr)	dsp_load_reg(DBG ,0,mgr);

void dsp_set_debug(struct dsp_patch_manager *mgr);

void dsp_print_route_list(struct dsp_patch_manager *);
void dsp_print_patch_list(struct dsp_patch_manager *);
int dsp_print_control_gprs_patch( struct dsp_patch_manager *mgr, const char *patch_name);

	
void print_debug(struct dsp_patch_manager *mgr);

void dsp_print_inputs_name(void);
void dsp_print_outputs_name(void);
void dsp_print_oss_mixers(void);

void dsp_set_debug(struct dsp_patch_manager *);

int dsp_load(struct dsp_patch_manager *);

int dsp_check_input_name(struct dsp_patch_manager *, const char *);
int dsp_check_input(struct dsp_patch_manager *, int);

int dsp_check_output_name(struct dsp_patch_manager *, const char *);
int dsp_check_output(struct dsp_patch_manager *, int);

int dsp_check_route_name(struct dsp_patch_manager *, const char *);
int dsp_check_route(struct dsp_patch_manager *, int, int);
int dsp_check_route_volume(struct dsp_patch_manager *, int, int);

int dsp_add_route_name(struct dsp_patch_manager *, const char *);
int dsp_add_route(struct dsp_patch_manager *, int, int);

int dsp_add_route_v_name(struct dsp_patch_manager *, const char *);
int dsp_add_route_v(struct dsp_patch_manager *, int, int);

/* functions to delete dsp routes returns:
      0: route removed
      -1: bad name/arguments
      -2: unable to remove route
      -3: route already removed
*/
int dsp_del_route_name(struct dsp_patch_manager *, const char *);
int dsp_del_route(struct dsp_patch_manager *, int, int);

void dsp_print_control_gpr_list(struct dsp_patch_manager *);
int dsp_set_control_gpr_value(struct dsp_patch_manager *, const char *, const char *, __s32);
int dsp_get_control_gpr_value(struct dsp_patch_manager *, const char *, const char *, __s32 *);
int get_control_gpr_info(struct dsp_patch_manager *, const char *, const char *, struct dsp_gpr *);
int dsp_set_patch_name(struct dsp_patch_manager *, const char *, const char *);

int dsp_read_patch(struct dsp_patch_manager *mgr, const char *file, int input[DSP_NUM_INPUTS],
		   int output[DSP_NUM_OUTPUTS], int num_in, int num_out, int io, char *name, int placement);


int dsp_unload_patch(struct dsp_patch_manager *, const char *);


	

void dsp_print_oss_mixers(void);
int dsp_set_oss_control_gpr(struct dsp_patch_manager *, const char *, const char *, const char *);

void dsp_unload_all(struct dsp_patch_manager *);

int dsp_find_patch(struct dsp_patch_manager *mgr, const char *name, struct dsp_patch **patch);
int get_driver_version(int mixer_fd);
int dsp_init(struct dsp_patch_manager *);

int get_input_name(const char *input);
int get_output_name(const char *input);
int get_stereo_input_name(const char *input);
int get_stereo_output_name(const char *input);


#endif
