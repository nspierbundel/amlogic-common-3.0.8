/*
 * hdmi_rx_ctrl_hdcp.h brief HDMI RX controller HDCP modul
 * 
 * Copyright (C) 2012 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef HDMI_RX_CTRL_HDCP_H_
#define HDMI_RX_CTRL_HDCP_H_

#include "hdmi_rx_ctrl.h"

/** Receiver key selection size - 40 bits */
#define HDCP_BKSV_SIZE	(2 *  1)
/** Encrypted keys size - 40 bits x 40 keys */
#define HDCP_KEYS_SIZE	(2 * 40)

/**
 * @short HDMI RX controller HDCP configuration
 */
struct hdmi_rx_ctrl_hdcp {
	/** Repeater mode else receiver only */
	bool repeat;
	/** Key description seed */
	uint32_t seed;
	/**
	 * Receiver key selection
	 * @note 0: high order, 1: low order
	 */
	uint32_t bksv[HDCP_BKSV_SIZE];
	/**
	 * Encrypted keys
	 * @note 0: high order, 1: low order
	 */
	uint32_t keys[HDCP_KEYS_SIZE];
};

int hdmi_rx_ctrl_hdcp_config(struct hdmi_rx_ctrl *ctx, const struct hdmi_rx_ctrl_hdcp *hdcp);
int hdmi_rx_ctrl_hdcp_ksv_list(struct hdmi_rx_ctrl *ctx, const uint32_t *list, unsigned count, unsigned depth);
int hdmi_rx_ctrl_hdcp_debug(struct hdmi_rx_ctrl *ctx);

#endif /* HDMI_RX_CTRL_HDCP_H_ */
