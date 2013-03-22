/*
 * HDMI RX control level driver which is responsible for HDMI RX Controller
 * is part of HDMI RX driver.
 * 
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include "hdmi_rx_ctrl.h"
#include "hdmi_rx_reg.h"

/**
 * Controller version. If defined version is 1.30a or above,
 * if not defined the version is 1.20a
 */
#define CTRL_13xa
 
/*
 * Miscellaneous
 */
/** Configuration clock minimum [kHz] */
#define CFG_CLK_MIN				(10000UL)
/** Configuration clock maximum [kHz] */
#define CFG_CLK_MAX				(160000UL)
/** Mode detection clock minimum [kHz] */
#define MD_CLK_MIN				(10000UL)
/** Mode detection clock maximum [kHz] */
#define MD_CLK_MAX				(50000UL)
/** TMDS clock minimum [kHz] */
#define TMDS_CLK_MIN			(25000UL)
/** TMDS clock maximum [kHz] */
#define TMDS_CLK_MAX			(340000UL)
/** TMDS clock delta [kHz] */
#define TMDS_CLK_DELTA			(125)
/** Pixel clock minimum [kHz] */
#define PIXEL_CLK_MIN			TMDS_CLK_MIN
/** Pixel clock maximum [kHz] */
#define PIXEL_CLK_MAX			TMDS_CLK_MAX
/** Horizontal resolution minimum */
#define HRESOLUTION_MIN			(320)
/** Horizontal resolution maximum */
#define HRESOLUTION_MAX			(4096)
/** Vertical resolution minimum */
#define VRESOLUTION_MIN			(240)
/** Vertical resolution maximum */
#define VRESOLUTION_MAX			(4455)
/** Refresh rate minimum [Hz] */
#define REFRESH_RATE_MIN		(100)
/** Refresh rate maximum [Hz] */
#define REFRESH_RATE_MAX		(25000)

/** File name */
static const char *file = "hdmi_rx_ctrl";

/* ***************************************************************************
 * Data Manipulation and Access
 * ***************************************************************************/
/**
 * Find first (least significant) bit set
 * @param[in] data word to search
 * @return bit position or 32 if none is set
 */
static unsigned first_bit_set(uint32_t data)
{
	unsigned n = 32;

	if (data != 0) {
		for (n = 0; (data & 1) == 0; n++) {
			data >>= 1;
		}
	}
	return n;
}

/**
 * Get bit field
 * @param[in] data raw data
 * @param[in] mask bit field mask
 * @return bit field value
 */
static uint32_t get(uint32_t data, uint32_t mask)
{
	return (data & mask) >> first_bit_set(mask);
}

/**
 * Set bit field
 * @param[in] data raw data
 * @param[in] mask bit field mask
 * @param[in] value new value
 * @return new raw data
 */
static uint32_t set(uint32_t data, uint32_t mask, uint32_t value)
{
	return ((value << first_bit_set(mask)) & mask) | (data & ~mask);
}

/**
 * Read bit field from device
 * @param[in,out] ctx context information
 * @param[in] addr bus address
 * @param[in] mask bitfield mask
 * @return zero if error or bit field value
 */
static uint32_t io_read(struct hdmi_rx_ctrl *ctx, uint16_t addr, uint32_t mask)
{
	return (ctx == 0) ? 0 : get(ctx->bsp_read(addr), mask);
}

/**
 * Write bit field into device
 * @param[in,out] ctx context information
 * @param[in] addr bus address
 * @param[in] mask bit field mask
 * @param[in] value new value
 * @return error code
 */
static int io_write(struct hdmi_rx_ctrl *ctx, uint16_t addr, uint32_t mask, uint32_t value)
{
	int error = 0;
	unsigned long flags = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	error |= ctx->spin_lock_irq_save(&ctx->spin, &flags);
	if (error == 0) {
		ctx->bsp_write(addr, set(ctx->bsp_read(addr), mask, value));
		error |= ctx->spin_unlock_irq_restore(&ctx->spin, flags);
	}
	return error;
}

/* ***************************************************************************
 * Auxiliary
 * ***************************************************************************/
/**
 * Configure audio after reset (default settings)
 * @param[in,out] ctx context information
 * @return error code
 */
static int audio_cfg(struct hdmi_rx_ctrl *ctx)
{
	int err = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	err |= io_write(ctx, RA_AUD_MUTE_CTRL, AUD_MUTE_SEL, AUD_MUTE_FORCE_UN);
	/* enable all outputs and select 32-bit for I2S */
	ctx->bsp_write(RA_AUD_SAO_CTRL, 1);
	return err;
}

/**
 * Reset audio FIFO
 * @param[in,out] ctx context information
 * @return error code
 */
static int audio_fifo_rst(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	error |= io_write(ctx, RA_AUD_FIFO_CTRL, AFIF_INIT, 1);
	error |= io_write(ctx, RA_AUD_FIFO_CTRL, AFIF_INIT, 0);
	ctx->debug_audio_fifo_rst++;
	return error;
}

/**
 * Configure legal clock range
 * @param[in,out] ctx context information
 * @param[in] min TMDS clock minimum
 * @param[in] max TMDS clock maximum
 * @return error code
 */
static int control_clk_range(struct hdmi_rx_ctrl *ctx, unsigned long min, unsigned long max)
{
	int error = 0;
	unsigned evaltime = 0;
	unsigned long ref_clk;

	if (ctx == 0 || min > max) {
		return -EINVAL;
	}
#ifdef CTRL_13xa  /* RX Controller Release 1.30a and above */
	ref_clk = ctx->md_clk;
#else	/* RX Controller Release 1.20a */
	ref_clk = ctx->cfg_clk;
#endif
	evaltime = (ref_clk * 4095) / 158000;
	min = (min * evaltime) / ref_clk;
	max = (max * evaltime) / ref_clk;
	error |= io_write(ctx, RA_HDMI_CKM_F, MINFREQ, min);
	error |= io_write(ctx, RA_HDMI_CKM_F, CKM_MAXFREQ, max);
	return error;
}

/**
 * Configure control after reset (default settings)
 * @param[in,out] ctx context information
 * @param[in] port input port 0 (A) or 1 (B)
 * @return error code
 */
static int control_cfg(struct hdmi_rx_ctrl *ctx, unsigned port)
{
	int err = 0;
	unsigned evaltime = 0;
	
	if (ctx == 0) {
		return -EINVAL;
	}

#ifdef CTRL_13xa /* RX Controller Release 1.30a and above */
	evaltime = (ctx->md_clk * 4095) / 158000;
#else	/* RX Controller Release 1.20a */
	evaltime = (ctx->cfg_clk * 4095) / 158000;
#endif
	ctx->bsp_write(RA_HDMI_OVR_CTRL, ~0);	/* enable all */
	err |= io_write(ctx, RA_HDMI_SYNC_CTRL, VS_POL_ADJ_MODE, VS_POL_ADJ_AUTO);
	err |= io_write(ctx, RA_HDMI_SYNC_CTRL, HS_POL_ADJ_MODE, HS_POL_ADJ_AUTO);
	err |= io_write(ctx, RA_HDMI_CKM_EVLTM, EVAL_TIME, evaltime);
	err |= control_clk_range(ctx, TMDS_CLK_MIN, TMDS_CLK_MAX);
	/* bit field shared between phy and controller */
	err |= io_write(ctx, RA_HDMI_PCB_CTRL, INPUT_SELECT, port);
	err |= io_write(ctx, RA_SNPS_PHYG3_CTRL, ((1 << 2) - 1) << 2, port);
	return err;
}

/**
 * Enable/disable all interrupts
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable interrupts
 * @return error code
 */
static int interrupts_cfg(struct hdmi_rx_ctrl *ctx, bool enable)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	if (enable) {
		/* set enable */
		ctx->bsp_write(RA_PDEC_IEN_SET, ~0);
		ctx->bsp_write(RA_AUD_CLK_IEN_SET, ~0);
		ctx->bsp_write(RA_AUD_FIFO_IEN_SET, ~0);
		ctx->bsp_write(RA_MD_IEN_SET, ~0);
		ctx->bsp_write(RA_HDMI_IEN_SET, ~0);
	} else {
		/* clear enable */
		ctx->bsp_write(RA_PDEC_IEN_CLR, ~0);
		ctx->bsp_write(RA_AUD_CLK_IEN_CLR, ~0);
		ctx->bsp_write(RA_AUD_FIFO_IEN_CLR, ~0);
		ctx->bsp_write(RA_MD_IEN_CLR, ~0);
		ctx->bsp_write(RA_HDMI_IEN_CLR, ~0);
		/* clear status */
		ctx->bsp_write(RA_PDEC_ICLR, ~0);
		ctx->bsp_write(RA_AUD_CLK_ICLR, ~0);
		ctx->bsp_write(RA_AUD_FIFO_ICLR, ~0);
		ctx->bsp_write(RA_MD_ICLR, ~0);
		ctx->bsp_write(RA_HDMI_ICLR, ~0);
	}
	return error;
}

/**
 * Enable/disable interrupts after HPD change
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable interrupts
 * @return error code
 */
static int interrupts_hpd(struct hdmi_rx_ctrl *ctx, bool enable)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	if (enable) {
		/* set enable */
		ctx->bsp_write(RA_PDEC_IEN_SET, DVIDET|AIF_CKS_CHG|AVI_CKS_CHG|PD_FIFO_NEW_ENTRY|PD_FIFO_OVERFL);
		ctx->bsp_write(RA_AUD_FIFO_IEN_SET, AFIF_OVERFL|AFIF_UNDERFL);
		ctx->bsp_write(RA_MD_IEN_SET, VIDEO_MODE);
		ctx->bsp_write(RA_HDMI_IEN_SET, AKSV_RCV|DCM_CURRENT_MODE_CHG);
	} else {
		/* clear enable */
		ctx->bsp_write(RA_PDEC_IEN_CLR, DVIDET|AIF_CKS_CHG|AVI_CKS_CHG|PD_FIFO_NEW_ENTRY|PD_FIFO_OVERFL);
		ctx->bsp_write(RA_AUD_FIFO_IEN_CLR, AFIF_OVERFL|AFIF_UNDERFL);
		ctx->bsp_write(RA_MD_IEN_CLR, VIDEO_MODE);
		ctx->bsp_write(RA_HDMI_IEN_CLR, AKSV_RCV|DCM_CURRENT_MODE_CHG);
		/* clear status */
		ctx->bsp_write(RA_PDEC_ICLR, DVIDET|AIF_CKS_CHG|AVI_CKS_CHG|PD_FIFO_NEW_ENTRY|PD_FIFO_OVERFL);
		ctx->bsp_write(RA_AUD_FIFO_ICLR, AFIF_OVERFL|AFIF_UNDERFL);
		ctx->bsp_write(RA_MD_ICLR, VIDEO_MODE);
		ctx->bsp_write(RA_HDMI_ICLR, AKSV_RCV|DCM_CURRENT_MODE_CHG);
	}
	return error;
}

/**
 * Configure packet decoder after reset (default settings)
 * @param[in,out] ctx context information
 * @return error code
 */
static int packet_cfg(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	ctx->bsp_write(RA_PDEC_CTRL, PFIFO_STORE_FILTER_EN|PD_FIFO_WE|PDEC_BCH_EN);
	ctx->bsp_write(RA_PDEC_ASP_CTRL, AUTO_VMUTE|AUTO_SPFLAT_MUTE);
	return error;
}

/**
 * Reset packet decoder FIFO
 * @param[in,out] ctx context information
 * @return error code
 */
static int packet_fifo_rst(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	error |= io_write(ctx, RA_PDEC_CTRL, PD_FIFO_FILL_INFO_CLR|PD_FIFO_CLR, ~0);
	error |= io_write(ctx, RA_PDEC_CTRL, PD_FIFO_FILL_INFO_CLR|PD_FIFO_CLR,  0);
	ctx->debug_packet_fifo_rst++;
	return error;
}

/**
 * Read packet AIF parameters
 * @param[in,out] ctx context information
 * @param[out] params audio parameters
 * @return error code
 */
static int packet_get_aif(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_audio *params)
{
	int error = 0;

	if (ctx == 0 || params == 0) {
		return -EINVAL;
	}
	params->coding_type = io_read(ctx, RA_PDEC_AIF_PB0, CODING_TYPE);
	params->channel_count = io_read(ctx, RA_PDEC_AIF_PB0, CHANNEL_COUNT);
	params->sample_frequency = io_read(ctx, RA_PDEC_AIF_PB0, SAMPLE_FREQ);
	params->sample_size = io_read(ctx, RA_PDEC_AIF_PB0, SAMPLE_SIZE);
	params->coding_extension = io_read(ctx, RA_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	params->channel_allocation = io_read(ctx, RA_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	params->down_mix_inhibit = io_read(ctx, RA_PDEC_AIF_PB1, DWNMIX_INHIBIT);
	params->level_shift_value = io_read(ctx, RA_PDEC_AIF_PB1, LEVEL_SHIFT_VAL);
	/** @note HW does not support AIF LFEPBL1-0, LFE play back level */
	return error;
}

/**
 * Read packet AVI parameters
 * @param[in,out] ctx context information
 * @param[out] params video parameters
 * @return error code
 */
static int packet_get_avi(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_video *params)
{
	int error = 0;

	if (ctx == 0 || params == 0) {
		return -EINVAL;
	}
	params->video_format = io_read(ctx, RA_PDEC_AVI_PB, VIDEO_FORMAT);
	params->active_valid = io_read(ctx, RA_PDEC_AVI_PB, ACT_INFO_PRESENT);
	params->bar_valid = io_read(ctx, RA_PDEC_AVI_PB, BAR_INFO_VALID);
	params->scan_info = io_read(ctx, RA_PDEC_AVI_PB, SCAN_INFO);
	params->colorimetry = io_read(ctx, RA_PDEC_AVI_PB, COLORIMETRY);
	params->picture_ratio = io_read(ctx, RA_PDEC_AVI_PB, PIC_ASPECT_RATIO);
	params->active_ratio = io_read(ctx, RA_PDEC_AVI_PB, ACT_ASPECT_RATIO);
	params->it_content = io_read(ctx, RA_PDEC_AVI_PB, IT_CONTENT);
	params->ext_colorimetry = io_read(ctx, RA_PDEC_AVI_PB, EXT_COLORIMETRY);
	params->rgb_quant_range = io_read(ctx, RA_PDEC_AVI_PB, RGB_QUANT_RANGE);
	params->n_uniform_scale = io_read(ctx, RA_PDEC_AVI_PB, NON_UNIF_SCALE);
	params->video_mode = io_read(ctx, RA_PDEC_AVI_PB, VID_IDENT_CODE);
	params->pixel_repetition = io_read(ctx, RA_PDEC_AVI_HB, PIX_REP_FACTOR);
	/** @note HW does not support AVI YQ1-0, YCC quantization range */
	/** @note HW does not support AVI CN1-0, IT content type */
	params->bar_end_top = io_read(ctx, RA_PDEC_AVI_TBB, LIN_END_TOP_BAR);
	params->bar_start_bottom = io_read(ctx, RA_PDEC_AVI_TBB, LIN_ST_BOT_BAR);
	params->bar_end_left = io_read(ctx, RA_PDEC_AVI_LRB, PIX_END_LEF_BAR);
	params->bar_start_right = io_read(ctx, RA_PDEC_AVI_LRB, PIX_ST_RIG_BAR);
	return error;
}

/* ***************************************************************************
 * Interface
 * ***************************************************************************/
/**
 * Open device validating context information
 * @param[in,out] ctx context information
 * @return error code
 *
 * @note Call sequence: open => [configure] => close
 */
int hdmi_rx_ctrl_open(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	unsigned i = 0;

	if (ctx == 0
		|| ctx->log_error == 0 || ctx->log_info == 0
		|| ctx->bsp_reset == 0 || ctx->bsp_reset_hdmi == 0
		|| ctx->bsp_read == 0 || ctx->bsp_write == 0
		|| ctx->spin_init == 0 || ctx->spin_destroy == 0
		|| ctx->spin_lock_irq_save == 0
		|| ctx->spin_unlock_irq_restore == 0
		|| ctx->cfg_clk < CFG_CLK_MIN || ctx->cfg_clk > CFG_CLK_MAX
		|| ctx->md_clk < MD_CLK_MIN || ctx->md_clk > MD_CLK_MAX) {
		return -EINVAL;
	}
	if (ctx->status != 0) {		/* is not closed? */
		return -EPERM;
	}
	error |= interrupts_cfg(ctx, false);
	for (i = 0; i < hdmi_rx_ctrl_event_cnt; i++) {
		ctx->event_handler[i] = 0;
	}
	ctx->debug_audio_fifo_rst = 0;
	ctx->debug_packet_fifo_rst = 0;
	ctx->debug_irq_handling = 0;
	ctx->debug_irq_packet_decoder = 0;
	ctx->debug_irq_audio_clock = 0;
	ctx->debug_irq_audio_fifo = 0;
	ctx->debug_irq_video_mode = 0;
	ctx->debug_irq_hdmi = 0;
	ctx->spin = 0;
	error |= ctx->spin_init(&ctx->spin);
	if (error == 0) {
		ctx->status = -1;
	}
	ctx->tmds_clk = 0;
	ctx->bsp_reset(true);
	ctx->bsp_reset(false);
	return error;
}

/**
 * Configure device
 * @param[in,out] ctx context information
 * @param[in] port input port 0 (A) or 1 (B)
 * @return error code
 *
 * @note Call sequence: open => [configure] => close
 */
int hdmi_rx_ctrl_config(struct hdmi_rx_ctrl *ctx, unsigned port)
{
	int error = 0;
	unsigned i = 0;

	if (ctx == 0 || port > 1) {
		return -EINVAL;
	}
	if (ctx->status == 0) {		/* is closed? */
		return -EPERM;
	}
	error |= interrupts_cfg(ctx, false);
	ctx->status = -1;
	ctx->tmds_clk = 0;
	if (ctx->bsp_edid_update != 0) {
		error |= ctx->bsp_edid_update();
	}
	error |= control_cfg(ctx, port);
	error |= audio_cfg(ctx);
	error |= packet_cfg(ctx);
	error |= audio_fifo_rst(ctx);
	error |= packet_fifo_rst(ctx);
	for (i = 0; i < hdmi_rx_ctrl_event_cnt; i++) {
		ctx->event_handler[i] = 0;
	}
	if (error == 0) {
		/* HPD workaround */
		ctx->bsp_write(RA_HDMI_IEN_SET, CLK_CHANGE);
		ctx->status = 1;
	}
	return error;
}

/**
 * Interrupt handler
 * @param[in,out] ctx context information
 * @return error code
 */
int hdmi_rx_ctrl_irq_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	unsigned i = 0;
	uint32_t intr = 0;
	uint32_t data = 0;
	unsigned long tclk = 0;
	unsigned long ref_clk;
	unsigned evaltime = 0;
	bool execute[hdmi_rx_ctrl_event_cnt];

#ifdef CTRL_13xa  /* RX Controller Release 1.30a and above */
	ref_clk = ctx->md_clk;
#else	/* RX Controller Release 1.20a */
	ref_clk = ctx->cfg_clk;
#endif
	if (ctx == 0) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	for (i = 0; i < hdmi_rx_ctrl_event_cnt; i++) {
		execute[i] = false;
	}
	intr = ctx->bsp_read(RA_HDMI_ISTS) & ctx->bsp_read(RA_HDMI_IEN);
	if (intr != 0) {
		ctx->bsp_write(RA_HDMI_ICLR, intr);
		if (get(intr, CLK_CHANGE) != 0) {
			execute[hdmi_rx_ctrl_event_clock_change] = true;
			evaltime = (ref_clk * 4095) / 158000;
			data = ctx->bsp_read(RA_HDMI_CKM_RESULT);
			tclk = ((ref_clk * get(data, CLKRATE)) / evaltime);
			if (tclk == 0) {
				error |= interrupts_hpd(ctx, false);
				error |= control_clk_range(ctx, TMDS_CLK_MIN, TMDS_CLK_MAX);
			}
			else {
				for (i = 0; i < TMDS_STABLE_TIMEOUT; i++) { /* time for TMDS to stabilise */
					;
				}
				tclk = ((ref_clk * get(data, CLKRATE)) / evaltime);
				error |= control_clk_range(ctx, tclk - TMDS_CLK_DELTA, tclk + TMDS_CLK_DELTA);
				error |= interrupts_hpd(ctx, true);
			}
			ctx->tmds_clk = tclk;
		}
		if (get(intr, DCM_CURRENT_MODE_CHG) != 0) {
			execute[hdmi_rx_ctrl_event_video_status] = true;
		}
		if (get(intr, AKSV_RCV) != 0) {
			execute[hdmi_rx_ctrl_event_aksv_reception] = true;
		}
		ctx->debug_irq_hdmi++;
	}
	intr = ctx->bsp_read(RA_MD_ISTS) & ctx->bsp_read(RA_MD_IEN);
	if (intr != 0) {
		ctx->bsp_write(RA_MD_ICLR, intr);
		if (get(intr, VIDEO_MODE) != 0) {
			execute[hdmi_rx_ctrl_event_video_status] = true;
		}
		ctx->debug_irq_video_mode++;
	}
	intr = ctx->bsp_read(RA_PDEC_ISTS) & ctx->bsp_read(RA_PDEC_IEN);
	if (intr != 0) {
		ctx->bsp_write(RA_PDEC_ICLR, intr);
		if (get(intr, DVIDET|AVI_CKS_CHG) != 0) {
			execute[hdmi_rx_ctrl_event_video_status] = true;
		}
		if (get(intr, AIF_CKS_CHG) != 0) {
			execute[hdmi_rx_ctrl_event_audio_status] = true;
		}
		if (get(intr, PD_FIFO_NEW_ENTRY) != 0) {
			execute[hdmi_rx_ctrl_event_packet_reception] = true;
		}
		if (get(intr, PD_FIFO_OVERFL) != 0) {
			error |= packet_fifo_rst(ctx);
		}
		ctx->debug_irq_packet_decoder++;
	}
	intr = ctx->bsp_read(RA_AUD_CLK_ISTS) & ctx->bsp_read(RA_AUD_CLK_IEN);
	if (intr != 0) {
		ctx->bsp_write(RA_AUD_CLK_ICLR, intr);
		ctx->debug_irq_audio_clock++;
	}
	intr = ctx->bsp_read(RA_AUD_FIFO_ISTS) & ctx->bsp_read(RA_AUD_FIFO_IEN);
	if (intr != 0) {
		ctx->bsp_write(RA_AUD_FIFO_ICLR, intr);
		if (get(intr, AFIF_OVERFL|AFIF_UNDERFL) != 0) {
			error |= audio_fifo_rst(ctx);
		}
		ctx->debug_irq_audio_fifo++;
	}
	ctx->debug_irq_handling++;
	for (i = 0; i < hdmi_rx_ctrl_event_cnt; i++) {
		if (execute[i] && ctx->event_handler[i] != 0) {
			ctx->event_handler[i](ctx);
		}
	}
	return error;
}

/**
 * Enable/disable hot plug detect output
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable output
 * @return error code
 */
int hdmi_rx_ctrl_hpd(struct hdmi_rx_ctrl *ctx, bool enable)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	error |= io_write(ctx, RA_HDMI_SETUP_CTRL, HOT_PLUG_DETECT, enable? 1 : 0);
	return error;
}

/**
 * Subscribe event notification. Override previous subscription.
 * @param[in,out] ctx context information
 * @param[in] type event type
 * @param[in] handler event handler, 0 to unsubscribe
 * @return error code
 */
int hdmi_rx_ctrl_subscribe(struct hdmi_rx_ctrl *ctx, enum hdmi_rx_ctrl_event_t type, int (*handler)(struct hdmi_rx_ctrl *))
{
	int error = 0;
	unsigned long flags = 0;

	if (ctx == 0 || type >= hdmi_rx_ctrl_event_cnt) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	error |= ctx->spin_lock_irq_save(&ctx->spin, &flags);
	/* Behavior
	 * 1) Enabling:  nothing to declare
	 * 2) Disabling: an event can occur till store be disabled but
	 *    nothing will be done because interrupt checks null handler
	 * 3) Replacing: handler may be executed sooner
	 */
	if (error == 0) {
		ctx->event_handler[type] = handler;
		error |= ctx->spin_unlock_irq_restore(&ctx->spin, flags);
	}
	return error;
}

/**
 * Configure packet store
 * @param[in,out] ctx context information
 * @param[in] type packet type
 * @param[in] enable enable/disable packet store
 * @return error code
 *
 * @note Subscribe packet reception event first
 */
int hdmi_rx_ctrl_store(struct hdmi_rx_ctrl *ctx, enum hdmi_rx_ctrl_packet_t type, bool enable)
{
	int error = 0;

	if (ctx == 0 || type >= hdmi_rx_ctrl_packet_cnt) {
		return -EINVAL;
	}
	if (ctx->status <= 0		/* is not configured? */
		&& ctx->event_handler[hdmi_rx_ctrl_event_packet_reception] == 0) {
		return -EPERM;
	}
	error |= io_write(ctx, RA_PDEC_CTRL, PFIFO_STORE_PACKET << type, enable? 1 : 0);
	return error;
}

/**
 * Get TDMS clock frequency [kHz]
 * @param[in,out] ctx context information
 * @return error code or value
 */
long hdmi_rx_ctrl_get_tmds_clk(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	return (error != 0)? error : ctx->tmds_clk;
}

/**
 * Read input audio parameters
 * @param[in,out] ctx context information
 * @param[out] params audio parameters
 * @return error code
 *
 * @note If EGAIN is returned,
 * @note clear interrupts before call it again
 * @note or let interrupt handler run in between
 */
int hdmi_rx_ctrl_get_audio(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_audio *params)
{
	int error = 0;

	if (ctx == 0 || params == 0) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	error |= packet_get_aif(ctx, params);
	if (error != 0) {
		goto exit;
	}
	if (io_read(ctx, RA_PDEC_ISTS, AIF_CKS_CHG) != 0) {
		error = -EAGAIN;
		goto exit;
	}
exit:
	return error;
}

/**
 * Get packet latest read from FIFO
 * @param[in,out] ctx context information
 * @param[out] data buffer for packet data
 * @param[in] size expected packet size
 * @return error code
 *
 * @note Info frames checksum should be verified
 */
int hdmi_rx_ctrl_get_packet(struct hdmi_rx_ctrl *ctx, uint8_t *data, size_t size)
{
	int error = 0;
	const unsigned max = 32;
	uint32_t tmp = 0;
	unsigned i = 0;

	if (ctx == 0 || size > max) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	if (io_read(ctx, RA_PDEC_STS, PD_FIFO_NEW_ENTRY) == 0) {
		error |= io_write(ctx, RA_PDEC_CTRL, PD_FIFO_SKIP, 1);	/* re-align */
		error |= io_write(ctx, RA_PDEC_CTRL, PD_FIFO_SKIP, 0);
	}
	if (io_read(ctx, RA_PDEC_STS, PD_FIFO_NEW_ENTRY) == 0) {
		error = -ENODATA;
		goto exit;
	}
	for (i = 0; i < max; i++) {
		if ((i % 4) == 0) {
			tmp = ctx->bsp_read(RA_PDEC_FIFO_DATA);
		}
		if (io_read(ctx, RA_PDEC_STS, PD_FIFO_UNDERFL) != 0) {
			error = -ENODATA;
			goto exit;
		}
		if (i < size) {
			data[i] = (uint8_t)tmp;
		}
		tmp >>= 8;
	}
exit:
	return error;
}

/**
 * Read input video parameters
 * @param[in,out] ctx context information
 * @param[out] params video parameters
 * @return error code
 *
 * @note If EGAIN is returned,
 * @note clear interrupts before call it again
 * @note or let interrupt handler run in between
 */
int hdmi_rx_ctrl_get_video(struct hdmi_rx_ctrl *ctx, struct hdmi_rx_ctrl_video *params)
{
	int error = 0;
	const unsigned factor = 100;
	unsigned divider = 0;
	uint32_t tmp = 0;

	if (ctx == 0 || params == 0) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	/* DVI mode */
	params->dvi = io_read(ctx, RA_PDEC_STS, DVIDET) != 0;
	/* AVI parameters */
	error |= packet_get_avi(ctx, params);
	if (error != 0) {
		goto exit;
	}
	/* pixel clock */
	params->pixel_clk = ctx->tmds_clk;
	/* refresh rate */
	tmp = io_read(ctx, RA_MD_VTC, VTOT_CLK);
	params->refresh_rate = (tmp == 0)? 0: (ctx->md_clk * 100000) / tmp;
	/* image parameters */
	params->interlaced = io_read(ctx, RA_MD_STS, ILACE) != 0;
	params->voffset = io_read(ctx, RA_MD_VOL, VOFS_LIN);
	params->vactive = io_read(ctx, RA_MD_VAL, VACT_LIN);
	params->vtotal = io_read(ctx, RA_MD_VTL, VTOT_LIN);
	if (params->interlaced)	{
		params->voffset <<= 1;
		params->vactive <<= 1;
		params->vtotal <<= 1;
	}
	params->hoffset = io_read(ctx, RA_MD_HT1, HOFS_PIX);
	params->hactive = io_read(ctx, RA_MD_HACT_PX, HACT_PIX);
	params->htotal = io_read(ctx, RA_MD_HT1, HTOT_PIX);
	/* deep color mode */
	tmp = io_read(ctx, RA_HDMI_STS, DCM_CURRENT_MODE);
	if (io_read(ctx, RA_PDEC_ISTS, DVIDET|AVI_CKS_CHG) != 0
		|| io_read(ctx, RA_MD_ISTS, VIDEO_MODE) != 0
		|| io_read(ctx, RA_HDMI_ISTS, DCM_CURRENT_MODE_CHG) != 0) {
		error = -EAGAIN;
		goto exit;
	}
	switch (tmp) {
	case DCM_CURRENT_MODE_48b:
		params->deep_color_mode = 48;
		divider = 2.00 * factor;	/* divide by 2 */
		break;
	case DCM_CURRENT_MODE_36b:
		params->deep_color_mode = 36;
		divider = 1.50 * factor;	/* divide by 1.5 */
		break;
	case DCM_CURRENT_MODE_30b:
		params->deep_color_mode = 30;
		divider = 1.25 * factor;	/* divide by 1.25 */
		break;
	default:
		params->deep_color_mode = 24;
		divider = 1.00 * factor;
		break;
	}
	params->pixel_clk = (params->pixel_clk * factor) / divider;
	params->hoffset = (params->hoffset * factor) / divider;
	params->hactive	= (params->hactive * factor) / divider;
	params->htotal = (params->htotal  * factor) / divider;
	if (params->pixel_clk < PIXEL_CLK_MIN || params->pixel_clk > PIXEL_CLK_MAX
		|| params->hactive < HRESOLUTION_MIN
		|| params->hactive > HRESOLUTION_MAX
		|| params->htotal < (params->hactive + params->hoffset)
		|| params->vactive < VRESOLUTION_MIN
		|| params->vactive > VRESOLUTION_MAX
		|| params->vtotal < (params->vactive + params->voffset)
		|| params->refresh_rate < REFRESH_RATE_MIN
		|| params->refresh_rate > REFRESH_RATE_MAX) {
		error = -ERANGE;
		goto exit;
	}
exit:
	return error;
}

/**
 * Debug device
 * @param[in,out] ctx context information
 * @return error code
 */
int hdmi_rx_ctrl_debug(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	struct hdmi_rx_ctrl_video v;
	struct hdmi_rx_ctrl_audio a;

	if (ctx == 0) {
		return -EINVAL;
	}
	if (ctx->status <= 0) {		/* is not configured? */
		return -EPERM;
	}
	ctx->log_info("%s", file);
	ctx->log_info("  Config clock: %3u.%03uMHz",
			ctx->cfg_clk / 1000, ctx->cfg_clk % 1000);
	ctx->log_info("  TMDS clock:   %3u.%03uMHz",
			ctx->tmds_clk / 1000, ctx->tmds_clk % 1000);
	error |= hdmi_rx_ctrl_get_video(ctx, &v);
	if (error == 0) {
		ctx->log_info("  Video: %ux%u%c@%u.%02uHz: %s, DCM %ub",
				v.hactive, v.vactive, v.interlaced? 'i' : 'p',
				v.refresh_rate / 100, v.refresh_rate % 100,
				v.dvi? "DVI" : "HDMI", v.deep_color_mode);
	}
	error |= hdmi_rx_ctrl_get_audio(ctx, &a);
	if (error == 0) {
		ctx->log_info("  Audio: CT=%u CC=%u SF=%u SS=%u CA=%u",
				a.coding_type, a.channel_count, a.sample_frequency,
				a.sample_size, a.channel_allocation);
	} else {
		ctx->log_info("  Audio");
	}
	ctx->log_info("  - N parameter:     %u", io_read(ctx, RA_PDEC_ACR_N, N_DECODED));
	ctx->log_info("  - CTS parameter:   %u", io_read(ctx, RA_PDEC_ACR_CTS, CTS_DECODED));
	ctx->log_info("  Audio  FIFO reset: %u", ctx->debug_audio_fifo_rst);
	ctx->log_info("  Packet FIFO reset: %u", ctx->debug_packet_fifo_rst);
	ctx->log_info("  IRQ handling:      %u", ctx->debug_irq_handling);
	ctx->log_info("  - packet decoder:  %u", ctx->debug_irq_packet_decoder);
	ctx->log_info("  - audio clock:     %u", ctx->debug_irq_audio_clock);
	ctx->log_info("  - audio FIFO:      %u", ctx->debug_irq_audio_fifo);
	ctx->log_info("  - video mode:      %u", ctx->debug_irq_video_mode);
	ctx->log_info("  - HDMI:            %u", ctx->debug_irq_hdmi);
	return error;
}

/**
 * Close device
 * @param[in,out] ctx context information
 * @return error code
 *
 * @note Call sequence: open => [configure] => close
 */
int hdmi_rx_ctrl_close(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0) {
		return -EINVAL;
	}
	if (ctx->status == 0) {		/* is closed? */
		return -EPERM;
	}
	error |= interrupts_cfg(ctx, false);
	error |= ctx->spin_destroy(&ctx->spin);
	ctx->status = 0;
	ctx->tmds_clk = 0;
	ctx->bsp_reset(true);
	return error;
}

/**
 * PHY  GEN 3 PDDQ
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable pddq signal
 * @return error code
 */
int hdmi_rx_ctrl_pddq(struct hdmi_rx_ctrl *ctx, int enable)
{
	int error = 0;
	if (ctx == 0)
	{
		error = -EINVAL;
	}
	else
	{
		error = io_write(ctx, RA_SNPS_PHYG3_CTRL, MSK(1,1), enable);
	}
	return error;
}

/**
 * PHY GEN 3 SVS MODE
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable physvsretmodez
 * @return error code
 */
int hdmi_rx_ctrl_svsmode(struct hdmi_rx_ctrl *ctx, int enable)
{
	int error = 0;
	if (ctx == 0)
	{
		error = -EINVAL;
	}
	else
	{
		error = io_write(ctx, RA_SNPS_PHYG3_CTRL, MSK(1,6), enable);
	}
	return error;
}

/**
 * PHY GEN 3 reset signal
 * @param[in,out] ctx context information
 * @param[in] enable enable/disable phy reset
 * @return error code
 */
int hdmi_rx_ctrl_phy_reset(struct hdmi_rx_ctrl *ctx, int enable)
{
	int error = 0;
	if (ctx == 0)
	{
		error = -EINVAL;
	}
	else
	{
		error = io_write(ctx, RA_SNPS_PHYG3_CTRL, MSK(1,0), enable);
	}
	return error;
}

/**
 * PHY GEN 3 read function through internal I2C Master controller
 * @param[in,out] ctx context information
 * @param [in] slave_address of peripheral/phy
 * @param [in] reg_address to read from
 * @return 16-bit read data or error
 */
uint16_t hdmi_rx_ctrl_phy_read(struct hdmi_rx_ctrl *ctx, uint8_t slave_address, uint8_t reg_address)
{
	int cnt = 0;
	if (ctx == 0)
	{
		return -EINVAL;
	}
	ctx->bsp_write(RA_I2CM_PHYG3_SLAVE, slave_address);
	ctx->bsp_write(RA_I2CM_PHYG3_ADDRESS, reg_address);
	ctx->bsp_write(RA_I2CM_PHYG3_OPERATION, 0x02); /* read op */
	do{ //wait i2cmpdone
		if(ctx->bsp_read(RA_HDMI_ISTS)&(1<<28)){
		ctx->bsp_write(RA_HDMI_ICLR, 1<<28);
		break;
	}
	cnt++;
	if(cnt>10000){
		printk("[HDMIRX error]: %s(%x,%x) timeout\n", __func__, slave_address, reg_address);
		break;
	}
	}while(1);

	return (uint16_t)(ctx->bsp_read(RA_I2CM_PHYG3_DATAI));
}

/**
 * PHY GEN 3 write function through internal I2C Master controller
 * @param[in,out] ctx context information
 * @param [in] slave_address of peripheral/phy
 * @param [in] reg_address to write to
 * @param [in] data 16-bit data to be written to peripheral
 * @return error
 */
int hdmi_rx_ctrl_phy_write(struct hdmi_rx_ctrl *ctx, uint8_t slave_address, uint8_t reg_address, uint16_t data)
{
	int error = 0;
	int cnt = 0;
	if (ctx == 0)
	{
		return -EINVAL;
	}
	ctx->bsp_write(RA_I2CM_PHYG3_SLAVE, slave_address);
	ctx->bsp_write(RA_I2CM_PHYG3_ADDRESS, reg_address);
	ctx->bsp_write(RA_I2CM_PHYG3_DATAO, data);
	ctx->bsp_write(RA_I2CM_PHYG3_OPERATION, 0x01); /* write op */

	do{ //wait i2cmpdone
		if(ctx->bsp_read(RA_HDMI_ISTS)&(1<<28)){
		ctx->bsp_write(RA_HDMI_ICLR, 1<<28);
		break;
	}
	cnt++;
	if(cnt>10000){
		printk("[HDMIRX error]: %s(%x,%x,%x) timeout\n", __func__, slave_address, reg_address, data);
		break;
	}
	}while(1);

	return error;
}
