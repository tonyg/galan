/* internal library declarations, use dsp.h in ../include for user programs */

#ifndef INTERNAL_H
#define INTERNAL_H



#define MIXER_DEV_NAME "/dev/mixer"
#define DSP_DEV_NAME "/dev/dsp"

#define TANKMEMADDRREG_ADDR_MASK 0x000fffff     /* 20 bit tank address field                    */
#define TANKMEMADDRREG_CLEAR    0x00800000      /* Clear tank memory                            */
#define TANKMEMADDRREG_ALIGN    0x00400000      /* Align read or write relative to tank access  */
#define TANKMEMADDRREG_WRITE    0x00200000      /* Write to tank memory                         */
#define TANKMEMADDRREG_READ     0x00100000      /* Read from tank memory                        */

#define OUTPUT_BASE 0x20
#define CONSTANTS_BASE 0x40
#define GPR_BASE 0x100
#define TRAML_IDATA_BASE 0x200
#define TRAML_EDATA_BASE 0x280
#define TRAML_IADDR_BASE 0x300
#define TRAML_EADDR_BASE 0x380
#define DSP_CODE_BASE 0x400

#define CONSTANTS_SIZE 0x16
#define GPR_SIZE 0x100

#define TRAML_ISIZE 0x80
#define TRAML_ESIZE 0x20
#define TRAMB_ISIZE 0x2000
#define TRAMB_ESIZE 0x200000
#define DSP_CODE_SIZE 0x400

#define TRAM_INTERNAL 0x10000000
#define TRAM_EXTERNAL 0x20000000

#define patch_uses_input(patch, foo) test_bit(foo,&patch->input)

#define patch_uses_output(patch, foo) test_bit(foo,&patch->output)

//#define patch_is_in_line(patch, foo) ((patch->line&0x3f)==foo)

struct dsp_kernel_gpr {
	__u8 type;                      /* gpr type, STATIC, DYNAMIC, INPUT, OUTPUT, CONTROL */
	char name[DSP_GPR_NAME_SIZE];       /* gpr value, only valid for control gprs */
	__s32 min, max;         /* value range for this gpr, only valid for control gprs */
	__u8 line;                    /* which input/output line is the gpr attached, only valid for input/output gprs */
	__u8 usage;
};


struct dsp_kernel_rpatch {
	char name[DSP_PATCH_NAME_SIZE];
	__u16 code_start;
	__u16 code_size;

	__u32 gpr_used[DSP_NUM_GPRS / 32];
	__u32 gpr_input[DSP_NUM_GPRS / 32];
	__u32 route[DSP_NUM_OUTPUTS];
	__u32 route_v[DSP_NUM_OUTPUTS];
};

struct kdsp_patch {
	char name[DSP_PATCH_NAME_SIZE];
	__u8 id;
	__u32 inputs;		/* bitmap of lines used as inputs */
	__u32 outputs;          /* bitmap of lines used as output */
	__u16 code_start;
	__u16 code_size;

	__u32 gpr_used[DSP_NUM_GPRS / 32];	/* bitmap of used gprs */
	__u32 gpr_input[DSP_NUM_GPRS / 32];
	__u8 traml_istart;	/* starting address of the internal tram lines used */
	__u8 traml_isize;	/* number of internal tram lines used */

	__u8 traml_estart;
	__u8 traml_esize;

	__u16 tramb_istart;	/* starting address of the internal tram memory used */
	__u16 tramb_isize;	/* amount of internal memory used */
	__u32 tramb_estart;
	__u32 tramb_esize;
};


#define TRAML_TYPE_READ 0x1
#define TRAML_TYPE_WRITE 0x2

#define CMD_READ 1
#define CMD_WRITE 2

struct mixer_private_ioctl {
	__u32 cmd;
	__u32 val[90];		/* in kernel space only a fraction of this is used */
};

#define CMD_WRITEPTR		_IOW('D', 2, struct mixer_private_ioctl)
#define CMD_READPTR		_IOR('D', 3, struct mixer_private_ioctl)
#define CMD_GETPATCH		_IOR('D', 8, struct mixer_private_ioctl)
#define CMD_GETGPR		_IOR('D', 9, struct mixer_private_ioctl)
#define CMD_GETCTLGPR		_IOR('D', 10, struct mixer_private_ioctl)
#define CMD_SETPATCH		_IOW('D', 11, struct mixer_private_ioctl)
#define CMD_SETGPR		_IOW('D', 12, struct mixer_private_ioctl)
#define CMD_SETCTLGPR		_IOW('D', 13, struct mixer_private_ioctl)
#define CMD_GETGPR2OSS		_IOR('D', 15, struct mixer_private_ioctl)
#define CMD_SETGPR2OSS		_IOW('D', 16, struct mixer_private_ioctl)
#define CMD_PRIVATE3_VERSION	_IOW('D', 19, struct mixer_private_ioctl)

#define PRIVATE3_VERSION 1

#define PATCH_INPUT	0x1
#define PATCH_OUTPUT	0x2
#define GPR_INPUT	0x3
#define GPR_OUTPUT	0x4



int find_control_gpr_addr(struct dsp_patch_manager *mgr, const char *patch_name, const char *gpr_name);
int get_io_gpr(struct dsp_patch_manager *mgr, int line, int type, __u32 * used, __u32 * input);
int get_dynamic_gpr(struct dsp_patch_manager *mgr, __u32 * used);
int get_static_gpr(struct dsp_patch_manager *mgr, __s32 val, __u32 * used);
int get_control_gpr(struct dsp_patch_manager *mgr, __s32 val, __s32 min, __s32 max,
			   const char *name, __u32 * used, const char *patch_name);
int get_constant_gpr(struct dsp_patch_manager *mgr, __s32 val, __u32 * used);
int get_gpr(struct dsp_patch_manager *mgr, int addr, __u32 * used);
int free_gpr(struct dsp_patch_manager *mgr, int addr, __u32 * used);

void search_and_replace(__u32 * code, __u32 size, __u32 * table, __u32 min, __u32 max);
void init_addr_change_table(__u32 * table, __u32 min, __u32 max);
int construct_routing(struct dsp_patch_manager *mgr);
void determine_io(struct dsp_patch_manager *mgr, struct dsp_patch *patch);
void init_io_gprs_table(struct dsp_patch_manager *mgr);

#endif
