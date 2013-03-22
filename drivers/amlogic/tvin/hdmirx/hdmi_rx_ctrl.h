/*
 * hdmi_rx_ctrl.h brief HDMI RX controller driver
 * 
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef HDMI_RX_CTRL_H_
#define HDMI_RX_CTRL_H_

#if 1
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

//#include <linux/amports/canvas.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif
/**
 * @short HDMI RX controller events
 */
enum hdmi_rx_ctrl_event_t
{
	/** TMDS clock change event */
	hdmi_rx_ctrl_event_clock_change = 0,
	/** Video status change event */
	hdmi_rx_ctrl_event_video_status,
	/** Audio status change event */
	hdmi_rx_ctrl_event_audio_status,
	/** Packet reception event */
	hdmi_rx_ctrl_event_packet_reception,
	/** AKSV reception event */
	hdmi_rx_ctrl_event_aksv_reception,
	/** Sentinel value */
	hdmi_rx_ctrl_event_cnt
};

/**
 * @short HDMI RX controller packet reception
 */
enum hdmi_rx_ctrl_packet_t
{
	/** Audio clock regeneration packet */
	hdmi_rx_ctrl_packet_ACR = 0,
	/** General control packet */
	hdmi_rx_ctrl_packet_GCP,
	/** Audio content protection packet */
	hdmi_rx_ctrl_packet_ACP,
	/** International standard recording code 1 packet */
	hdmi_rx_ctrl_packet_ISRC1,
	/** International standard recording code 2 packet */
	hdmi_rx_ctrl_packet_ISRC2,
	/** Gamut metadata packet */
	hdmi_rx_ctrl_packet_GAMUT,
	/** Vendor specific info frame packet */
	hdmi_rx_ctrl_packet_VSI,
	/** Auxiliary video info frame packet */
	hdmi_rx_ctrl_packet_AVI,
	/** Source product descriptor info frame packet */
	hdmi_rx_ctrl_packet_SPD,
	/** Audio info frame packet */
	hdmi_rx_ctrl_packet_AIF,
	/** MPEG source info frame packet */
	hdmi_rx_ctrl_packet_MPEGS,
	/** Sentinel value */
	hdmi_rx_ctrl_packet_cnt
};

/**
 * @short HDMI RX controller audio parameters
 *
 * For Audio InfoFrame (AIF) details see HDMI 1.4a section 8.2.2
 */
struct hdmi_rx_ctrl_audio
{
	/** AIF CT3-0, coding type */
	unsigned coding_type;
	/** AIF CC2-0, channel count */
	unsigned channel_count;
	/** AIF SF2-0, sample frequency */
	unsigned sample_frequency;
	/** AIF SS1-0, sample size */
	unsigned sample_size;
	/** AIF CTX, coding extension */
	unsigned coding_extension;
	/** AIF CA7-0, channel/speaker allocation */
	unsigned channel_allocation;
	/** AIF DM_INH, down-mix inhibit */
	unsigned down_mix_inhibit;
	/** AIF LSV3-0, level shift value */
	unsigned level_shift_value;
};

/**
 * @short HDMI RX controller video parameters
 *
 * For Auxiliary Video InfoFrame (AVI) details see HDMI 1.4a section 8.2.2
 */
struct hdmi_rx_ctrl_video
{
	/** DVI detection status: DVI (true) or HDMI (false) */
	bool dvi;
	/** Deep color mode: 24, 30, 36 or 48 [bits per pixel] */
	unsigned deep_color_mode;

	/** Pixel clock frequency [kHz] */
	unsigned long pixel_clk;
	/** Refresh rate [0.01Hz] */
	unsigned long refresh_rate;
	/** Interlaced */
	bool interlaced;
	/** Vertical offset */
	unsigned voffset;
	/** Vertical active */
	unsigned vactive;
	/** Vertical total */
	unsigned vtotal;
	/** Horizontal offset */
	unsigned hoffset;
	/** Horizontal active */
	unsigned hactive;
	/** Horizontal total */
	unsigned htotal;

	/** AVI Y1-0, video format */
	unsigned video_format;
	/** AVI A0, active format information present */
	unsigned active_valid;
	/** AVI B1-0, bar valid information */
	unsigned bar_valid;
	/** AVI S1-0, scan information */
	unsigned scan_info;
	/** AVI C1-0, colorimetry information */
	unsigned colorimetry;
	/** AVI M1-0, picture aspect ratio */
	unsigned picture_ratio;
	/** AVI R3-0, active format aspect ratio */
	unsigned active_ratio;
	/** AVI ITC, IT content */
	unsigned it_content;
	/** AVI EC2-0, extended colorimetry */
	unsigned ext_colorimetry;
	/** AVI Q1-0, RGB quantization range */
	unsigned rgb_quant_range;
	/** AVI SC1-0, non-uniform scaling information */
	unsigned n_uniform_scale;
	/** AVI VIC6-0, video mode identification code */
	unsigned video_mode;
	/** AVI PR3-0, pixel repetition factor */
	unsigned pixel_repetition;
	/** AVI, line number of end of top bar */
	unsigned bar_end_top;
	/** AVI, line number of start of bottom bar */
	unsigned bar_start_bottom;
	/** AVI, pixel number of end of left bar */
	unsigned bar_end_left;
	/** AVI, pixel number of start of right bar */
	unsigned bar_start_right;
};

/**
 * @short HDMI RX controller context information
 *
 * Initialize @b user fields (set status to zero).
 * After opening this data is for internal use only.
 */
struct hdmi_rx_ctrl
{
	/** (@b user) Context status: closed (0), opened (<0) and configured (>0) */
	int status;
	/** (@b user) Log errors */
	void (*log_error)(const char *file, int line, const char *fmt, ...);
	/** (@b user) Log information */
	void (*log_info)(const char *fmt, ...);
	/** (@b user) Board support package, reset device */
	void (*bsp_reset)(bool enable);
	/** (@b user) Board support package, reset HDMI clock domain */
	void (*bsp_reset_hdmi)(bool enable);
	/** (@b user) Board support package, EDID update - optional if not provided no action is performed */
	int (*bsp_edid_update)(void);
	/** (@b user) Board support package, register read access */
	uint32_t (*bsp_read)(uint16_t addr);
	/** (@b user) Board support package, register write access */
	void (*bsp_write)(uint16_t addr, uint32_t data);
	/** Mutual exclusion */
	void *spin;
	/** (@b user) Mutual exclusion initialization */
	int (*spin_init)(void *handler);
	/** (@b user) Mutual exclusion destruction */
	int (*spin_destroy)(void *handler);
	/** (@b user) Mutual exclusion lock */
	int (*spin_lock_irq_save)(void *handler, unsigned long *flags);
	/** (@b user) Mutual exclusion unlock */
	int (*spin_unlock_irq_restore)(void *handler, unsigned long flags);
	/** (@b user) Configuration clock frequency [kHz], valid range 10MHz to 160MHz */
	unsigned long cfg_clk;
	/** (@b user) Mode detection clock frequency [kHz], valid range 10MHz to 50MHz */
	unsigned long md_clk;
	/** TDMS clock frequency [kHz] */
	unsigned long tmds_clk;
	/** Event handler */
	int (*event_handler[hdmi_rx_ctrl_event_cnt])(struct hdmi_rx_ctrl *);
	/** Debug status, audio FIFO reset count */
	unsigned debug_audio_fifo_rst;
	/** Debug status, packet FIFO reset count */
	unsigned debug_packet_fifo_rst;
	/** Debug status, IRQ handling count */
	unsigned debug_irq_handling;
	/** Debug status, IRQ packet decoder count */
	unsigned debug_irq_packet_decoder;
	/** Debug status, IRQ audio clock count */
	unsigned debug_irq_audio_clock;
	/** Debug status, IRQ audio FIFO count */
	unsigned debug_irq_audio_fifo;
	/** Debug status, IRQ video mode count */
	unsigned debug_irq_video_mode;
	/** Debug status, IRQ HDMI count */
	unsigned debug_irq_hdmi;
};

int hdmi_rx_ctrl_open(struct hdmi_rx_ctrl *ctx);
int hdmi_rx_ctrl_config(struct hdmi_rx_ctrl *ctx, unsigned input);
int hdmi_rx_ctrl_irq_handler(struct hdmi_rx_ctrl *ctx);
int hdmi_rx_ctrl_hpd(struct hdmi_rx_ctrl *ctx, bool enable);
int hdmi_rx_ctrl_subscribe(struct hdmi_rx_ctrl *ctx, enum hdmi_rx_ctrl_event_t type, int (*handler)(struct hdmi_rx_ctrl *));
int hdmi_rx_ctrl_store(struct hdmi_rx_ctrl *ctx, enum hdmi_rx_ctrl_packet_t type, bool enable);
long hdmi_rx_ctrl_get_tmds_clk(struct hdmi_rx_ctrl *ctx);
int hdmi_rx_ctrl_get_audio(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_audio *params);
int hdmi_rx_ctrl_get_packet(struct hdmi_rx_ctrl *ctx, uint8_t *data, size_t size);
int hdmi_rx_ctrl_get_video(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_video *params);
int hdmi_rx_ctrl_debug(struct hdmi_rx_ctrl *ctx);
int hdmi_rx_ctrl_close(struct hdmi_rx_ctrl *ctx);
/* PHY GEN3 functions */
int hdmi_rx_ctrl_pddq(struct hdmi_rx_ctrl *ctx, int enable);
int hdmi_rx_ctrl_svsmode(struct hdmi_rx_ctrl *ctx, int enable);
int hdmi_rx_ctrl_phy_reset(struct hdmi_rx_ctrl *ctx, int enable);
uint16_t hdmi_rx_ctrl_phy_read(struct hdmi_rx_ctrl *ctx, uint8_t slave_address, uint8_t reg_address);
int hdmi_rx_ctrl_phy_write(struct hdmi_rx_ctrl *ctx, uint8_t slave_address, uint8_t reg_address, uint16_t data);

#endif /* HDMI_RX_CTRL_H_ */
