/*
 * HDMI RX HDCP driver which is responsible for HDMI RX PHY
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

#include "hdmi_rx_phy.h"

/** File name */
static const char *file = "hdmi_rx_phy";
/*
 * Miscellaneous
 */
/** Configuration clock minimum [kHz] */
#define CFG_CLK_MIN				(10000UL)
/** Configuration clock maximum [kHz] */
#define CFG_CLK_MAX				(160000UL)
/** TMDS clock minimum [kHz] */
#define TMDS_CLK_MIN			(25000UL)
/** TMDS clock maximum [kHz] */
#define TMDS_CLK_MAX			(340000UL)

#include "hdmi_rx_reg.h"

/* ***************************************************************************
 * Interface and configuration
 * ***************************************************************************/
/**
 * Open device validating context information
 * @param[in,out] ctx context information
 * @return error code
 */
int hdmi_rx_phy_open(struct hdmi_rx_phy *ctx)
{
	int error = 0;
	uint32_t evaltime = 0;
	if (ctx == 0 || ctx->bsp_reset == 0
		|| ctx->log_error == 0	|| ctx->log_info == 0
		|| ctx->bsp_read == 0 || ctx->bsp_write == 0
		|| ctx->cfg_clk < CFG_CLK_MIN || ctx->cfg_clk > CFG_CLK_MAX)
	{
		return -EINVAL;
	}
	if (ctx->status != 0)
	{	/* is not closed? */
		return -EPERM;
	}
#ifndef PHY_GEN3
	evaltime = (ctx->cfg_clk * 4095) / 158000;
#ifdef TESTCHIP
#ifdef HDMIRX_PHY_TSMC_65LP_2_5V /* E203 */
	ctx->log_info("HDMI RX PHY E203");
	ctx->peaking = PEAKING_CFG_E203;
#else
	ctx->log_info("HDMI RX PHY E201");
	ctx->peaking = PEAKING_CFG_E201;
#endif
	ctx->pll_cfg = 0;
	ctx->status = -1;
	ctx->bsp_reset(true);
#else
	ctx->bsp_reset(true);
	ctx->bsp_reset(false);
	/* keep HPD to zero, and PHY state to zero */
	ctx->bsp_write(REG_HDMI_SETUP_CTRL, 0xC0000C00);
	/* write PLL defaults */
	ctx->bsp_write(REG_HDMI_PLL_FRQSET1, HDMI_PLL_FRQSET1_DEFAULT);
	ctx->bsp_write(REG_HDMI_PLL_FRQSET2, HDMI_PLL_FRQSET2_DEFAULT);
	ctx->bsp_write(REG_HDMI_PLL_PAR1, HDMI_PLL_PAR1_DEFAULT);
	ctx->bsp_write(REG_HDMI_PLL_PAR2, HDMI_PLL_PAR2_DEFAULT);
	ctx->bsp_write(REG_HDMI_PLL_PAR3, HDMI_PLL_PAR3_DEFAULT);
	/* leave registers 0x004, 0x010, 0x018 and 0x04C to default values */
	/* write number of sys_clks in 4us  */
	ctx->bsp_write(REG_HDMI_TIMER_CTRL, ((4 * ctx->cfg_clk)/ 1000));
	/* write hysteresis and active edge of PLL clock to clock out data */
	ctx->bsp_write(REG_HDMI_CLK_CTRL, ((INV_ALIGNCLOCK_DEFAULT << 6) | HDMI_CLK_CTRL_DEFAULT));
	/* keep blue input to channel A to 1 (special case) and no inversion of clocks */
	ctx->bsp_write(REG_HDMI_PCB_CTRL, ctx->bsp_read(REG_HDMI_PCB_CTRL) & 0xFFFF0008);
	/* write eq_rsv accoding to PHY databook value */
	ctx->bsp_write(REG_HDMI_EQGAIN_CTRL, ((EQ_RSV_DEFAULT << 24) | ctx->bsp_read(REG_HDMI_EQGAIN_CTRL)));
	/* put HPD to one, and allow PHY FSM to to proceed to ALL ON state */
	ctx->bsp_write(REG_HDMI_SETUP_CTRL, HDMI_SETUP_CTRL_DEFAULT);
	/* for cfg_clk at 158MHz, evaltime is 4095. It has a linear relationship */
	/* 1 << 20 means hysteresis for PLL lock detection is clk4 */
	ctx->bsp_write(REG_HDMI_CKM_EVLTM, (1 << 20) | (evaltime << 4));
#endif
#else
	ctx->status = -1;
	ctx->bsp_reset(true);
	ctx->bsp_reset(false);
	ctx->bsp_pddq(1);
	ctx->bsp_svsmode(1);
	ctx->bsp_zcal_reset(1);
	/* Check zcal done assertion */
	if (ctx->bsp_zcal_done() != 0)
	{	/* power up */
		ctx->bsp_pddq(0);
		/* write timebase override and override enable */
		ctx->bsp_write(REG_HDMI_PHY_CMU_CONFIG, (LOCK_THRES << 10) | (1 << 9) | (((1 << 9) - 1) & ((ctx->cfg_clk * 4) / 1000)));

	}
	else
	{
		error = -EHOSTDOWN;
	}
#endif
	return error;
}

/**
 * Configure device setting internal timers and PLL
 * @param[in,out] ctx context information
 * @param[in] port input port 0 (A) or 1 (B)
 * @param[in] tclk TMDS clock frequency [kHz]
 * @param[in] dcm deep color mode (bits per pixel)
 * @return error code
 */
int hdmi_rx_phy_config(struct hdmi_rx_phy *ctx, unsigned port, unsigned long tclk, unsigned dcm)
{
	int error = 0;
#ifndef PHY_GEN3
	uint32_t pll = 0;
	uint32_t evaltime = 0;
#endif
	if (ctx == 0 || tclk < TMDS_CLK_MIN || tclk > TMDS_CLK_MAX)
	{
		return -EINVAL;
	}
	if (ctx->status == 0) 
	{	/* is closed? */
		return -EPERM;
	}
	ctx->status = -1;
#ifndef PHY_GEN3
	/* for cfg_clk at 158MHz, evaltime is 4095. It has a linear relationship */
	evaltime = (ctx->cfg_clk * 4095) / 158000;
	pll = tclk * evaltime / ctx->cfg_clk;
	/* configuration clock does not change */
	/* reconfigure only if PLL ranges have changed */
	if (ctx->pll_cfg != pll) 
	{
		ctx->pll_cfg = pll;
#ifdef TESTCHIP
		ctx->bsp_reset(true);
		/* write number of sys_clks in 4us  */
		/* refer to test chip documentation */
		ctx->bsp_write(RA_TIMERS, (((4 * ctx->cfg_clk) / 1000) << 1));
		ctx->bsp_write(RA_LATCH_EN, 0);					/* disarm */
		ctx->bsp_write(RA_PHY_PLL, pll);
		ctx->bsp_write(RA_INPUT_PORT, port);
		ctx->bsp_write(RA_CLK_GOOD, CLK_GOOD_CFG);		/* assert */
		ctx->bsp_write(RA_PEAKING, ctx->peaking);
		ctx->bsp_write(RA_LATCH_EN, 1);					/* re-arm */
		ctx->bsp_reset(false);
#else
		ctx->peaking = 0;
		ctx->pll_cfg = 0;
		ctx->status = -1;
#endif
	}
#else
	ctx->bsp_pddq(1);
	switch (dcm)
	{
		case 24:
			dcm = 0;
			break;
		case 30:
			dcm = 1;
			break;
		case 36:
			dcm = 2;
			break;
		case 48:
			dcm = 3;
			break;
		default:
			dcm = 0;
			break;
	}
	/* force port select */
	ctx->bsp_write(REG_HDMI_PHY_SYSTEM_CONFIG, ctx->bsp_read(REG_HDMI_PHY_SYSTEM_CONFIG) | (dcm << 5)| ( 1 << 2) | port);
	error = -ctx->bsp_config(tclk);
	ctx->bsp_pddq(0);
#endif
	ctx->status = 1;
	return error;
}

/**
 * Debug device
 * @param[in,out] ctx context information
 * @return error code
 */
int hdmi_rx_phy_debug(struct hdmi_rx_phy *ctx)
{
	int error = 0;

	if (ctx == 0) 
	{
		return -EINVAL;
	}
	if (ctx->status <= 0) 
	{	/* is not configured? */
		return -EPERM;
	}
#ifdef PHY_GEN3
	ctx->pll_cfg = ctx->bsp_read(REG_HDMI_PHY_MAINFSM_STATUS1);
#endif
	ctx->log_info("%s", file);
	ctx->log_info("  Config clock: %lukHz", ctx->cfg_clk);
	ctx->log_info("  PLL configuration: %04X", ctx->pll_cfg);
	return error;
}

/**
 * Fast switching control
 * @param[in,out] ctx context information
 * @param[in] enable or disable fast switching
 */
void hdmi_rx_phy_fast_switching(struct hdmi_rx_phy *ctx, int enable)
{
	ctx->bsp_write(REG_HDMI_PHY_SYSTEM_CONFIG, ctx->bsp_read(REG_HDMI_PHY_SYSTEM_CONFIG) | ((enable & 1) << 11));
}

/**
 * Close device
 * @param[in,out] ctx context information
 * @return error code
 */
int hdmi_rx_phy_close(struct hdmi_rx_phy *ctx)
{
	int error = 0;

	if (ctx == 0) 
	{
		return -EINVAL;
	}
	if (ctx->status == 0) 
	{		/* is closed? */
		return -EPERM;
	}
	ctx->status = 0;
	ctx->bsp_reset(true);
#ifdef PHY_GEN3
	ctx->bsp_pddq(1);
#endif
	return error;
}
