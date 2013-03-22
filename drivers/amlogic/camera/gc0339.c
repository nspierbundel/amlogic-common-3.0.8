/*
 *gc0339 - This code emulates a real video device with v4l2 api
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <linux/i2c.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/amlogic/aml_camera.h>
#include <linux/mipi/am_mipi_csi2.h>
#include <media/amlogic/656in.h>
#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include "common/plat_ctrl.h"
#ifdef CONFIG_ARCH_MESON6
#include <mach/mod_gate.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend gc0339_early_suspend;
#endif

#define GC0339_CAMERA_MODULE_NAME "mipi-gc0339"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define GC0339_CAMERA_MAJOR_VERSION 0
#define GC0339_CAMERA_MINOR_VERSION 7
#define GC0339_CAMERA_RELEASE 0
#define GC0339_CAMERA_VERSION \
	KERNEL_VERSION(GC0339_CAMERA_MAJOR_VERSION, GC0339_CAMERA_MINOR_VERSION, GC0339_CAMERA_RELEASE)

static unsigned short DGain_shutter,AGain_shutter,DGain_shutterH,DGain_shutterL,AGain_shutterH,AGain_shutterL,shutterH,shutterL,shutter;
static unsigned short UXGA_Cap = 0;

MODULE_DESCRIPTION("mipi-gc0339 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static int video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int vidio_set_fmt_ticks=0;

static int gc0339_h_active=640;
static int gc0339_v_active=480;

static struct am_csi2_camera_para gc0339_para = {
    .name = GC0339_CAMERA_MODULE_NAME,
    .x_start = 0,
    .y_start = 0,
    .output_pixel = 0,
    .output_line = 0,
    .active_pixel = 0,
    .active_line = 0,
    .frame_rate = 0,
    .ui_val = 0,
    .hs_freq = 0,
    .clock_lane_mode = 0,
    .fmt = NULL,
};

/* supported controls */
static struct v4l2_queryctrl gc0339_qctrl[] = {
	{
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "white balance",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = 0,
		.maximum       = 8,
		.step          = 0x1,
		.default_value = 4,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_COLORFX,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "effect",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_WHITENESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "banding",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_BLUE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "scene mode",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct gc0339_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct gc0339_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},

	{
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},
	{
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	}
};

static struct gc0339_fmt *get_format(struct v4l2_format *f)
{
	struct gc0339_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct gc0339_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct gc0339_fmt        *fmt;
};

struct gc0339_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(gc0339_devicelist);

struct gc0339_device {
	struct list_head			gc0339_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct gc0339_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(gc0339_qctrl)];
};

static inline struct gc0339_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc0339_device, sd);
}

struct gc0339_fh {
	struct gc0339_device            *dev;

	/* video capture */
	struct gc0339_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

static inline struct gc0339_fh *to_fh(struct gc0339_device *dev)
{
	return container_of(dev, struct gc0339_fh, dev);
}

static struct v4l2_frmsize_discrete gc0339_prev_resolution[1]= //should include 352x288 and 640x480, those two size are used for recording
{
	{640,480},
};

static struct v4l2_frmsize_discrete gc0339_pic_resolution[1]=
{
	{640,480},
};

/* ------------------------------------------------------------------
	reg spec of GC0339
   ------------------------------------------------------------------*/

typedef struct aml_camera_i2c_fig2_s{
    unsigned char   addr;
    unsigned char    val;
    unsigned res1;
    unsigned res2;
} aml_camera_i2c_fig2_t;

struct aml_camera_i2c_fig2_s GC0339_script[] = {
    {0xfc,0x10,1,0},//clockenable
    {0xfe,0x00,1,0},//###########systemreg########################
    {0xf6,0x07,1,0},//pllmode
    {0xf7,0x01,1,0},
    {0xf7,0x03,1,0},//
    {0xfc,0x16,1,0},
    {0x06,0x00,1,0},//rowstart//###########mipireg########################
    {0x08,0x04,1,0},//colstart2
    {0x09,0x01,1,0},//488-2
    {0x0a,0xe8,1,0},
    {0x0b,0x02,1,0},//648-4+8=
    {0x0c,0x88,1,0},

    {0x0f,0x02,1,0},//[7:4]vb,0x,0x[3:0]hb
    {0x14,0x20,1,0},//
    {0x1a,0x21,1,0},//clk_delay
    {0x1b,0x80,1,0},//
    {0x1c,0x49,1,0},//
    {0x61,0x2a,1,0},//RAW8->2ayuv->1e
    {0x62,0x8c,1,0},
    {0x63,0x02,1,0},//lwc//652*
    {0x32,0x00,1,0},//globaloffset10
    {0x3a,0x20,1,0},//0x40
    {0x3b,0x20,1,0},
    {0x69,0x03,1,0},//phyenable
    {0x60,0x84,1,0},//mipienable//0x60,0x90//mipienableandlane_enalbe
    {0x65,0x10,1,0},
    {0x6c,0xaa,1,0},
    {0x6d,0x00,1,0},
    {0x67,0x10,1,0},
    {0x4a,0x40,1,0},
    {0x4b,0x40,1,0},
    {0x4c,0x40,1,0},
    {0xe8,0x04,1,0},
    {0xe9,0xbb,1,0},
    {0x42,0x20,1,0},//bi_b
    {0x47,0x10,1,0},
    {0x50,0x40,1,0},//60//globalgain
    {0xd0,0x00,1,0},
    {0xd3,0x50,1,0},//target
    {0xf6,0x05,1,0},//05-192m;07-96m
    {0x01,0x6a,1,0},//
    {0x02,0x0c,1,0},//
    {0x0f,0x00,1,0},
    {0x6a,0x11,1,0},//mipipaddriver

    {0x71,0x01,1,0},
    //{0x72,0x02,1,0},
    //{0x79,0x02,1,0},
    //{0x73,0x01,1,0},
    {0x7a,0x01,1,0},
    
    {0x73,0x02,1,0},
    {0x72,0x01,1,0},
    {0x79,0x01,1,0},	
    {0x76,0x02,1,0},
    {0x7b,0x02,1,0},

    {0x2e,0x30,1,0},//0->normal01->test[5]1pixel+8
    {0x2b,0x00,1,0},//fixdata
    {0x2c,0x03,1,0},//fixdata
    {0xd2,0x9e,1,0},
    {0x20,0xb0,1,0},//enabledddn
    {0x60,0x94,1,0},

    {0xff,0xff,0,0},
};

//load GC0339 parameters
void GC0339_init_regs(struct gc0339_device *dev)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
    unsigned char buf[2];
    while(1){
        if (GC0339_script[i].val==0xff&&GC0339_script[i].addr==0xff){
        	printk("GC0339_write_regs success in initial GC0339.\n");
        	break;
        }
        buf[0] = GC0339_script[i].addr;
        buf[1] = GC0339_script[i].val;
        if((i2c_put_byte_add8(client,buf, 2)) < 0){
        	printk("fail in initial GC0339. \n");
		return;
	 }
        i++;
    }
#if 0
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
    if (plat_dat&&plat_dat->custom_init_script) {
        i=0;
        aml_camera_i2c_fig_t*  custom_script = (aml_camera_i2c_fig_t*)plat_dat->custom_init_script;
        while(1){
            if (custom_script[i].val==0xff&&custom_script[i].addr==0xffff){
                printk("GC0339_write_custom_regs success in initial GC0339.\n");
                break;
            }
            if((i2c_put_byte(client,custom_script[i].addr, custom_script[i].val)) < 0){
                printk("fail in initial GC0339 custom_regs. \n");
                return;
            }
            i++;
        }
    }
#endif
    return;
}
/*************************************************************************
* FUNCTION
*    GC0339_set_param_wb
*
* DESCRIPTION
*    wb setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0339_set_param_wb(struct gc0339_device *dev,enum  camera_wb_flip_e para)//white balance
{
    //struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para){
        case CAM_WB_AUTO://auto
            break;
        case CAM_WB_CLOUD: //cloud
            break;
        case CAM_WB_DAYLIGHT: //
            break;
        case CAM_WB_INCANDESCENCE:
            break;
        case CAM_WB_TUNGSTEN:
            break;
        case CAM_WB_FLUORESCENT:
            break;
        case CAM_WB_MANUAL:
            // TODO
            break;
    }
} /* GC0339_set_param_wb */
/*************************************************************************
* FUNCTION
*    GC0339_set_param_exposure
*
* DESCRIPTION
*    exposure setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0339_set_param_exposure(struct gc0339_device *dev,enum camera_exposure_e para)
{
    //struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    switch (para){
        case EXPOSURE_N4_STEP:
        case EXPOSURE_N3_STEP:
        case EXPOSURE_N2_STEP:
        case EXPOSURE_N1_STEP:
        case EXPOSURE_0_STEP:	
        case EXPOSURE_P1_STEP:			
        case EXPOSURE_P2_STEP:
        case EXPOSURE_P3_STEP:					
        case EXPOSURE_P4_STEP:	
        default:
            break;
    }
} /* GC0339_set_param_exposure */
/*************************************************************************
* FUNCTION
*    GC0339_set_param_effect
*
* DESCRIPTION
*    effect setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0339_set_param_effect(struct gc0339_device *dev,enum camera_effect_flip_e para)
{
    //struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    switch (para){
        case CAM_EFFECT_ENC_NORMAL:
        case CAM_EFFECT_ENC_GRAYSCALE:
        case CAM_EFFECT_ENC_SEPIA:
        case CAM_EFFECT_ENC_SEPIAGREEN:
        case CAM_EFFECT_ENC_SEPIABLUE:
        case CAM_EFFECT_ENC_COLORINV:
        default:
            break;
    }
} /* GC0339_set_param_effect */

/*************************************************************************
* FUNCTION
*    GC0339_NightMode
*
* DESCRIPTION
*    This function night mode of GC0339.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0339_set_night_mode(struct gc0339_device *dev,enum  camera_night_mode_flip_e enable)
{
    //struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
}    /* GC0339_NightMode */

void GC0339_set_param_banding(struct gc0339_device *dev,enum  camera_night_mode_flip_e banding)
{
    //struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
}

void GC0339_set_resolution(struct gc0339_device *dev,int height,int width)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[2];
    buf[0] = 0x09;
    buf[1] = 0x01;
    i2c_put_byte_add8(client,buf, 2);
    buf[0] = 0x0a;
    buf[1] = 0xe8;
    i2c_put_byte_add8(client,buf, 2);
    buf[0] = 0x0b;
    buf[1] = 0x02;
    i2c_put_byte_add8(client,buf, 2);
    buf[0] = 0x0c;
    buf[1] = 0x88;
    i2c_put_byte_add8(client,buf, 2);
    mdelay(100);
    gc0339_h_active=640;
    gc0339_v_active=480;
}    /* GC0339_set_resolution */

unsigned char v4l_2_gc0339(int val)
{
    int ret=val/0x20;
    if(ret<4) return ret*0x20+0x80;
    else if(ret<8) return ret*0x20+0x20;
    else return 0;
}

static int gc0339_setting(struct gc0339_device *dev,int PROP_ID,int value )
{
    int ret=0;
    unsigned char buf[2];
    //unsigned char cur_val;
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    switch(PROP_ID)  {
        case V4L2_CID_BRIGHTNESS:
            dprintk(dev, 1, "setting brightned:%d\n",v4l_2_gc0339(value));
            buf[0] = 0x44;
            buf[1] = v4l_2_gc0339(value);
            i2c_put_byte_add8(client,buf, 2);
            break;
        case V4L2_CID_CONTRAST:
        case V4L2_CID_SATURATION:
        case V4L2_CID_HFLIP:    /* set flip on H. */
        case V4L2_CID_VFLIP:    /* set flip on V. */
            break;
        case V4L2_CID_DO_WHITE_BALANCE:
            if(gc0339_qctrl[0].default_value!=value){
                gc0339_qctrl[0].default_value=value;
                GC0339_set_param_wb(dev,value);
	         printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
            } 
            break;
        case V4L2_CID_EXPOSURE:
            if(gc0339_qctrl[1].default_value!=value){
                gc0339_qctrl[1].default_value=value;
                GC0339_set_param_exposure(dev,value);
                printk(KERN_INFO " set camera  exposure=%d. \n ",value);
            }
            break;
        case V4L2_CID_COLORFX:
            if(gc0339_qctrl[2].default_value!=value){
                gc0339_qctrl[2].default_value=value;
                GC0339_set_param_effect(dev,value);
                printk(KERN_INFO " set camera  effect=%d. \n ",value);
            }
            break;
        case V4L2_CID_WHITENESS:
            if(gc0339_qctrl[3].default_value!=value){
                gc0339_qctrl[3].default_value=value;
                GC0339_set_param_banding(dev,value);
                printk(KERN_INFO " set camera  banding=%d. \n ",value);
            }
            break;
        case V4L2_CID_BLUE_BALANCE:
            if(gc0339_qctrl[4].default_value!=value){
                gc0339_qctrl[4].default_value=value;
                GC0339_set_night_mode(dev,value);
                printk(KERN_INFO " set camera  scene mode=%d. \n ",value);
            }
            break;
        default:
            ret=-1;
            break;
    }
    return ret;
}

static void power_down_gc0339(struct gc0339_device *dev)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[2];
    buf[0] = 0xfc;
    buf[1] = 0;
    i2c_put_byte_add8(client,buf, 2); //clock
    buf[0] = 0xf7;
    buf[1] = 0;
    i2c_put_byte_add8(client,buf, 2);//pll
    buf[0] = 0x60;
    buf[1] = 0;
    i2c_put_byte_add8(client,buf, 2);//csi2
    buf[0] = 0x69;
    buf[1] = 0;
    i2c_put_byte_add8(client,buf, 2);//phy
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

extern  int am_csi2_fill_buffer(struct am_csi2_camera_para *para,void* output);
#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void gc0339_fillbuff(struct gc0339_fh *fh, struct gc0339_buffer *buf)
{
    struct gc0339_device *dev = fh->dev;
    void *vbuf = videobuf_to_vmalloc(&buf->vb);
    dprintk(dev,1,"%s\n", __func__);
    if (!vbuf)
        return;
    gc0339_para.x_start= 0;
    gc0339_para.y_start= 0;    
    gc0339_para.output_line= gc0339_v_active;
    gc0339_para.output_pixel= gc0339_h_active;
    gc0339_para.active_line = gc0339_v_active;
    gc0339_para.active_pixel = gc0339_h_active;
    gc0339_para.frame_rate = 236;
    gc0339_para.fmt = (struct am_csi2_camera_para*)fh->fmt;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
    am_csi2_fill_buffer(&gc0339_para,vbuf);
    buf->vb.state = VIDEOBUF_DONE;
}

static void gc0339_thread_tick(struct gc0339_fh *fh)
{
	struct gc0339_buffer *buf;
	struct gc0339_device *dev = fh->dev;
	struct gc0339_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct gc0339_buffer, vb.queue);
	dprintk(dev, 1, "%s\n", __func__);
	dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	gc0339_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)					\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void gc0339_sleep(struct gc0339_fh *fh)
{
	struct gc0339_device *dev = fh->dev;
	struct gc0339_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	gc0339_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int gc0339_thread(void *data)
{
	struct gc0339_fh  *fh = data;
	struct gc0339_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		gc0339_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int gc0339_start_thread(struct gc0339_fh *fh)
{
	struct gc0339_device *dev = fh->dev;
	struct gc0339_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(gc0339_thread, fh, "mipi-gc0339");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc0339_stop_thread(struct gc0339_dmaqueue  *dma_q)
{
	struct gc0339_device *dev = container_of(dma_q, struct gc0339_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);
	/* shutdown control thread */
	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/
static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct gc0339_fh  *fh = vq->priv_data;
	struct gc0339_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
	*size = (fh->width*fh->height*fh->fmt->depth)>>3;
	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct gc0339_buffer *buf)
{
	struct gc0339_fh  *fh = vq->priv_data;
	struct gc0339_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1920
#define norm_maxh() 1200
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct gc0339_fh     *fh  = vq->priv_data;
	struct gc0339_device    *dev = fh->dev;
	struct gc0339_buffer *buf = container_of(vb, struct gc0339_buffer, vb);
	int rc;
    //int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width*fh->height*fh->fmt->depth)>>3;
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	//precalculate_bars(fh);

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void
buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct gc0339_buffer    *buf  = container_of(vb, struct gc0339_buffer, vb);
	struct gc0339_fh        *fh   = vq->priv_data;
	struct gc0339_device       *dev  = fh->dev;
	struct gc0339_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct gc0339_buffer   *buf  = container_of(vb, struct gc0339_buffer, vb);
	struct gc0339_fh       *fh   = vq->priv_data;
	struct gc0339_device      *dev  = (struct gc0339_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops gc0339_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct gc0339_fh  *fh  = priv;
	struct gc0339_device *dev = fh->dev;

	strcpy(cap->driver, "mipi-gc0339");
	strcpy(cap->card, "gc0339");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC0339_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct gc0339_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct gc0339_fh *fh = priv;

	f->fmt.pix.width = fh->width;
	f->fmt.pix.height = fh->height;
	f->fmt.pix.field = fh->vb_vidq.field;
	f->fmt.pix.pixelformat = fh->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = 	f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct gc0339_fh  *fh  = priv;
	struct gc0339_device *dev = fh->dev;
	struct gc0339_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != field) {
		dprintk(dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw  = norm_maxw();
	maxh  = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
			      &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =	f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct gc0339_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct gc0339_device *dev = fh->dev;

	int ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dprintk(fh->dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;
	if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
		vidio_set_fmt_ticks=1;
		GC0339_set_resolution(dev,fh->height,fh->width);
	}
	else if(vidio_set_fmt_ticks==1){
		GC0339_set_resolution(dev,fh->height,fh->width);
	}
	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct gc0339_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0339_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0339_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0339_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct gc0339_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
    struct gc0339_fh  *fh = priv;
    int ret = 0 ;
    if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;
    if (i != fh->type)
        return -EINVAL;

    //gc0339_para.x_start= 0;
    //gc0339_para.y_start= 0;    
    //gc0339_para.output_line= gc0339_v_active;
    //gc0339_para.output_pixel= gc0339_h_active;
    gc0339_para.active_line = gc0339_v_active;
    gc0339_para.active_pixel = gc0339_h_active;
    gc0339_para.frame_rate = 236;
    gc0339_para.ui_val = 6; // 6 ns
    gc0339_para.hs_freq = 96000000;//96 MHZ
    gc0339_para.clock_lane_mode = 0;
    gc0339_para.fmt = (struct am_csi2_camera_para*)fh->fmt;
    ret = videobuf_streamon(&fh->vb_vidq);
    printk(KERN_INFO " vidioc_streamon+++ ,%d\n ",ret);
    if(ret == 0){
        ret = start_mipi_csi2_service(&gc0339_para);	
        if(ret<0)
            videobuf_streamoff(&fh->vb_vidq);
        else
            fh->stream_on = 1;
    }
    return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
    struct gc0339_fh  *fh = priv;

    int ret = 0 ;
    if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;
    if (i != fh->type)
        return -EINVAL;
    ret = videobuf_streamoff(&fh->vb_vidq);
    printk(KERN_INFO " vidioc_streamoff+++ %d,\n ",ret);
    if(ret == 0 ){
        ret = stop_mipi_csi2_service(&gc0339_para);
        fh->stream_on = 0;
    }
    return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct gc0339_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format){
			fmt = &formats[i];
			break;
		}
	}
	if (fmt == NULL)
		return -EINVAL;
	if (fmt->fourcc == V4L2_PIX_FMT_NV21){
		if (fsize->index >= ARRAY_SIZE(gc0339_prev_resolution))
			return -EINVAL;
		frmsize = &gc0339_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(gc0339_pic_resolution))
			return -EINVAL;
		frmsize = &gc0339_pic_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	//if (inp->index >= NUM_INPUTS)
		//return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);

	return (0);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct gc0339_fh *fh = priv;
	struct gc0339_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct gc0339_fh *fh = priv;
	struct gc0339_device *dev = fh->dev;

	//if (i >= NUM_INPUTS)
		//return -EINVAL;

	dev->input = i;
	//precalculate_bars(fh);

	return (0);
}

	/* --- controls ---------------------------------------------- */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0339_qctrl); i++){
		if (qc->id && qc->id == gc0339_qctrl[i].id) {
			memcpy(qc, &(gc0339_qctrl[i]),
				sizeof(*qc));
			return (0);
		}
	}
	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc0339_fh *fh = priv;
	struct gc0339_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0339_qctrl); i++){
		if (ctrl->id == gc0339_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct gc0339_fh *fh = priv;
	struct gc0339_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0339_qctrl); i++){
		if (ctrl->id == gc0339_qctrl[i].id) {
			if (ctrl->value < gc0339_qctrl[i].minimum ||
			    ctrl->value > gc0339_qctrl[i].maximum ||
			    gc0339_setting(dev,ctrl->id,ctrl->value)<0) {
				return -ERANGE;
			}
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	}
	return -EINVAL;
}

/* ------------------------------------------------------------------
	File operations for the device
   ------------------------------------------------------------------*/

static int gc0339_open(struct file *file)
{
	struct gc0339_device *dev = video_drvdata(file);
	struct gc0339_fh *fh = NULL;
	int retval = 0;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif		
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	GC0339_init_regs(dev);
	msleep(40);
	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	dprintk(dev, 1, "open %s type=%s users=%d\n",
		video_device_node_name(dev->vdev),
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

    	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	spin_lock_init(&dev->slock);
	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		dev->users--;
		retval = -ENOMEM;
	}
	mutex_unlock(&dev->mutex);

	if (retval)
		return retval;

	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 640;
	fh->height   = 480;
	fh->stream_on = 0 ;
	/* Resets frame counters */
	dev->jiffies = jiffies;


	videobuf_queue_vmalloc_init(&fh->vb_vidq, &gc0339_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct gc0339_buffer), fh,NULL);

	gc0339_start_thread(fh);

	return 0;
}

static ssize_t
gc0339_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct gc0339_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
gc0339_poll(struct file *file, struct poll_table_struct *wait)
{
	struct gc0339_fh        *fh = file->private_data;
	struct gc0339_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int gc0339_close(struct file *file)
{
	struct gc0339_fh         *fh = file->private_data;
	struct gc0339_device *dev       = fh->dev;
	struct gc0339_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);

	gc0339_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if(fh->stream_on){
	    stop_mipi_csi2_service(&gc0339_para);
	}
	videobuf_mmap_free(&fh->vb_vidq);

	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
		video_device_node_name(vdev), dev->users);

	gc0339_h_active=640;
	gc0339_v_active=480;
	gc0339_qctrl[0].default_value=0;
	gc0339_qctrl[1].default_value=4;
	gc0339_qctrl[2].default_value=0;
	gc0339_qctrl[3].default_value=0;
	gc0339_qctrl[4].default_value=0;


	power_down_gc0339(dev);

	msleep(10);
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}

	msleep(10);
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif		
	return 0;
}

static int gc0339_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gc0339_fh  *fh = file->private_data;
	struct gc0339_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc0339_fops = {
	.owner		= THIS_MODULE,
	.open           = gc0339_open,
	.release        = gc0339_close,
	.read           = gc0339_read,
	.poll		= gc0339_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = gc0339_mmap,
};

static const struct v4l2_ioctl_ops gc0339_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device gc0339_template = {
	.name		= "gc0339_v4l",
	.fops           = &gc0339_fops,
	.ioctl_ops 	= &gc0339_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int gc0339_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0339, 0);
}

static const struct v4l2_subdev_core_ops gc0339_core_ops = {
	.g_chip_ident = gc0339_g_chip_ident,
};

static const struct v4l2_subdev_ops gc0339_ops = {
	.core = &gc0339_core_ops,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_gc0339_early_suspend(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->early_suspend) {
			plat_dat->early_suspend();
		}
	}
}

static void aml_gc0339_late_resume(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->late_resume) {
			plat_dat->late_resume();
		}
	}
}
#endif

static int gc0339_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct gc0339_device *t;
	struct v4l2_subdev *sd;
	aml_plat_cam_data_t* plat_dat;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0339_ops);
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &gc0339_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	/* Register it */
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		t->platform_dev_data.device_disable=plat_dat->device_disable;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	gc0339_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	gc0339_early_suspend.suspend = aml_gc0339_early_suspend;
	gc0339_early_suspend.resume = aml_gc0339_late_resume;
	gc0339_early_suspend.param = plat_dat;
	register_early_suspend(&gc0339_early_suspend);
#endif

	return 0;
}

static int gc0339_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}

static int gc0339_suspend(struct i2c_client *client, pm_message_t state)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *t = to_dev(sd);
	struct gc0339_fh  *fh = to_fh(t);
	if(fh->stream_on == 1){
		stop_mipi_csi2_service(&gc0339_para);
	}
	power_down_gc0339(t);
	return 0;
}

static int gc0339_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *t = to_dev(sd);
	struct gc0339_fh  *fh = to_fh(t);
	//gc0339_para.x_start= 0;
	//gc0339_para.y_start= 0;    
	//gc0339_para.output_line= gc0339_v_active;
	//gc0339_para.output_pixel= gc0339_h_active;
	gc0339_para.active_line = gc0339_v_active;
	gc0339_para.active_pixel = gc0339_h_active;
	gc0339_para.frame_rate = 236;
	gc0339_para.fmt = (struct am_csi2_camera_para*)fh->fmt;
	GC0339_init_regs(t);
	if(fh->stream_on == 1){
       	start_mipi_csi2_service(&gc0339_para);
	}
	return 0;
}


static const struct i2c_device_id gc0339_id[] = {
	{ "gc0339_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0339_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "mipi-gc0339",
	.probe = gc0339_probe,
	.remove = gc0339_remove,
	.suspend = gc0339_suspend,
	.resume = gc0339_resume,
	.id_table = gc0339_id,
};

