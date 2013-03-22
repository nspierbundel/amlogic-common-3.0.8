/*
 * Amlogic M6TV
 * HDMI RX
 * Copyright (C) 2010 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */


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
#include <mach/regs.h>
#include <mach/power_gate.h>

#include "hdmi_rx_phy.h"
#include "hdmi_rx_ctrl.h"
#include "hdmi_rx_ctrl_hdcp.h"

#include <linux/tvin/tvin.h>
/* Local include */
#include "hdmirx_drv.h"

int hdmirx_log_flag = 0x1;

static int edid_mode = 0;

static int force_vic = 0;

static int color_format_mode = 0; /* output color format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422; other same as input color format */

void hdmirx_wr_top (unsigned long addr, unsigned long data);

#define HDMIRX_IRQ                              24

#include "hdmi_rx_reg.h"

#define MAX_KEY_BUF_SIZE 512

static char key_buf[MAX_KEY_BUF_SIZE];
static int key_size = 0;

#define MAX_EDID_BUF_SIZE 1024
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size = 0;

static int init = 0;

/***********************
  Wrapper
************************/
/**
 * @short HDMI RX parameters
 */
static struct rx {
	/** HDMI RX received signal changed */
	bool change;
	/** HDMI RX input port 0 (A) or 1 (B) (or 2(C) or 3 (D)) */
	unsigned port;
	/** HDMI RX IRQ number */
	const unsigned irq;
	/** HDMI RX PHY context */
	struct hdmi_rx_phy phy;
	/** HDMI RX controller context */
	struct hdmi_rx_ctrl ctrl;
	/** HDMI RX controller HDCP configuration */
	const struct hdmi_rx_ctrl_hdcp hdcp;
	/* wrapper */
	unsigned int state;
	bool tx_5v_status;
	bool no_signal;

} rx = {false, 0, 1, {0}, {0},
	{
		/**
		 * @note These HDCP encrypted keys are test keys.
		 * @note Do NOT use them in production!
		 */
		false, 0x0000a55a,
		{
			0x00000051, 0x1ef21acd,
		},
		{
			0x00c0e0bd, 0x0ab26f9f,	0x000b90b3, 0xe9b2b75f,
			0x00bd00b5, 0xd15859ee,	0x00d89597, 0x7578e44c,
			0x004aff12, 0xfcc45ca2,	0x0036555b, 0xd5b12faf,
			0x008ae77f, 0x4edfd419,	0x007aa3d0, 0x0fd2c60f,
			0x0079052e, 0xbd613745,	0x00b67bb5, 0xe12ae0a6,
			0x0078b9dd, 0xf6629ac5,	0x0061deee, 0x2bfe2f2f,
			0x001a40b2, 0x1f63f998,	0x005a9ae6, 0xde543c62,
			0x0065df19, 0xa00e5744,	0x006c684f, 0x4b65a8bb,
			0x007da075, 0xb7f8d6cc,	0x001de01c, 0xeeadfbc8,
			0x0006e607, 0xc4dc61c4,	0x00a3bb1e, 0xd7510d5e,
			0x0002f495, 0xeceb5843,	0x0080e13e, 0x57081dcb,
			0x006fb563, 0x6f2e0eab,	0x0072439f, 0x4058074b,
			0x00b98261, 0xf21fbeef,	0x00c1eb77, 0x5aecdf3b,
			0x00f780a5, 0x5e975124,	0x00e1db09, 0x5e94f736,
			0x008ffa7b, 0x82786b25,	0x00e60823, 0x52b35574,
			0x00212a04, 0x82e7c09f,	0x0038af79, 0xc2a06f25,
			0x00fb17b5, 0x2a46aca3,	0x002c2de0, 0x1316dbc3,
			0x005e5761, 0x758cca16,	0x004d93a9, 0x09c6a332,
			0x00fa6bf7, 0x463357f5, 0x0060b17c, 0xa1a5d7fa,
			0x007bb35c, 0x605646d5, 0x0028aad1, 0x52893794,
		}
	}
};

/**
 * Log errors to standard error
 * @note limited to MAX_BUFFER characters
 * @param[in] file source file
 * @param[in] line line number
 * @param[in] fmt message format
 */
#define MAX_BUFFER		(128)
 
void log_error(const char *file, int line, const char *fmt, ...)
{
	va_list argp;
	char msg[MAX_BUFFER];

	if (file == 0 || fmt == 0) {
		return;
	}
	va_start(argp, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argp);
	va_end(argp);
	printk("[HDMIRX error] %s: %d: %s\n", file, line, msg);
    
}

void log_info(const char *fmt, ...)
{
	va_list argp;
	char msg[MAX_BUFFER];

	if (fmt == 0) {
		return;
	}
	va_start(argp, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argp);
	va_end(argp);
	printk("[HDMIRX] %s\n", msg);
    
}

/**
 * Initialize spinlock
 * @param[in] handler spinlock handler location
 * @return error code
 *
 * @note OS without spinlock native support
 * @note Still in a single processor platform spinlock = (un)mute interrupts
 */

static DEFINE_SPINLOCK(p_lock);
 
int os_spin_init(void *handler)
{
	if (handler == 0) {
		return -EINVAL;
	}
	return 0;
}

/**
 * Destroy spinlock
 * @param[in] handler spinlock handler location
 * @return error code
 *
 * @note OS without spinlock native support
 * @note Still in a single processor platform spinlock = (un)mute interrupts
 */
int os_spin_destroy(void *handler)
{
	if (handler == 0) {
		return -EINVAL;
	}
	return 0;
}

/**
 * Lock spinlock, disable interrupts and save IRQ flags
 * @param[in] handler spinlock handler location
 * @param[out] flags holder for IRQ flags
 * @return error code
 *
 * @note OS without spinlock native support
 * @note Still in a single processor platform spinlock = (un)mute interrupts
 */
int os_spin_lock_irq_save(void *handler, unsigned long *flags)
{
	spin_lock_irqsave(&p_lock, *flags);
	return 0;
}

/**
 * Unlock spinlock and restore IRQ flags
 * @param[in] handler spinlock handler location
 * @param[in] flags IRQ flags to restore
 * @return error code
 *
 * @note OS without spinlock native support
 * @note Still in a single processor platform spinlock = (un)mute interrupts
 */
int os_spin_unlock_irq_restore(void *handler, unsigned long flags)
{
	spin_unlock_irqrestore(&p_lock, flags);
	return 0;
}

/**
 * Sleep during a given period
 * @param[in] ms sleep period
 */
void os_sleep(unsigned ms)
{
	msleep(ms);
}

/**
 * Reset platform
 * @return error code
 */
int platform_reset(void)
{
	/* to do ... */
	int error = 0;
	return error;
}

void platform_zcal_reset(int enable)
{
    /* to do ... ??? */
}

/**
 * Reset HDMI RX controller
 * @param[in] enable reset enable
 */
void hdmi_rx_ctrl_reset(bool enable)
{
    /* to do .. */
}

/**
 * Reset HDMI RX controller - HDMI clock domain
 * @param[in] enable reset enable
 */
void hdmi_rx_ctrl_reset_hdmi(bool enable)
{
    /* null */
}


static unsigned char hdmirx_8bit_3d_edid_port1[] =
{
//8 bit only with 3D
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0x77 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x6b ,0x79 ,0x77 ,0x6f ,0x72 ,0x74 ,0x68 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0xdc,
0x02 ,0x03 ,0x38 ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x23 ,0x09 ,0x07 ,0x01 ,0x83 ,0x01 ,0x00 ,0x00,
0x74 ,0x03 ,0x0c ,0x00 ,0x10 ,0x00 ,0x88 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40 ,0x00 ,0x7f ,0x20,
0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72 ,0x38 ,0x2d ,0x40,
0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0xbc ,0x52 ,0xd0,
0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x80 ,0xd0,
0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x9e ,0x00 ,0x00,
0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x8e
};


static unsigned char hdmirx_12bit_3d_edid_port1 [] =
{
0x00 ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0x00 ,0x4d ,0xd9 ,0x02 ,0x2c ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x15 ,0x01 ,0x03 ,0x80 ,0x85 ,0x4b ,0x78 ,0x0a ,0x0d ,0xc9 ,0xa0 ,0x57 ,0x47 ,0x98 ,0x27,
0x12 ,0x48 ,0x4c ,0x21 ,0x08 ,0x00 ,0x81 ,0x80 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01,
0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x01 ,0x02 ,0x3a ,0x80 ,0x18 ,0x71 ,0x38 ,0x2d ,0x40 ,0x58 ,0x2c,
0x45 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00 ,0x72 ,0x51 ,0xd0 ,0x1e ,0x20,
0x6e ,0x28 ,0x55 ,0x00 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x00 ,0x00 ,0x00 ,0xfc ,0x00 ,0x53,
0x4f ,0x4e ,0x59 ,0x20 ,0x54 ,0x56 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x00 ,0x00 ,0x00 ,0xfd,
0x00 ,0x30 ,0x3e ,0x0e ,0x46 ,0x0f ,0x00 ,0x0a ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x20 ,0x01 ,0x1c,
0x02 ,0x03 ,0x3b ,0xf0 ,0x53 ,0x1f ,0x10 ,0x14 ,0x05 ,0x13 ,0x04 ,0x20 ,0x22 ,0x3c ,0x3e ,0x12,
0x16 ,0x03 ,0x07 ,0x11 ,0x15 ,0x02 ,0x06 ,0x01 ,0x26 ,0x09 ,0x07 ,0x07 ,0x15 ,0x07 ,0x50 ,0x83,
0x01 ,0x00 ,0x00 ,0x74 ,0x03 ,0x0c ,0x00 ,0x20 ,0x00 ,0xb8 ,0x2d ,0x2f ,0xd0 ,0x0a ,0x01 ,0x40,
0x00 ,0x7f ,0x20 ,0x30 ,0x70 ,0x80 ,0x90 ,0x76 ,0xe2 ,0x00 ,0xfb ,0x02 ,0x3a ,0x80 ,0xd0 ,0x72,
0x38 ,0x2d ,0x40 ,0x10 ,0x2c ,0x45 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01 ,0x1d ,0x00,
0xbc ,0x52 ,0xd0 ,0x1e ,0x20 ,0xb8 ,0x28 ,0x55 ,0x40 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00 ,0x1e ,0x01,
0x1d ,0x80 ,0xd0 ,0x72 ,0x1c ,0x16 ,0x20 ,0x10 ,0x2c ,0x25 ,0x80 ,0x30 ,0xeb ,0x52 ,0x00 ,0x00,
0x9e ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0xd9,
};

int hdmi_rx_ctrl_edid_update(void)
{
	int i, ram_addr, byte_num;
	unsigned int value;
	unsigned char check_sum;
	//printk("HDMI_OTHER_CTRL2=%x\n", hdmi_rd_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL2));

	if(edid_size>4 && edid_buf[0]=='E' && edid_buf[1]=='D' && edid_buf[2]=='I' && edid_buf[3]=='D'){ 
		printk("edid: use custom edid\n");
		check_sum = 0;
		for (i = 0; i < (edid_size-4); i++)
		{
			value = edid_buf[i+4];
			if(((i+1)&0x7f)!=0){
				check_sum += value;
				check_sum &= 0xff;
			}
			else{
				if(value != ((0x100-check_sum)&0xff)){
					printk("HDMIRX: origin edid[%d] checksum %x is incorrect, change to %x\n",
					i, value, (0x100-check_sum)&0xff);
				}
				value = (0x100-check_sum)&0xff;
				check_sum = 0;
		 	}
		    ram_addr = HDMIRX_TOP_EDID_OFFSET+i;
		    hdmirx_wr_top(ram_addr, value);
		}
	}
	else{
		if((edid_mode&0xf) == 0){
			unsigned char * p_edid_array;
			byte_num = sizeof(hdmirx_8bit_3d_edid_port1)/sizeof(unsigned char);
			p_edid_array =  hdmirx_8bit_3d_edid_port1;

			/*recalculate check sum*/
			for(check_sum = 0,i=0;i<127;i++){
				check_sum += p_edid_array[i];
				check_sum &= 0xff;
			}
			p_edid_array[127] = (0x100-check_sum)&0xff;

			for(check_sum = 0,i=128;i<255;i++){
				check_sum += p_edid_array[i];
				check_sum &= 0xff;
			}
			p_edid_array[255] = (0x100-check_sum)&0xff;
			/**/

			for (i = 0; i < byte_num; i++){
				value = p_edid_array[i];
				ram_addr = HDMIRX_TOP_EDID_OFFSET+i;
				hdmirx_wr_top(ram_addr, value);
			}
		}
		else if((edid_mode&0xf) == 1){
			byte_num = sizeof(hdmirx_12bit_3d_edid_port1)/sizeof(unsigned char);

			for (i = 0; i < byte_num; i++){
				value = hdmirx_12bit_3d_edid_port1[i];
				ram_addr = HDMIRX_TOP_EDID_OFFSET+i;
				hdmirx_wr_top(ram_addr, value);
			}
		}
	}
	return 0;
}

#define HDMIRX_ADDR_PORT    0xc800e000  // TOP ADDR_PORT: 0xc800e000; DWC ADDR_PORT: 0xc800e010
#define HDMIRX_DATA_PORT    0xc800e004  // TOP DATA_PORT: 0xc800e004; DWC DATA_PORT: 0xc800e014
#define HDMIRX_CTRL_PORT    0xc800e008  // TOP CTRL_PORT: 0xc800e008; DWC CTRL_PORT: 0xc800e018

void hdmirx_wr_top (unsigned long addr, unsigned long data)
{
	unsigned long dev_offset = 0;       // TOP ADDR_PORT: 0xc800e000; DWC ADDR_PORT: 0xc800e010
	*((volatile unsigned long *) (HDMIRX_ADDR_PORT+dev_offset)) = addr;
	*((volatile unsigned long *) (HDMIRX_DATA_PORT+dev_offset)) = data;
} /* hdmirx_wr_only_TOP */

unsigned long hdmirx_rd_top (unsigned long addr)
{
	unsigned long dev_offset = 0;       // TOP ADDR_PORT: 0xc800e000; DWC ADDR_PORT: 0xc800e010
	unsigned long data;
	*((volatile unsigned long *) (HDMIRX_ADDR_PORT+dev_offset)) = addr;
	data = *((volatile unsigned long *) (HDMIRX_DATA_PORT+dev_offset)); 
	return (data);
} /* hdmirx_rd_TOP */

/**
 * Read data from HDMI RX CTRL
 * @param[in] addr register address
 * @return data read value
 */
uint32_t hdmi_rx_ctrl_read(uint16_t addr)
{
	unsigned long dev_offset = 0x10;    // TOP ADDR_PORT: 0xc800e000; DWC ADDR_PORT: 0xc800e010
	unsigned long data;
	*((volatile unsigned long *) (HDMIRX_ADDR_PORT+dev_offset)) = addr;
	data = *((volatile unsigned long *) (HDMIRX_DATA_PORT+dev_offset)); 
	return (data);
} /* hdmirx_rd_DWC */

/**
 * Write data to HDMI RX CTRL
 * @param[in] addr register address
 * @param[in] data new register value
 */
void hdmi_rx_ctrl_write(uint16_t addr, uint32_t data)
{ 
	/* log_info("%04X:%08X", addr, data); */
	unsigned long dev_offset = 0x10;    // TOP ADDR_PORT: 0xc800e000; DWC ADDR_PORT: 0xc800e010
	*((volatile unsigned long *) (HDMIRX_ADDR_PORT+dev_offset)) = addr;
	*((volatile unsigned long *) (HDMIRX_DATA_PORT+dev_offset)) = data;
} /* hdmirx_wr_only_DWC */


/**
 * Set video interface to appropriate mode
 * @param[in] enable video enable
 * @param[in] hactive horizontal video active
 * @param[in] vactive vertical video active
 * @return error code
 */
int video_if_mode(bool enable, unsigned hactive, unsigned vactive, unsigned pr)
{
	return 0;
}

#define TMDS_TOLERANCE  (4000)

static unsigned long tmds_clock_old = 0;
static struct hdmi_rx_ctrl_video video_params = {0};
static int aksv_log = 1;

static struct rx_port_hdcp_t
{
	int is_init;
	int is_authenticated;
}
rx_port_hdcp[2] =
{
	{0,0},
	{0,0}
};

/** PHY GEN 3 I2C slave address (Testchip E205) */
#define PHY_GEN3_I2C_SLAVE_ADDR			(0x39UL)
#define PHY_GEN3_GLUE_I2C_SLAVE_ADDR	(0x48UL)
/**
 * PHY Wrapper
 * wrap functions needed by phy
 * used for phy gen 3 only
 */
uint16_t phy_wrapper_read(uint8_t reg_address)
{
	return hdmi_rx_ctrl_phy_read(&rx.ctrl, PHY_GEN3_I2C_SLAVE_ADDR, reg_address);
}

void phy_wrapper_write(uint8_t reg_address, uint16_t data)
{
	hdmi_rx_ctrl_phy_write(&rx.ctrl, PHY_GEN3_I2C_SLAVE_ADDR, reg_address, data);
}

static void phy_wrapper_svsretmode(int enable)
{
	hdmi_rx_ctrl_svsmode(&rx.ctrl, enable);
}

static void phy_wrapper_phy_reset(bool enable)
{
	hdmi_rx_ctrl_phy_reset(&rx.ctrl, enable? 1: 0);
}

static void phy_wrapper_pddq(int enable)
{
	hdmi_rx_ctrl_pddq(&rx.ctrl, enable);
}

static uint16_t phy_wrapper_zcal_done(void)
{
	/* Check if bit 14 of I2C Glue Logic register¡äs 3 is at 0.  (If 0 ok, else failed) */
	return (hdmi_rx_ctrl_phy_read(&rx.ctrl, PHY_GEN3_GLUE_I2C_SLAVE_ADDR, 0x3) >> 1 & 1)? 0: 1;
}

static int phy_wrapper_setup_bsp(unsigned long tmds)
{
	int error = 0;
  /* to do: phy config tmds clock */
  printk("[HDMIRX to do] %s\n", __func__);
	return error;
}

/**
 * Update port selection
 * @param[out] rx HDMI RX parameters
 * @param[in] delta delta {-1, 0, 1}
 * @return port selected
 */
static unsigned rx_update_port(struct rx *rx, int delta)
{
	rx->port += delta % 2;
	rx->port %= 2;
	return rx->port;
}

/* prototype */
static int rx_ctrl_config(struct hdmi_rx_ctrl *ctrl, unsigned input, const struct hdmi_rx_ctrl_hdcp *hdcp);

/**
 * Interrupt handler
 * @param[in] params handler parameters
 */

static irqreturn_t irq_handler(int irq, void *params)
{

	int error = 0;
	unsigned long hdmirx_top_intr_stat;
	if (params == 0)
	{
		log_error(__func__, __LINE__, "RX IRQ invalid parameter");
		return IRQ_HANDLED;
	}

	hdmirx_top_intr_stat = hdmirx_rd_top(HDMIRX_TOP_INTR_STAT);
	hdmirx_wr_top(HDMIRX_TOP_INTR_STAT_CLR, hdmirx_top_intr_stat); // clear interrupts in HDMIRX-TOP module 

	if (hdmirx_top_intr_stat & (0xf << 2)) {    // [5:2] hdmirx_5v_rise
		rx.tx_5v_status = true;
	} /* if (hdmirx_top_intr_stat & (0xf << 2)) // [5:2] hdmirx_5v_rise */

	if (hdmirx_top_intr_stat & (0xf << 6)) {    // [9:6] hdmirx_5v_fall
		rx.tx_5v_status = false;
	} /* if (hdmirx_top_intr_stat & (0xf << 6)) // [9:6] hdmirx_5v_fall */

	if(hdmirx_top_intr_stat & (1 << 31)){
		error = hdmi_rx_ctrl_irq_handler(&((struct rx *)params)->ctrl);
		if (error < 0)
		{
			if (error != -EPERM)
			{
				log_error(__func__, __LINE__, "RX IRQ handler %d", error);
			}
		}
    }
   return IRQ_HANDLED;

}

/**
 * Clock event handler
 * @param[in,out] ctx context information
 * @return error code
 */
static int clock_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	unsigned long tclk = 0;
	struct hdmi_rx_ctrl_video v;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	tclk = hdmi_rx_ctrl_get_tmds_clk(ctx);
	error |= hdmi_rx_ctrl_get_video(ctx, &v);
	if (((tmds_clock_old + TMDS_TOLERANCE) > tclk) &&
		((tmds_clock_old - TMDS_TOLERANCE) < tclk))
	{
		return 0;
	}
	if ((tclk == 0) && (tmds_clock_old != 0))
	{
		rx.change = true;
		/* TODO Review if we need to turn off the display
		video_if_mode(false, 0, 0, 0);
		*/
#if 0
		/* workaround for sticky HDMI mode */
		error |= rx_ctrl_config(ctx, rx.port, &rx.hdcp);
#endif
	}
	else
	{
#if 1
		error |= hdmi_rx_phy_config(&rx.phy, rx.port, tclk, v.deep_color_mode);
#endif
	}
	tmds_clock_old = ctx->tmds_clk;
//#if MESSAGES
	ctx->log_info("TMDS clock: %3u.%03uMHz",
			ctx->tmds_clk / 1000, ctx->tmds_clk % 1000);
//#endif
	return error;
}

/**
 * Video event handler
 * @param[in,out] ctx context information
 * @return error code
 */
static int video_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	int i = 0;
	struct hdmi_rx_ctrl_video v;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	/* wait for the video mode is stable */
	for (i = 0; i < 5000000; i++)
	{
		;
	}

	error |= hdmi_rx_ctrl_get_video(ctx, &v);
	if ((error == 0) &&
			(((video_params.hactive + 5) < (v.hactive)) ||
			((video_params.hactive - 5) > (v.hactive)) ||
			((video_params.vactive + 5) < (v.vactive)) ||
			((video_params.vactive - 5) > (v.vactive)) ||
			(video_params.pixel_repetition != v.pixel_repetition))
		)
	{
		rx.change = true;
		video_if_mode(true, v.hactive, v.vactive, v.pixel_repetition);
//#if MESSAGES
		ctx->log_info("Video: %ux%u%c@%u.%02uHz: %s, DCM %ub",
				v.hactive, v.vactive, v.interlaced? 'i' : 'p',
				v.refresh_rate / 100, v.refresh_rate % 100,
				v.dvi? "DVI" : "HDMI", v.deep_color_mode);
//#endif
	}
	video_params.deep_color_mode = v.deep_color_mode;
	video_params.htotal = v.htotal;
	video_params.vtotal = v.vtotal;
	video_params.pixel_clk = v.pixel_clk;
	video_params.refresh_rate = v.refresh_rate;
	video_params.hactive = v.hactive;
	video_params.vactive  = v.vactive;
	video_params.pixel_repetition = v.pixel_repetition;

	video_params.video_mode = v.video_mode;
	video_params.video_format = v.video_format;
	video_params.dvi = v.dvi;
	return error;
}

/**
 * Audio event handler
 * @param[in,out] ctx context information
 * @return error code
 */
static int audio_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	struct hdmi_rx_ctrl_audio a;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	error |= hdmi_rx_ctrl_get_audio(ctx, &a);
	if (error == 0)
	{
		/*ctx->log_info("Audio: CT=%u CC=%u SF=%u SS=%u CA=%u",
				a.coding_type, a.channel_count, a.sample_frequency,
				a.sample_size, a.channel_allocation);*/
	}
	return error;
}

/**
 * Packet store configuration
 * @param[in,out] ctx context information
 * @param[in] type packet type to store
 * @param[in] enable enable/disable store
 * @return error code
 */
static int packet_store(struct hdmi_rx_ctrl *ctx, uint8_t type, bool enable)
{
	int error = 0;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	switch (type)
	{
	case 0x01:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_ACR, enable);
		break;
	case 0x03:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_GCP, enable);
		break;
	case 0x04:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_ACP, enable);
		break;
	case 0x05:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_ISRC1, enable);
		break;
	case 0x06:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_ISRC2, enable);
		break;
	case 0x0A:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_GAMUT, enable);
		break;
	case 0x81:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_VSI, enable);
		break;
	case 0x82:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_AVI, enable);
		break;
	case 0x83:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_SPD, enable);
		break;
	case 0x84:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_AIF, enable);
		break;
	case 0x85:
		error |= hdmi_rx_ctrl_store(ctx, hdmi_rx_ctrl_packet_MPEGS, enable);
		break;
	default:
		if (enable)
		{
			ctx->log_info("Packet Types");
			ctx->log_info("  0x01 - ACR");
			ctx->log_info("  0x03 - GCP");
			ctx->log_info("  0x04 - ACP");
			ctx->log_info("  0x05 - ISR1");
			ctx->log_info("  0x06 - ISR2");
			ctx->log_info("  0x0A - GAMUT");
			ctx->log_info("  0x81 - VSI");
			ctx->log_info("  0x82 - AVI");
			ctx->log_info("  0x83 - SPD");
			ctx->log_info("  0x84 - AIF");
			ctx->log_info("  0x85 - MPEGS");
		}
		error = -EINVAL;
		break;
	}
	return error;
}

/**
 * Packet event handler
 * @param[in,out] ctx context information
 * @return error code
 */
static int packet_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	uint8_t data[32] = {0};
	unsigned i = 0;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	error |= hdmi_rx_ctrl_get_packet(ctx, data, sizeof(data));
	if (error == 0)
	{
		error |= packet_store(ctx, data[0], false);		/* disable */
		ctx->log_info("-> %02X", data[0]);
		for (i = 0; i < sizeof(data); i += 8)
		{
			ctx->log_info("   %02X %02X %02X %02X %02X %02X %02X %02X",
							data[i+0], data[i+1], data[i+2], data[i+3],
							data[i+4], data[i+5], data[i+6], data[i+7]);
		}
	}
	return error;
}

/**
 * AKSV event handler
 * @param[in,out] ctx context information
 * @return error code
 */
static int aksv_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (ctx == 0)
	{
		return -EINVAL;
	}
	rx_port_hdcp[(hdmi_rx_ctrl_read(0x38) >> 16) & 0x1].is_init = 1;
	rx_port_hdcp[(hdmi_rx_ctrl_read(0x38) >> 16) & 0x1].is_authenticated = 1;
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_video_status, video_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe video status");
	}
	if (aksv_log)
	{
		ctx->log_info("AKSV received");
	}
	return error;
}

/**
 * Configure HDMI RX controller
 * @param[in,out] ctx context information
 * @param[in] port input port 0 (A) or 1 (B)
 * @param[in] hdcp HDCP configuration
 * @return error code
 */
static int rx_ctrl_config(struct hdmi_rx_ctrl *ctx, unsigned port, const struct hdmi_rx_ctrl_hdcp *hdcp)
{
	int error = 0;

	if (ctx == 0 || hdcp == 0)
	{
		return -EINVAL;
	}
	if (ctx->status > 0)
	{		/* is not configured? */
		error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_clock_change, 0);
		if (error != 0)
		{
			ctx->log_error(__func__, __LINE__, "RX subscribe clock change");
			goto exit;
		}
		error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_video_status, 0);
		if (error != 0)
		{
			ctx->log_error(__func__, __LINE__, "RX subscribe video status");
			goto exit;
		}
		error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_audio_status, 0);
		if (error != 0)
		{
			ctx->log_error(__func__, __LINE__, "RX subscribe audio status");
			goto exit;
		}
		error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_packet_reception, 0);
		if (error != 0)
		{
			ctx->log_error(__func__, __LINE__, "RX subscribe packet reception");
			goto exit;
		}
		error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_aksv_reception, 0);
		if (error != 0)
		{
			ctx->log_error(__func__, __LINE__, "RX subscribe AKSV reception");
			goto exit;
		}
	}
	error |= hdmi_rx_ctrl_config(ctx, port);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX controller configure %d", error);
		goto exit;
	}
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_clock_change, clock_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe clock change");
		goto exit;
	}
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_video_status, video_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe video status");
		goto exit;
	}
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_audio_status, audio_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe audio status");
		goto exit;
	}
#if 0
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_packet_reception, packet_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe packet reception");
		goto exit;
	}
#endif
	error |= hdmi_rx_ctrl_subscribe(ctx, hdmi_rx_ctrl_event_aksv_reception, aksv_handler);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX subscribe AKSV reception");
		goto exit;
	}
	error |= hdmi_rx_ctrl_hpd(ctx, true);
	if (error != 0)
	{
		ctx->log_error(__func__, __LINE__, "RX hot plug detect output");
		goto exit;
	}
exit:
	return error;
}

static void restart_rx_phy(void)
{
	hdmi_rx_ctrl_write (RA_SNPS_PHYG3_CTRL,0x4f); /* PHY reset */
	platform_zcal_reset(1);

	hdmi_rx_ctrl_write (RA_I2CM_PHYG3_SLAVE,0x39);
	hdmi_rx_ctrl_write (RA_I2CM_PHYG3_ADDRESS,0x02);
	hdmi_rx_ctrl_write (RA_I2CM_PHYG3_DATAO,0x22fa);
	hdmi_rx_ctrl_write (RA_I2CM_PHYG3_OPERATION,0x01);
	hdmi_rx_ctrl_write (RA_SNPS_PHYG3_CTRL,0x4C); /* PHY power-up */
}

/***********************
  TVIN driver interface
************************/
#define HDMIRX_HWSTATE_5V_LOW             0
#define HDMIRX_HWSTATE_5V_HIGH            1
#define HDMIRX_HWSTATE_HPD_READY          2
#define HDMIRX_HWSTATE_SIG_UNSTABLE        3
#define HDMIRX_HWSTATE_SIG_STABLE          4
#define HDMIRX_HWSTATE_INIT                5

void hdmirx_hw_monitor(void)
{

	switch(rx.state){

		case HDMIRX_HWSTATE_5V_LOW:
			if(rx.tx_5v_status){
				rx.state = HDMIRX_HWSTATE_5V_HIGH;
				log_info("[HDMIRX State] 5v low->5v high\n");
			}
			else{
				rx.no_signal = true;
			}
		break;

		case HDMIRX_HWSTATE_5V_HIGH:
			if(!rx.tx_5v_status){
				rx.no_signal = true;
				rx.state = HDMIRX_HWSTATE_5V_LOW;
				log_info("[HDMIRX State] 5v high->5v low\n");
			}
			else{
				rx.state = HDMIRX_HWSTATE_HPD_READY;
				log_info("[HDMIRX State] 5v high->hpd ready\n");
			}
		break;

		case HDMIRX_HWSTATE_HPD_READY:
			if(!rx.tx_5v_status){
				rx.no_signal = true;
				rx.state = HDMIRX_HWSTATE_5V_LOW;
				log_info("[HDMIRX State] hpd ready ->5v low\n");
			}
			else{
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				log_info("[HDMIRX State] hpd ready->unstable\n");
			}
		break;

		case HDMIRX_HWSTATE_SIG_UNSTABLE:
			if(!rx.tx_5v_status==0){
				rx.no_signal = true;
				rx.state = HDMIRX_HWSTATE_5V_LOW;
				log_info("[HDMIRX State] unstable->5v low\n");
			}
			else{
				if (hdmi_rx_ctrl_read (RA_HDMI_PLL_LCK_STS) && 0x01){
					rx.change = 0;
					rx.state = HDMIRX_HWSTATE_SIG_STABLE;
					log_info("[HDMIRX State] unstable->stable\n");
				}
			}
		break;

		case HDMIRX_HWSTATE_SIG_STABLE:
			if(!rx.tx_5v_status){
				rx.no_signal = true;
				rx.state = HDMIRX_HWSTATE_5V_LOW;
				log_info("[HDMIRX State] stable->5v low\n");
			}
			else if ((hdmi_rx_ctrl_read (RA_HDMI_PLL_LCK_STS) && 0x01) == 0){
				restart_rx_phy();
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				log_info("[HDMIRX State] stable->unstable, pll unlock\n");
			}
			else if(rx.change){
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				log_info("[HDMIRX State] stable->unstable, mode change\n");
			}
			else{
			}
		break;

		default:
			if(!rx.tx_5v_status){
				rx.no_signal = true;
				rx.state = HDMIRX_HWSTATE_5V_LOW;
			}
		break;
	}	
}


/**
 * Program core_pin_mux to enable HDMI pins
 */
void hdmirx_hw_enable(void)
{
	WRITE_CBUS_REG(PERIPHS_PIN_MUX_0 , READ_CBUS_REG(PERIPHS_PIN_MUX_0 )|
				( (1 << 27)   |   // pm_gpioW_0_hdmirx_5V_A  
				(1 << 26)   |   // pm_gpioW_1_hdmirx_HPD_A 
				(1 << 25)   |   // pm_gpioW_2_hdmirx_scl_A 
				(1 << 24)   |   // pm_gpioW_3_hdmirx_sda_A 
				(1 << 23)   |   // pm_gpioW_4_hdmirx_5V_B  
				(1 << 22)   |   // pm_gpioW_5_hdmirx_HPD_B 
				(1 << 21)   |   // pm_gpioW_6_hdmirx_scl_B 
				(1 << 20)   |   // pm_gpioW_7_hdmirx_sda_B 
				(1 << 19)   |   // pm_gpioW_8_hdmirx_5V_C  
				(1 << 18)   |   // pm_gpioW_9_hdmirx_HPD_C 
				(1 << 17)   |   // pm_gpioW_10_hdmirx_scl_C
				(1 << 16)   |   // pm_gpioW_11_hdmirx_sda_C
				(1 << 15)   |   // pm_gpioW_12_hdmirx_5V_D 
				(1 << 14)   |   // pm_gpioW_13_hdmirx_HPD_D
				(1 << 13)   |   // pm_gpioW_14_hdmirx_scl_D
				(1 << 12)   |   // pm_gpioW_15_hdmirx_sda_D
				(1 << 11)));     // pm_gpioW_16_hdmirx_cec  

	WRITE_CBUS_REG(PERIPHS_PIN_MUX_1 , READ_CBUS_REG(PERIPHS_PIN_MUX_1 )|
				( (1 << 2)    |   // pm_gpioW_17_hdmirx_tmds_clk
				(1 << 1)    |   // pm_gpioW_18_hdmirx_pix_clk 
				(1 << 0)));      // pm_gpioW_19_hdmirx_audmeas 
    
}

void hdmirx_hw_disable(unsigned char flag)
{
}

int hdmirx_hw_dump_reg(unsigned char* buf, int size)
{
	return 0;
}

/*
* EDID & hdcp
*/
int hdmirx_read_key_buf(char* buf, int max_size)
{
	if(key_size > max_size){
		pr_err("Error: %s, key size %d is larger than the buf size of %d\n", __func__,  key_size, max_size);
		return 0;
	}
	memcpy(buf, key_buf, key_size);
	pr_info("HDMIRX: read key buf\n");
	return key_size;
}

void hdmirx_fill_key_buf(const char* buf, int size)
{
	if(size > MAX_KEY_BUF_SIZE){
		pr_err("Error: %s, key size %d is larger than the max size of %d\n", __func__,  size, MAX_KEY_BUF_SIZE);
		return;
	}
	memcpy(key_buf, buf, size);
	key_size = size;
	pr_info("HDMIRX: fill key buf, size %d\n", size);
}

int hdmirx_read_edid_buf(char* buf, int max_size)
{
	if(edid_size > max_size){
		pr_err("Error: %s, edid size %d is larger than the buf size of %d\n", __func__,  edid_size, max_size);
		return 0;
	}
	memcpy(buf, edid_buf, edid_size);
	pr_info("HDMIRX: read edid buf\n");
	return edid_size;
}

void hdmirx_fill_edid_buf(const char* buf, int size)
{
	if(size > MAX_EDID_BUF_SIZE){
		pr_err("Error: %s, edid size %d is larger than the max size of %d\n", __func__,  size, MAX_EDID_BUF_SIZE);
		return;
	}
	memcpy(edid_buf, buf, size);
	edid_size = size;
	pr_info("HDMIRX: fill edid buf, size %d\n", size);
}

int get_packet_type(char *pkttype)
{
	int type = 0;
	if (strncmp(pkttype, "ACR", 3) == 0){
		type = 0x01;
	}
	else if (strncmp(pkttype, "GCP", 3) == 0){
		type = 0x03;
	}
	else if (strncmp(pkttype, "ACP", 3) == 0){
		type = 0x04;
	}
	else if (strncmp(pkttype, "ISRC1", 5) == 0){
		type = 0x05;
	}
	else if (strncmp(pkttype, "ISRC1", 5) == 0){
		type = 0x06;
	}
	else if (strncmp(pkttype, "GAMUT", 5) == 0){
		type = 0x0A;
	}
	else if (strncmp(pkttype, "VSI", 3) == 0){
		type = 0x81;
	}
	else if (strncmp(pkttype, "AVI", 3) == 0){
		type = 0x82;
	}
	else if (strncmp(pkttype, "SPD", 3) == 0){
		type = 0x83;
	}
	else if (strncmp(pkttype, "AIF", 3) == 0){
		type = 0x84;
	}
	else if (strncmp(pkttype, "MPEGS", 5) == 0){
		type = 0x85;
	}

	return type;
}

int hdmirx_debug(const char* buf, int size)
{
	char tmpbuf[128];
	int i=0;
	int type = 0;
	
	while((buf[i])&&(buf[i]!=',')&&(buf[i]!=' ')){
	    tmpbuf[i]=buf[i];
	    i++;
	}
	tmpbuf[i]=0;
	if(strncmp(tmpbuf, "pkt", 3) == 0){
		type = get_packet_type(&tmpbuf[4]);
		packet_store(&rx.ctrl, type, true);
	}
	else if (strncmp(tmpbuf, "aksv", 4) == 0){
		aksv_log = !aksv_log;
	}
	else if (strncmp(tmpbuf, "dbg", 3) == 0){
		hdmi_rx_phy_debug(&rx.phy);
		hdmi_rx_ctrl_debug(&rx.ctrl);
		hdmi_rx_ctrl_hdcp_debug(&rx.ctrl);
	}

	return 0;    
}

bool hdmirx_hw_check_frame_skip(void)
{
	return false;
}

int hdmirx_hw_get_color_fmt(void)
{
	int color_format = color_format_mode;
	if((color_format_mode>=0) && (color_format_mode<=3)){
		color_format = color_format_mode;
	}
	else{
		color_format = video_params.video_format;
	}
	return 0;
}

int hdmirx_hw_get_dvi_info(void)
{
	return video_params.dvi;
}

int hdmirx_hw_get_3d_structure(unsigned char* _3d_structure, unsigned char* _3d_ext_data)
{
	/*
	if((hdmirx_hw_stru.vendor_specific_info.identifier == 0x000c03)&&
	(hdmirx_hw_stru.vendor_specific_info.hdmi_video_format == 0x2)){
	*_3d_structure = hdmirx_hw_stru.vendor_specific_info._3d_structure;
	*_3d_ext_data = hdmirx_hw_stru.vendor_specific_info._3d_ext_data;
	return 0;
	}
	*/
	return -1;
}

int hdmirx_hw_get_pixel_repeat(void)
{
	return (video_params.pixel_repetition+1);
}


static unsigned char is_frame_packing(void)
{
	/*
	if((hdmirx_hw_stru.vendor_specific_info.identifier == 0x000c03)&&
	(hdmirx_hw_stru.vendor_specific_info.hdmi_video_format == 0x2)&&
	(hdmirx_hw_stru.vendor_specific_info._3d_structure == 0x0)){
	return 1;
	}*/
	return 0;
}

typedef struct{
	unsigned int vic;
	unsigned char vesa_format;
	unsigned int ref_freq; /* 8 bit tmds clock */
	unsigned int active_pixels;
	unsigned int active_lines;
	unsigned int active_lines_fp;
}freq_ref_t;

static freq_ref_t freq_ref[]=
{
/* basic format*/
	{HDMI_640x480p60, 0, 25000, 640, 480, 480},
	{HDMI_480p60, 0, 27000, 720, 480, 1005},
	{HDMI_480p60_16x9, 0, 27000, 720, 480, 1005},
	{HDMI_480i60, 0, 27000, 1440, 240, 240},
	{HDMI_480i60_16x9, 0, 27000, 1440, 240, 240},
	{HDMI_576p50, 0, 27000, 720, 576, 1201},
	{HDMI_576p50_16x9, 0, 27000, 720, 576, 1201},
	{HDMI_576i50, 0, 27000, 1440, 288, 288},
	{HDMI_576i50_16x9, 0, 27000, 1440, 288, 288},
	{HDMI_576i50_16x9, 0, 27000, 1440, 145, 145},
	{HDMI_720p60, 0, 74250, 1280, 720, 1470},
	{HDMI_720p50, 0, 74250, 1280, 720, 1470},
	{HDMI_1080i60, 0, 74250, 1920, 540, 2228},
	{HDMI_1080i50, 0, 74250, 1920, 540, 2228},
	{HDMI_1080p60, 0, 148500, 1920, 1080, 1080},
	{HDMI_1080p24, 0, 74250, 1920, 1080, 2205},
	{HDMI_1080p25, 0, 74250, 1920, 1080, 2205},
	{HDMI_1080p30, 0, 74250, 1920, 1080, 2205},
	{HDMI_1080p50, 0, 148500, 1920, 1080, 1080},
/* extend format */
	{HDMI_1440x240p60, 0, 27000, 1440, 240, 240},      //vic 8
	{HDMI_1440x240p60_16x9, 0, 27000, 1440, 240, 240}, //vic 9
	{HDMI_2880x480i60, 0, 54000, 2880, 240, 240},      //vic 10
	{HDMI_2880x480i60_16x9, 0, 54000, 2880, 240, 240}, //vic 11
	{HDMI_2880x240p60, 0, 54000, 2880, 240, 240},      //vic 12
	{HDMI_2880x240p60_16x9, 0, 54000, 2880, 240, 240}, //vic 13
	{HDMI_1440x480p60, 0, 54000, 1440, 480, 480},      //vic 14
	{HDMI_1440x480p60_16x9, 0, 54000, 1440, 480, 480}, //vic 15

	{HDMI_1440x288p50, 0, 27000, 1440, 288, 288},      //vic 23
	{HDMI_1440x288p50_16x9, 0, 27000, 1440, 288, 288}, //vic 24
	{HDMI_2880x576i50, 0, 54000, 2880, 288, 288},      //vic 25
	{HDMI_2880x576i50_16x9, 0, 54000, 2880, 288, 288}, //vic 26
	{HDMI_2880x288p50, 0, 54000, 2880, 288, 288},      //vic 27
	{HDMI_2880x288p50_16x9, 0, 54000, 2880, 288, 288}, //vic 28
	{HDMI_1440x576p50, 0, 54000, 1440, 576, 576},      //vic 29
	{HDMI_1440x576p50_16x9, 0, 54000, 1440, 576, 576}, //vic 30

	{HDMI_2880x480p60, 0, 108000, 2880, 480, 480},     //vic 35
	{HDMI_2880x480p60_16x9, 0, 108000, 2880, 480, 480},//vic 36
	{HDMI_2880x576p50, 0, 108000, 2880, 576, 576},     //vic 37
	{HDMI_2880x576p50_16x9, 0, 108000, 2880, 576, 576},//vic 38
	{HDMI_1080i50_1250, 0, 72000, 1920, 540, 540},     //vic 39
	{HDMI_720p24, 0, 74250, 1280, 720, 1470},          //vic 60
	{HDMI_720p30, 0, 74250, 1280, 720, 1470},          //vic 62

/* vesa format*/
	{HDMI_800_600, 1, 0, 800, 600, 600},
	{HDMI_1024_768, 1, 0, 1024, 768, 768},
	{HDMI_720_400,  1, 0, 720, 400, 400},
	{HDMI_1280_768, 1, 0, 1280, 768, 768},
	{HDMI_1280_800, 1, 0, 1280, 800, 800},
	{HDMI_1280_960, 1, 0, 1280, 960, 960},
	{HDMI_1280_1024, 1, 0, 1280, 1024, 1024},
	{HDMI_1360_768, 1, 0, 1360, 768, 768},
	{HDMI_1366_768, 1, 0, 1366, 768, 768},
	{HDMI_1600_1200, 1, 0, 1600, 1200, 1200},
	{HDMI_1920_1200, 1, 0, 1920, 1200, 1200},
	{HDMI_1440_900, 1, 0, 1440, 900, 900},
	{HDMI_1400_1050, 1, 0, 1400, 1050, 1050},
	{HDMI_1680_1050, 1, 0, 1680, 1050, 1050},          //vic 79

	/* for AG-506 */
	{HDMI_480p60, 0, 27000, 720, 483, 483},
	{0, 0, 0}
};

static unsigned int get_vic_from_timing(void)
{
	int i;
	for(i = 0; freq_ref[i].vic; i++){
		if((video_params.hactive == freq_ref[i].active_pixels)&&
		    ((video_params.vactive == freq_ref[i].active_lines)||
		    (video_params.vactive == freq_ref[i].active_lines_fp))){
		    break;
		}
	}
	return freq_ref[i].vic;
}

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
	/* to do:
	TVIN_SIG_FMT_HDMI_1280x720P_24Hz_FRAME_PACKING,
	TVIN_SIG_FMT_HDMI_1280x720P_30Hz_FRAME_PACKING,

	TVIN_SIG_FMT_HDMI_1920x1080P_24Hz_FRAME_PACKING,
	TVIN_SIG_FMT_HDMI_1920x1080P_30Hz_FRAME_PACKING, // 150
	*/
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	unsigned int vic = video_params.video_mode;

	if(video_params.dvi){
		vic = get_vic_from_timing();
	}
	if(force_vic){
		vic = force_vic;
	}

	switch(vic){
		/* basic format */
		case HDMI_640x480p60:		    /*1*/
			fmt = TVIN_SIG_FMT_HDMI_640X480P_60HZ;
			break;
		case HDMI_480p60:                   /*2*/
		case HDMI_480p60_16x9:              /*3*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
			}
			break;
		case HDMI_720p60:                   /*4*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
			}
			break;
		case HDMI_1080i60:                  /*5*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ;
			}
			break;
		case HDMI_480i60:                   /*6*/
		case HDMI_480i60_16x9:              /*7*/
			fmt = TVIN_SIG_FMT_HDMI_1440X480I_60HZ;
			break;
		case HDMI_1080p60:		    /*16*/
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
			break;
		case HDMI_1080p24:		    /*32*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ;
			}
			break;
		case HDMI_576p50:		    /*17*/
		case HDMI_576p50_16x9:		    /*18*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ;
			}
			break;
		case HDMI_720p50:		    /*19*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_50HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_50HZ;
			}
			break;
		case HDMI_1080i50:		    /*20*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A;
			}
			break;
		case HDMI_576i50:		   /*21*/
		case HDMI_576i50_16x9:		   /*22*/
			fmt = TVIN_SIG_FMT_HDMI_1440X576I_50HZ;
			break;
		case HDMI_1080p50:		   /*31*/
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_50HZ;
			break;
		case HDMI_1080p25:		   /*33*/
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_25HZ;
			break;
		case HDMI_1080p30:		   /*34*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ;
			}
			break;
		case HDMI_720p24:		   /*60*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ;
			}
			break;
		case HDMI_720p30:		   /*62*/
			if(is_frame_packing()){
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_30HZ_FRAME_PACKING;
			}
			else{
				fmt = TVIN_SIG_FMT_HDMI_1280X720P_30HZ;
			}
			break;

		/* extend format */
		case HDMI_1440x240p60:
		case HDMI_1440x240p60_16x9:
			fmt = TVIN_SIG_FMT_HDMI_1440X240P_60HZ;
			break;
		case HDMI_2880x480i60:
		case HDMI_2880x480i60_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X480I_60HZ;
			break;
		case HDMI_2880x240p60:
		case HDMI_2880x240p60_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X240P_60HZ;
			break;
		case HDMI_1440x480p60:
		case HDMI_1440x480p60_16x9:
			fmt = TVIN_SIG_FMT_HDMI_1440X480P_60HZ;
			break;
		case HDMI_1440x288p50:
		case HDMI_1440x288p50_16x9:
			fmt = TVIN_SIG_FMT_HDMI_1440X288P_50HZ;
			break;
		case HDMI_2880x576i50:
		case HDMI_2880x576i50_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X576I_50HZ;
			break;
		case HDMI_2880x288p50:
		case HDMI_2880x288p50_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X288P_50HZ;
			break;
		case HDMI_1440x576p50:
		case HDMI_1440x576p50_16x9:
			fmt = TVIN_SIG_FMT_HDMI_1440X576P_50HZ;
			break;

		case HDMI_2880x480p60:
		case HDMI_2880x480p60_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X480P_60HZ;
			break;
		case HDMI_2880x576p50:
		case HDMI_2880x576p50_16x9:
			fmt = TVIN_SIG_FMT_HDMI_2880X576P_60HZ; //????, should be TVIN_SIG_FMT_HDMI_2880x576P_50Hz
			break;
		case HDMI_1080i50_1250:
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B;
			break;
		case HDMI_1080I120:	/*46*/
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_120HZ;
			break;
		case HDMI_720p120:	/*47*/
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_120HZ;
			break;
		case HDMI_1080p120:	/*63*/
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_120HZ;
			break;

		/* vesa format*/
		case HDMI_800_600:	/*65*/
			fmt = TVIN_SIG_FMT_HDMI_800X600_00HZ;
			break;
		case HDMI_1024_768:	/*66*/
			fmt = TVIN_SIG_FMT_HDMI_1024X768_00HZ;
			break;
		case HDMI_720_400:
			fmt = TVIN_SIG_FMT_HDMI_720X400_00HZ;
			break;
		case HDMI_1280_768:
			fmt = TVIN_SIG_FMT_HDMI_1280X768_00HZ;
			break;
		case HDMI_1280_800:
			fmt = TVIN_SIG_FMT_HDMI_1280X800_00HZ;
			break;
		case HDMI_1280_960:
			fmt = TVIN_SIG_FMT_HDMI_1280X960_00HZ;
			break;
		case HDMI_1280_1024:
			fmt = TVIN_SIG_FMT_HDMI_1280X1024_00HZ;
			break;
		case HDMI_1360_768:
			fmt = TVIN_SIG_FMT_HDMI_1360X768_00HZ;
			break;
		case HDMI_1366_768:
			fmt = TVIN_SIG_FMT_HDMI_1366X768_00HZ;
			break;
		case HDMI_1600_1200:
			fmt = TVIN_SIG_FMT_HDMI_1600X1200_00HZ;
			break;
		case HDMI_1920_1200:
			fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
			break;
		case HDMI_1440_900:            
			fmt = TVIN_SIG_FMT_HDMI_1440X900_00HZ;            
			break;	
		case HDMI_1400_1050:            
			fmt = TVIN_SIG_FMT_HDMI_1400X1050_00HZ;            
			break;
		case HDMI_1680_1050:            
			fmt = TVIN_SIG_FMT_HDMI_1680X1050_00HZ;            
			break;
		default:
			break;
		}

	return fmt;
}

bool hdmirx_hw_pll_lock(void)
{
	return (rx.state==HDMIRX_HWSTATE_SIG_STABLE);
}

bool hdmirx_hw_is_nosig(void)
{
	return rx.no_signal;
}

void hdmirx_hw_init(tvin_port_t port)
{
	struct hdmi_rx_ctrl_video v = {0};

	rx.port = port - TVIN_PORT_HDMI1;
	if(init == 0){
		init = 1;
		rx.phy.log_error = log_error;
		rx.phy.log_info = log_info;
		rx.phy.bsp_reset = phy_wrapper_phy_reset;
		rx.phy.bsp_pddq = phy_wrapper_pddq;
		rx.phy.bsp_svsmode = phy_wrapper_svsretmode;
		rx.phy.bsp_zcal_reset = platform_zcal_reset;
		rx.phy.bsp_zcal_done = phy_wrapper_zcal_done;
		rx.phy.bsp_config = phy_wrapper_setup_bsp;
		rx.phy.bsp_read = phy_wrapper_read;
		rx.phy.bsp_write = phy_wrapper_write;
		log_info("PHY GEN 3");
		rx.phy.cfg_clk = 62500;
		/* HDMI RX controller */
		rx.ctrl.log_error = log_error;
		rx.ctrl.log_info = log_info;
		rx.ctrl.bsp_reset = hdmi_rx_ctrl_reset;
		rx.ctrl.bsp_reset_hdmi = hdmi_rx_ctrl_reset_hdmi;
		rx.ctrl.bsp_edid_update = hdmi_rx_ctrl_edid_update;
		rx.ctrl.bsp_read = hdmi_rx_ctrl_read;
		rx.ctrl.bsp_write = hdmi_rx_ctrl_write;
		rx.ctrl.spin_init = os_spin_init;
		rx.ctrl.spin_destroy = os_spin_destroy;
		rx.ctrl.spin_lock_irq_save = os_spin_lock_irq_save;
		rx.ctrl.spin_unlock_irq_restore = os_spin_unlock_irq_restore;
		rx.ctrl.cfg_clk = rx.phy.cfg_clk;
		rx.ctrl.md_clk = 25000;
    
#if 0
		log_info("HDMI RX demo: " __TIME__ " " __DATE__);
		if (param != 0)
		{
			log_info("input parameter will be ignored");
		}
#endif
		if (platform_reset() < 0)
		{
			log_error(__func__, __LINE__, "platform reset");
			//goto exit;
		}
		else if (hdmi_rx_ctrl_open(&rx.ctrl) < 0) {
			log_error(__func__, __LINE__, "RX controller open");
			//goto exit_phy;
		}
		else if (hdmi_rx_ctrl_hdcp_config(&rx.ctrl, &rx.hdcp))
		{
			log_error(__func__, __LINE__, "RX HDCP configure");
			//goto exit;
		}
		else if (hdmi_rx_phy_open(&rx.phy) < 0)
		{
			log_error(__func__, __LINE__, "RX PHY open");
			//goto exit;
		/* For Fast Switching all HDMI RX Interrupts must be disabled */
		}
#if 1
		else if(request_irq(AM_IRQ1(HDMIRX_IRQ), &irq_handler, IRQF_SHARED, "hdmirx", (void *)&rx)){
			log_error(__func__, __LINE__, "RX IRQ request");
		}
#else    	
		else if (os_irq_request(rx.irq, irq_handler, (void *)&rx) < 0)
		{
			log_error(__func__, __LINE__, "RX IRQ request");
			//goto exit_ctrl;

		}
		else if (os_thread_create(cli_thread, 0) < 0)
		{
			log_error(__func__, __LINE__, "CLI thread create");
			goto exit_irq;
		}
#endif    
		hdmi_rx_phy_fast_switching(&rx.phy, 1);

		log_info("Press Enter for help");
		if (hdmi_rx_phy_config(&rx.phy, rx.port, 74250, 0) < 0)
		{
			log_error(__func__, __LINE__, "RX PHY configure");
			//goto exit_irq;
		}
		else if (rx_ctrl_config(&rx.ctrl, rx.port, &rx.hdcp) < 0){
			//goto exit_irq;
		}
	/*mach-meson6tv/include/mach/regs.h file is not OK*/
#if 0
		READ_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR);
		WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK)|(1 << 24));
		//READ_MPEG_REG(SYS_CPU_0_IRQ_IN1_INTR_STAT_CLR);
		//WRITE_MPEG_REG(SYS_CPU_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(SYS_CPU_0_IRQ_IN1_INTR_MASK)|(1 << HDMIRX_IRQ));
#endif
    
		hdmi_rx_ctrl_write (0x2c0,0x4C); /* shift to HDMI input port D */
		/* wait at least 4 video frames (at 24Hz) : 167ms for the mode detection
		recover the video mode */
		os_sleep(200);
		hdmi_rx_ctrl_get_video(&rx.ctrl, &v);
		//video_if_mode(true, v.hactive, v.vactive, v.pixel_repetition);

		/* Check If HDCP engine is in Idle state, if not wait for authentication time.
		 200ms is enough if no Ri errors */
		if (hdmi_rx_ctrl_read(0xe0) != 0)
		{
			os_sleep(200);
		}
		/* Enable LCD Display */
		//video_if_source(1);
	}
}

void hdmirx_hw_uninit(void)
{
	init = 0;

	free_irq(AM_IRQ1(24), (void *)&rx);

	if (hdmi_rx_ctrl_close(&rx.ctrl) < 0) {
		log_error(__func__, __LINE__, "RX controller close");
	}
	if (hdmi_rx_phy_close(&rx.phy) < 0) {
		log_error(__func__, __LINE__, "RX PHY close");
	}

}

void cec_usrcmd_set_dispatch(const char * buf, size_t count)
{
	return;
}

size_t cec_usrcmd_get_global_info(char * buf)
{
	return 0;
}

MODULE_PARM_DESC(color_format_mode, "\n color_format_mode \n");
module_param(color_format_mode, int, 0664);

MODULE_PARM_DESC(edid_mode, "\n edid_mode \n");
module_param(edid_mode, int, 0664);

MODULE_PARM_DESC(force_vic, "\n force_vic \n");
module_param(force_vic, int, 0664);
