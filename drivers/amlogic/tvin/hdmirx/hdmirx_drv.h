/*
 * hdmirx_drv.h for HDMI device driver, and declare IO function,
 * structure, enum, used in TVIN AFE sub-module processing
 *
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef _TVHDMI_H
#define _TVHDMI_H


#include <linux/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"

#define HDMIRX_VER "Ref.2012/10/19"
#define HDMI_STATE_CHECK_FREQ     20
#define ABS(x) ((x)<0 ? -(x) : (x))

/*
 * enum definitions
 */
typedef enum hdmirx_src_type_e {
	TVHDMI_SRC_TYPE_NULL = 0,
} hdmirx_src_type_t;

/* add new value at the end,
 * do not insert new value in the middle
 * to avoid wrong VIC value !!! 
 */
typedef enum HDMI_Video_Type_ {

	HDMI_Unkown = 0 ,
	HDMI_640x480p60 = 1 ,
	HDMI_480p60,
	HDMI_480p60_16x9,
	HDMI_720p60,
	HDMI_1080i60,               /* 5 */

	HDMI_480i60,
	HDMI_480i60_16x9,
	HDMI_1440x240p60,
	HDMI_1440x240p60_16x9,
	HDMI_2880x480i60,           /* 10 */

	HDMI_2880x480i60_16x9,
	HDMI_2880x240p60,
	HDMI_2880x240p60_16x9,
	HDMI_1440x480p60,
	HDMI_1440x480p60_16x9,      /* 15 */

	HDMI_1080p60,
	HDMI_576p50,
	HDMI_576p50_16x9,
	HDMI_720p50,
	HDMI_1080i50,               /* 20 */

	HDMI_576i50,
	HDMI_576i50_16x9,
	HDMI_1440x288p50,
	HDMI_1440x288p50_16x9,
	HDMI_2880x576i50,           /* 25 */

	HDMI_2880x576i50_16x9,
	HDMI_2880x288p50,
	HDMI_2880x288p50_16x9,
	HDMI_1440x576p50,
	HDMI_1440x576p50_16x9,      /* 30 */

	HDMI_1080p50,
	HDMI_1080p24,
	HDMI_1080p25,
	HDMI_1080p30,
	HDMI_2880x480p60,           /* 35 */

	HDMI_2880x480p60_16x9,
	HDMI_2880x576p50,
	HDMI_2880x576p50_16x9,
	HDMI_1080i50_1250,          /* 39 */        

	HDMI_1080I120 = 46,
	HDMI_720p120  = 47,

	HDMI_720p24   = 60,
	HDMI_720p30   = 62,
	HDMI_1080p120 = 63,
	HDMI_800_600  = 65,

	HDMI_1024_768,              /* 66 */
	HDMI_720_400,
	HDMI_1280_768,
	HDMI_1280_800,
	HDMI_1280_960,              /* 70 */

	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900,
	HDMI_1600_1200,             /* 75 */

	HDMI_1920_1200,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050,             /* 79 */

} HDMI_Video_Codes_t ;

/**
 * all functions declare
 */
extern enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
extern void hdmirx_hw_monitor(void);
extern bool hdmirx_hw_is_nosig(void);
extern bool hdmirx_hw_pll_lock(void);
extern void hdmirx_reset(void);
extern void hdmirx_hw_init(tvin_port_t port);
extern void hdmirx_hw_uninit(void);
extern unsigned int hdmirx_get_cur_vic(void);
extern void hdmirx_hw_enable(void);
extern void hdmirx_hw_disable(unsigned char flag);
extern void hdmirx_fill_edid_buf(const char* buf, int size);
extern int hdmirx_read_edid_buf(char* buf, int max_size);
extern void hdmirx_fill_key_buf(const char* buf, int size);
extern int hdmirx_read_key_buf(char* buf, int max_size);
extern int hdmirx_debug(const char* buf, int size);
extern int hdmirx_hw_get_color_fmt(void);
extern int hdmirx_hw_get_3d_structure(unsigned char*, unsigned char*);
extern int hdmirx_hw_get_dvi_info(void);
extern int hdmirx_hw_get_pixel_repeat(void);
extern bool hdmirx_hw_check_frame_skip(void);
extern int hdmirx_print(const char *fmt, ...);
extern int hdmirx_log_flag;
extern int hdmirx_hw_dump_reg(unsigned char* buf, int size);

#endif  // _TVHDMI_H
