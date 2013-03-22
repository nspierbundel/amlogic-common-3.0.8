/*
 * d2d3 char device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
//#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>

/* Amlogic headers */
#include <mach/am_regs.h>
#include <mach/register.h>
#include <linux/amports/canvas.h>
/* Local include */
#include "d2d3_drv.h"
#include "d2d3_regs.h"



/******debug********/

static int capture_print_en = 0;
module_param(capture_print_en, int, 0664);
MODULE_PARM_DESC(capture_print_en, "\n capture_print_en\n");

static int data_print_en = 0;
module_param(data_print_en, int, 0664);
MODULE_PARM_DESC(data_print_en, "\n data_print_en\n");

static int bypass_slicer = 1;
module_param(bypass_slicer, int, 0664);
MODULE_PARM_DESC(bypass_slicer, "\n bypass_slicer\n");

static unsigned int d2d3_canvas_ids[D2D3_CANVAS_MAX_CNT] = {55,56};
static unsigned int d2d3_reg_table[D2D3_REG_NUM]={
        0x80000d1f,/*0x2b00*/        0x02cf01df,/*0x2b01*/
        0x02cf077f,/*0x2b02*/        0x00000167,/*0x2b03*/
        0x000300ef,/*0x2b04*/        0x40000000,/*0x2b05*/
        0x00000055,/*0x2b06*/        0x001ec8ff,/*0x2b07*/
        0x00ff001e,/*0x2b08*/        0xffff3fc0,/*0x2b09*/
        0x00f00003,/*0x2b0a*/        0x40000000,/*0x2b0b*/
        0x0001b340,/*0x2b0c*/        0x0504403f,/*0x2b0d*/
        0x0504403f,/*0x2b0e*/        0x00000000,/*0x2b0f*/
        0x00000008,/*0x2b10*/        0x10001000,/*0x2b11*/
        0x08000800,/*0x2b12*/        0x10001000,/*0x2b13*/
        0x00100c10,/*0x2b14*/        0x00100c10,/*0x2b15*/
        0x00404013,/*0x2b16*/        0x00000000,/*0x2b17*/
        0x000000a3,/*0x2b18*/        0xbb2dc0c0,/*0x2b19*/
        0xbb1ec0c0,/*0x2b1a*/        0x7777b3b3,/*0x2b1b*/
        0xb010b004,/*0x2b1c*/        0x02cc0000,/*0x2b1d*/
        0x58381808,/*0x2b1e*/        0x58381808,/*0x2b1f*/
        0x00008080,/*0x2b20*/        0x00000000,/*0x2b21*/
        0x00000000,/*0x2b22*/        0x00000000,/*0x2b23*/
        0x00000116,/*0x2b24*/        0x01670000,/*0x2b25*/
        0x00ef0000,/*0x2b26*/        0x00ef0167,/*0x2b27*/
        0x00000116,/*0x2b28*/        0x01670000,/*0x2b29*/
        0x03bf0000,/*0x2b2a*/        0x00000000,/*0x2b2b*/
        0x0000000c,/*0x2b2c*/        0x00000000,/*0x2b2d*/
        0x00000000,/*0x2b2e*/        0x00000100,/*0x2b2f*/
        0x00000000,/*0x2b30*/        0x00000000,/*0x2b31*/
        0x00000000,/*0x2b32*/        0x00000000,/*0x2b33*/
        0x00000000,/*0x2b34*/        0x00000000,/*0x2b35*/
        0x00000000,/*0x2b36*/        0x00000000,/*0x2b37*/
        0x00000000,/*0x2b38*/        0x00000000,/*0x2b39*/
        0x00000000,/*0x2b3a*/        0x00000000,/*0x2b3b*/
        0x00000000,/*0x2b3c*/        0x00000000,/*0x2b3d*/
        0x00000000,/*0x2b3e*/        0x00000000,/*0x2b3f*/
};
inline void d2d3_config_dpg_canvas(struct d2d3_dev_s *devp)
{
        unsigned int canvas_id = d2d3_canvas_ids[D2D3_DPG_CANVAS_INDEX];
        canvas_config(canvas_id,devp->dpg_addr, devp->canvas_w,devp->canvas_h,
                CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
}

inline void d2d3_config_dpr_canvas(struct d2d3_dev_s *devp)
{
        unsigned int canvas_id = d2d3_canvas_ids[D2D3_DPR_CANVAS_INDEX];
        canvas_config(canvas_id,devp->dpr_addr, devp->canvas_w,devp->canvas_h,
                CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
}


/*config the register use 720x480*/
inline void d2d3_default_config()
{
        int i = 0;
        for(i=0;i<D2D3_REG_NUM;i++)
        WR(D2D3_CBUS_BASE+i,d2d3_reg_table[i]);
}
inline void d2d3_canvas_config(struct d2d3_dev_s *devp)
{
        int i, canvas_id;
        unsigned long canvas_addr;

        devp->canvas_w = D2D3_HORIZONTAL_PIXLE_MAX;
        devp->canvas_h = devp->param.height>>1;//1/2        
        devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w * devp->canvas_h);
        printk("[d2d3..]: canvas configuration table:\n");

        for (i = 0; i < devp->canvas_max_num; i++)
        {
                canvas_id = d2d3_canvas_ids[i];
                //canvas_addr = canvas_get_addr(canvas_id);
                /*reinitlize the canvas*/
                canvas_addr = devp->mem_start + devp->canvas_max_size * i;
                canvas_config(canvas_id, canvas_addr, devp->canvas_w, devp->canvas_h,
                                CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
                pr_info("   %d: 0x%lx-0x%lx %ux%u\n",
                                canvas_id, canvas_addr, canvas_addr + devp->canvas_max_size, devp->canvas_w, devp->canvas_h);
        }
}

inline void d2d3_set_wr_canvas_id(unsigned int canvas_id)
{
        WRITE_CBUS_REG_BITS(D2D3_DWMIF_CTRL, canvas_id, 0, 8);
}

inline void d2d3_set_rd_canvas_id(unsigned int canvas_id)
{
        WRITE_CBUS_REG_BITS(D2D3_DRMIF_CTRL, canvas_id, 0, 8);
}

inline int d2d3_canvas_init(struct d2d3_dev_s *devp)
{
        unsigned int i,canvas_id,canvas_addr;
        devp->canvas_w = D2D3_HORIZONTAL_PIXLE_MAX;
        devp->canvas_h = devp->param.height>>1;//1/2        
        devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*devp->canvas_h);
        printk("[d2d3..]%s canvas max size 0x%x.\n",__func__);
        devp->canvas_max_num = devp->mem_size/devp->canvas_max_size;
        if(devp->canvas_max_num > D2D3_CANVAS_MAX_CNT){                
                devp->canvas_max_num = D2D3_CANVAS_MAX_CNT;
                printk("[d2d3..]%s d2d3 get %u over %u buffer.\n",__func__,devp->canvas_max_num, D2D3_CANVAS_MAX_CNT);
        } 
        else if(devp->canvas_max_num < D2D3_CANVAS_MAX_CNT){
                printk("[d2d3..]%s d2d3 get %u small than %u buffer.\n",__func__,devp->canvas_max_num, D2D3_CANVAS_MAX_CNT);
                return -1;
        }
                
        printk("[d2d3..]: canvas initial table:\n");
        for(i=0; i<devp->canvas_max_num;i++){
                canvas_id = d2d3_canvas_ids[i];
                canvas_addr = devp->mem_start + devp->canvas_max_size*i;
                if(i == D2D3_DPG_CANVAS_INDEX)
                        devp->dpg_addr= canvas_addr;
                if(i == D2D3_DPR_CANVAS_INDEX)
                        devp->dpr_addr = canvas_addr;
                canvas_config(canvas_id, canvas_addr, devp->canvas_w, devp->canvas_h,
                                           CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
                printk("   %d: 0x%x-0x%x  %dx%d (%d KB)\n",
                canvas_id, canvas_addr, (canvas_addr + devp->canvas_max_size),
                devp->canvas_w, devp->canvas_h, (devp->canvas_max_size >> 10));
        }
        return 0;
        
}


//*********************************************************************
//       Function to configure the D2D3 Data Flow
//**********************************************************************
//static void Enable_D2D3DBR_Path(int d2d3_en, int d2d3_flow_mode)
void enable_d2d3dbr_path(bool d2d3_en, int d2d3_flow_mode)
{
        //d2d3_flow_mode
        //1: D2D3 DPG: NRW (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> DBR (D2D3) -> VPP (Scaler) -> TVENC
        //2: D2D3 DPG: NRW (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> VPP (Scaler) -> DBR (D2D3) -> VPP (post process)
        //3: D2D3 DPG: VDR (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> DBR (D2D3) -> VPP -> TVENC
        //4: D2D3 DPG: VDR (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> VPP (Scaler) -> DBR (D2D3) -> VPP (post process)
        int  nrw2dpg, vpp2dbr;

        vpp2dbr =  (d2d3_flow_mode == 2) ||  (d2d3_flow_mode == 4);
        nrw2dpg =  (d2d3_flow_mode == 1) ||  (d2d3_flow_mode == 2);
        if(d2d3_en)
        {
                //enable clk_d2d3 & clk_d2d3_reg
                //WR_REG_BITS (D2D3_INTF_CTRL0, 3, 22, 2); //D2D3_INTF_CTRL0[23:22]
                //enable nrw_split: enable the path of "NRW memory WRite" and "NRW to D2D3"
                WR_REG_BITS(D2D3_INTF_CTRL0, (nrw2dpg ? 3:1),26,2); // D2D3_INTF_CTRL0[27:26]: bit16 for NRW and bit 27 for DPG
                //D2D3 DPG selector
                WR_REG_BITS(D2D3_INTF_CTRL0,(nrw2dpg ? 3:4),0,3);  //d2d3_v0_sel: 3 from NRW, 4 from VDR
                WR_REG_BITS(D2D3_INTF_CTRL0,(nrw2dpg ? 1:0),13,3); //v0_gofld_sel: 1 to use "pre_frame_rst" in DEINT, 0 use vsync
                //WR_REG_BITS(D2D3_INTF_LENGTH,dpg_src_size_x-1,0,13); //v0_line_length; [28:16]:v1_line_length
                //D2D3 DBR selector
                WR_REG_BITS(D2D3_INTF_CTRL0,(vpp2dbr ? 2:1),4,2);  //d2d3_v1_sel: source select from VPP Scaler
                WR_REG_BITS(D2D3_INTF_CTRL0,0,16,3); //v1_gofld_sel: use vsync from TVENC
                //WR_REG_BITS(D2D3_INTF_CTRL0,6,6,7);      //hold lines
                //VPP input selector
                WR_REG_BITS(VPP_INPUT_CTRL,0,0,3); //always "no interleave"
                WR_REG_BITS(VPP_INPUT_CTRL,3,6,3); //always use D2D3 DBR left channel output
        }
        else
        {
                //enable clk_d2d3 & clk_d2d3_reg
                //WR_REG_BITS (D2D3_INTF_CTRL0, 3, 22, 2); //D2D3_INTF_CTRL0[23:22]
                //enable nrw_split: enable the path of "NRW memory WRite" and "NRW to D2D3"
                WR_REG_BITS(D2D3_INTF_CTRL0, 1,26,2); // D2D3_INTF_CTRL0[27:26]: only NRW
                //D2D3 DPG selector
                WR_REG_BITS(D2D3_INTF_CTRL0,0,0,3);  //d2d3_v0_sel: 0 -> no use
                //D2D3 DBR selector
                WR_REG_BITS(D2D3_INTF_CTRL0,0,4,2);  //d2d3_v1_sel: 0 -> no use
                //VPP input selector
                WR_REG_BITS(VPP_INPUT_CTRL,0,0,3); //always "no interleave"
                WR_REG_BITS(VPP_INPUT_CTRL,1,6,3); //always use VDR as input
        }
}



//*********************************************************************************
//   Functions of D2D3 Driver
//*********************************************************************************
//=================================================================================
//static void d2d3_enable_dbg(int size_x, int size_y, int scale_x, int scale_y, int canvas_idx,
//                     sD2D3_CBDGParam sCBDG_Param, sD2D3_MBDGParam sMBDG_Param,
//                     sD2D3_LBDGParam sLBDG_Param
//                    )
void d2d3_enable_dbg(struct d2d3_dev_s *devp, struct d2d3_depth_s *depthp)
{
        unsigned int szxScaled, szyScaled;
        struct d2d3_param_s *parm = &devp->param;

        //0x00:{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        WR_REG_BITS(D2D3_GLB_CTRL,1,0,1-0); //Set dpg_en = 1

        //0x01:{szx_m1(16),szy_m1(0)}
        WR(D2D3_DPG_INPIC_SIZE,((depthp->dpg_size_x-1)<<16) |(depthp->dpg_size_y-1)); //set image size of DPG

        //Config SCU18
        szxScaled = depthp->depthsize_x;
        szyScaled = depthp->depthsize_y;
        if(szxScaled>D2D3_HORIZONTAL_PIXLE_MAX ||(szyScaled>256) ||(parm->scale_x == 0))
                printk("Error: WRong scd81_factor for D2D3_SCD81......\n");

        //=== SCD81 ===
        //0x06:{scu18_iniph_v(24),scu18_iniph_h(16),(12),scd81_predrop_en(11),cg_csc_sel(9),scu18_rep_en(8),scu18_factor(4),scd81_factor(0)}
        WR_REG_BITS(D2D3_SCALER_CTRL, depthp->dpg_scale_x|(depthp->dpg_scale_y<<2),0,4);  //Set SCD81 factor:

        WR_REG_BITS(D2D3_SCALER_CTRL,0,11,12-11); //Set SCD81_DROP: scd81_predrop_en=0
        //0x05:{reg_scd81_hphs_step(16),reg_scd81_hphs_ini(0)}
        //WR_REG_BITS(D2D3_PRE_SCD_H,....);     //Set SCD81_DROP: phase in horizontal

        //0x0b:{reg_scd81_vphs_step(16),reg_scd81_vphs_ini(0)}
        //WR_REG_BITS(D2D3_PRE_SCD_V,....);     //Set SCD81_DROP: phase in vertical

        //0x03:{dg_win_x_start(16),dg_win_x_end(0)}
        WR(D2D3_DGEN_WIN_HOR,(0<<16) | (szxScaled-1)); //set Windows of DPG
        //0x04:{dg_win_y_start(16),dg_win_y_end(0)}
        WR(D2D3_DGEN_WIN_VER,(3<<16) | (szyScaled-1)); //set Windows of DPG

        //=== Configure the DPG Models ===
        devp->cbdg.yscaled = szyScaled;
        devp->cbdg.cg_vp_y_thr = -1;
        d2d3_config_dpg_cbdg(&devp->cbdg);
        d2d3_config_dpg_mbdg(&devp->mbdg,depthp);
        d2d3_config_dpg_lbdg(&devp->lbdg);

        //configure the DBLD
        d2d3_config_dpg_dbld(-1,-1,-1); //use the default value

        //=== Configure the DWMIF ===
        //0x25:{dwmif_end_x(16),dwmif_start_x(0)}
        WR(D2D3_DWMIF_HPOS,(0x0)|((szxScaled-1)<<16));

        //0x26:{dwmif_end_y(16),dwmif_start_y(0)}
        WR(D2D3_DWMIF_VPOS,(0x0)|((szyScaled-1)<<16));

        //0x27:{dwmif_vsize_m1(16),dwmif_hsize_m1(0)}
        WR(D2D3_DWMIF_SIZE,(szxScaled-1)|((szyScaled-1)<<16));

        //0x24:{x_rev(17),y_rev(16),dw_done_clr(15),dw_little_endian(14),dw_pic_struct(12),dw_urgent(11),
        //dw_clr_WRrsp(10),dw_canvas_WR(9),dw_req_en(8),dw_canvas_index(0)}
        WR_REG_BITS(D2D3_DWMIF_CTRL,d2d3_canvas_ids[0],0,8-0);
        WR_REG_BITS(D2D3_DWMIF_CTRL,1,8,9-8); //dw_req_en = 1;

}

//Configure the CBDG
//static void d2d3_config_dpg_cbdg(int enable, int cg_vpos_adpt_en, int cg_vpos_en, int cg_vpos_thr, int cg_vp_y_thr)
void d2d3_config_dpg_cbdg(struct d2d3_cbdg_param_s *cbdg)
{
        //0x00:{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        WR_REG_BITS(D2D3_GLB_CTRL,cbdg->cbdg_en,2,3-2); //Set cg_en = 1

        //0x09: D2D3_CG_PARAM_1
        //{cg_vp_rel_k(24),cg_vp_y_thr(16),cg_meet_dval(8),cg_unmt_dval(0)}
        if(cbdg->cg_vp_y_thr<0)
                WR_REG_BITS(D2D3_CG_PARAM_1,cbdg->cg_vp_y_thr,16,24-16); //Set cg_vp_y_thr: defalut->0x10

        //0x0a: D2D3_CG_PARAM_2
        //{cg_vpos_thr(16),(8),cg_vpos_en(7),cg_vpos_adpt_en(6),cg_lpf_bypass(4),cg_vp_rel_s(0)}
        WR_REG_BITS(D2D3_CG_PARAM_2,cbdg->yscaled,16,16); //set cg_vpos_thr: szyScaled
        WR_REG_BITS(D2D3_CG_PARAM_2,(cbdg->cg_vpos_en<<1)|cbdg->cg_vpos_adpt_en,6,2); //set cg_vpos_en & cg_vpos_adpt_en
}

//Configure the MBDG
//static void d2d3_config_dpg_mbdg(int enable, int szx8, int szy8, int mg_vp_en, int mg_y_adapt_en, int mg_xmm_adapt_en,
//                          int mg_x_adapt_en, int mg_sw_en, int mg_iir_en, int mg_iir)
void d2d3_config_dpg_mbdg(struct d2d3_mbdg_param_s *mbdg,struct d2d3_depth_s *depthp)
{
        //0x00:{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        WR_REG_BITS(D2D3_GLB_CTRL,mbdg->mbdg_en,3,1); //Set mg_en = 1

        //0x18:{(18),mg_vp_en(17),mg_sw_en(16),mg_owin_fill(8),mg_iir_en(7),mg_iir(0)}
        WR_REG_BITS(D2D3_MBDG_CTRL,(mbdg->mg_vp_en<<1)|mbdg->mg_sw_en,16,2); //set mg_vp_en & mg_sw_en
        if(mbdg->mg_iir_en==1)
                WR_REG_BITS(D2D3_MBDG_CTRL,(mbdg->mg_iir_en<<7)|(mbdg->mg_iir&0x7f),0,8); //set IIR


        //0x19: {mg_dtl_pxl_left(28),mg_dtl_pxl_right(24),mg_cx_sw(16),mg_ux_sw(8),mg_dx_sw(0)}
        if(mbdg->mg_sw_en ==1){
                WR_REG_BITS(D2D3_MBDG_PARAM_0,depthp->depthsize_x>>1,16,24-16); //set mg_cx_sw: default (szx8/2)
        //0x1a:{mg_dtl_pxl_up(28),mg_dtl_pxl_dn(24),mg_cy_sw(16),mg_uy_sw(8),mg_dy_sw(0)}
                WR_REG_BITS(D2D3_MBDG_PARAM_1,depthp->depthsize_y>>1,16,24-16); //set mg_cy_sw: default (szy8/2)
        }
        //0x1b:{mg_dtl_ln_up(24),mg_dtl_ln_dn(16),mg_dtl_ln_left(8),mg_dtl_ln_right(0)}
        WR(D2D3_MBDG_PARAM_2,(((depthp->depthsize_x>>1)-1)) |(((depthp->depthsize_x>>1)-1)<<8) | 
        (((depthp->depthsize_y>>1)-1)<<16) | (((depthp->depthsize_y>>1)-1)<<24)); //set area for ACT

        //0x1d:{(27),mg_y_adapt_en(26),mg_xmm_adapt_en(25),mg_x_adapt_en(24),mg_ytrans_1(20),mg_xtrans_1(16),mg_yk_0(8),mg_xk_0(0)}
        WR_REG_BITS(D2D3_MBDG_PARAM_4,(mbdg->mg_y_adapt_en<<2)|(mbdg->mg_xmm_adapt_en<<1)|mbdg->mg_x_adapt_en,24,27-24); //set mg_y_adapt_en&mg_xmm_adapt_en&mg_x_adapt_en
}

//Configure the LBDG
void d2d3_config_dpg_lbdg(struct d2d3_lbdg_param_s * ldbgp)
{
        //0x00:{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        WR_REG_BITS(D2D3_GLB_CTRL,ldbgp->lbdg_en,4,5-4); //Set lg_en = 1

        //0x10:{(12),db_lpf_bpcoeff(20),lg_lpf_bpcoeff(18),cg_lpf_bpcoeff(16),(10),db_lpf_bypass(8),lg_lpf_bypass(6),lg_kc(0)}
        WR_REG_BITS(D2D3_DPF_LPF_CTRL,ldbgp->lg_kc,0,6-0); //set lg_kc

}

//Configure the LBDG
void d2d3_config_dpg_dbld(int db_factor, int db_f1_ctrl, int db_f2_ctrl)
{
        //0x14:{db_factor(24),db_hf_a(16),db_hf_b(8),db_hf_c(0)}
        if(db_factor>=0)
                WR_REG_BITS(D2D3_DBLD_LPF_HCOEFF,db_factor,24,32-24); //set db_factor

        //0x16:{(24),db_f1_ctrl(16),db_f2_ctrl(8),db_fifo0_sel(4),db_fifo1_sel(0)}
        if(db_f1_ctrl>=0)
                WR_REG_BITS(D2D3_DBLD_PATH_CTRL,db_f1_ctrl,16,24-16);//set db_f1_ctrl
        if(db_f2_ctrl>=0)
                WR_REG_BITS(D2D3_DBLD_PATH_CTRL,db_f2_ctrl,8,16-8);//set db_f2_ctrl
}

// DBR Function:
//input:
//    DBR_Mode = 0:  the 1st output field is left, field interleave (left/right)
//    DBR_Mode = 1:  the 1st output field is right, field interleave (left/right)
//    DBR_Mode = 2:  line interleave (left first): lllll|rrrrr|lllll|...
//    DBR_Mode = 3:  half line interleave (left first): lllllrrrrr|lllllrrrrr|... (both left and right are half size)
//    DBR_Mode = 4:  pixel interleave (left first): lrlrlrlr... (both left and right are half size)
//    DBR_Mode = 5:  left/right output with full size : lllllrrrrr|lllllrrrrr|...

//static void d2d3_enable_dbr(int size_x, int size_y, int scale_x, int scale_y, int DBR_Mode, int canvas_idx, int dbr_d2p_WRap_en,
//                     int dbr_extn_black, int dbr_brdlpf_en, int dbr_lr_merge, sD2D3_SCU18Param sSCU18Param)
void d2d3_enable_dbr(struct d2d3_dev_s *devp, struct d2d3_depth_s *depth)
{
        int dbr_d2p_neg, dbr_d2p_brdwid, dbr_d2p_smode, dbr_d2p_out_mode;
        int dbr_d2p_lomode, dbr_d2p_1dtolr, dbr_ddd_hhalf, dbr_d2p_lar, dbr_d2p_lr_switch;
        int dbr_scu18_rep_en, dbr_ddd_out_mode, dbr_ddd_lomode;
        int szxScaled,szyScaled;
        int scu18_hstep,scu18_vstep;
        int scale_x_act;
        unsigned int dwTemp;
        int dbr_d2p_wrap_en = 1;
        int dbr_extn_black = 0;
        int dbr_brdlpf_en = 0;
        int dbr_lr_merge = 0;
        struct d2d3_param_s *parm = &devp->param;
        struct d2d3_scu18_param_s *scu18 = &devp->scu18;
        //0x00:{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        WR_REG_BITS(D2D3_GLB_CTRL,1,0,1-0); //Set dpg_en = 1
        WR_REG_BITS(D2D3_GLB_CTRL,1,1,2-1); //Set dbr_en = 1

        //0x02:{szx_m1(16),szy_m1(0)}
        WR(D2D3_DBR_OUTPIC_SIZE,((depth->dbr_size_x-1)<<16) | (depth->dbr_size_y-1)); //image size

        //boundary
        if(dbr_d2p_wrap_en)
                dbr_d2p_brdwid = 1;
        else
                dbr_d2p_brdwid = 0;
        dbr_d2p_neg = 0;
        //the 1st output field is left, field interleave (left/right)
        if(0 == parm->dbr_mode) {
                dbr_d2p_smode = 3;    //00: no shift;01:left shift only; 10:right shift only; 11: left+right shift
                dbr_d2p_out_mode = 3;  //00: pixel interleave(lrlr...);
                //01 & ddd_hhalf=1: half line interleave(llllrrrr|llllrrrr)
                //01 & ddd_hhalf=0: line interleave (llll|rrrr|llll|rrrr)
                //10 or 11: all left or all right.
                dbr_d2p_lomode = 0;   //in each line:
                //00 or 01:all left or all right
                //10 : pixel interleave
                //11 : half line interleave
                dbr_d2p_1dtolr = 0;   //1: only when gD2D3Params.reg_d2p_ctrl_x0c.bitVec.d2p_out_mode=0
                dbr_ddd_hhalf = 0;    //1: in each line,half line interleave(llllrrrr|llllrrrr)
                dbr_d2p_lar    = 0;   //indicate the first output type (left or right)i,0 means left.
                dbr_d2p_lr_switch = 1;//switch the output type for each field
                dbr_scu18_rep_en = 0; //repeat each line twice to the d2p
        }//the 1st output field is right, field interleave (left/right)
        else if(1 == parm->dbr_mode) {
                dbr_d2p_smode = 3;
                dbr_d2p_out_mode = 3;
                dbr_d2p_lomode = 0;
                dbr_d2p_1dtolr = 0;
                dbr_ddd_hhalf = 0;
                dbr_d2p_lar   = 1;
                dbr_d2p_lr_switch = 1;
                dbr_scu18_rep_en = 0;
        }// line interleave (left first): lllll|rrrrr|lllll|...
        else if(2 == parm->dbr_mode) {
                dbr_d2p_smode = 3;
                dbr_d2p_out_mode = 1;
                dbr_d2p_lomode = 0;
                dbr_d2p_1dtolr = 0;
                dbr_ddd_hhalf = 0;
                dbr_d2p_lar   = 0;
                dbr_d2p_lr_switch = 0;
                dbr_scu18_rep_en = 0;
        }// half line interleave (left first): lllllrrrrr|lllllrrrrr|... (both left and right are half size)
        else if(3 == parm->dbr_mode) {
                dbr_d2p_smode = 3;
                dbr_d2p_out_mode = 1;
                dbr_d2p_lomode = 3;
                dbr_d2p_1dtolr = 0;
                dbr_ddd_hhalf = 1;
                dbr_d2p_lar   = 0;
                dbr_d2p_lr_switch = 0;
                dbr_scu18_rep_en = 1;
        }// pixel interleave (left first): lrlrlrlr... (both left and right are half size)
        else if(4 == parm->dbr_mode) {
                dbr_d2p_smode = 3;
                dbr_d2p_out_mode = 0;
                dbr_d2p_lomode = 2;
                dbr_d2p_1dtolr = 1;
                dbr_ddd_hhalf = 1;
                dbr_d2p_lar   = 0;
                dbr_d2p_lr_switch = 0;
                dbr_scu18_rep_en = 0;
        }//  left/right output with full size : lllllrrrrr|lllllrrrrr|...
        else if(5 == parm->dbr_mode) {
                dbr_d2p_smode = 3;
                dbr_d2p_out_mode = 1;
                dbr_d2p_lomode = 3;
                dbr_d2p_1dtolr = 0;
                dbr_ddd_hhalf = 0;
                dbr_d2p_lar   = 0;
                dbr_d2p_lr_switch = 0;
                dbr_scu18_rep_en = 1;
        }
        dbr_ddd_out_mode = dbr_d2p_out_mode;
        dbr_ddd_lomode   = dbr_d2p_lomode;

        //0x0c:{d2p_brdwid(24),(22),d2p_lomode(20),d2p_neg(19),(18),d2p_WRap_en(17),d2p_lar(16),d2p_lr_switch(15),d2p_1dtolr(14),
        //      d2p_out_mode(12),d2p_smode(8),d2p_offset(0)}
        dwTemp = (dbr_d2p_smode) | (dbr_d2p_out_mode<<4) | (dbr_d2p_1dtolr<<6) |
                (dbr_d2p_lr_switch<<7) | (dbr_d2p_lar<<8) | (dbr_d2p_wrap_en <<9) |
                (dbr_d2p_neg<<11) | (dbr_d2p_lomode<<12);
        WR_REG_BITS(D2D3_D2P_PARAM_1,dwTemp,8,22-8); //set DBR params

        //0x2c:{(8),brdlpf_en(7),extn_black(6),(5),ddd_hhalf(4),ddd_out_mode(2),ddd_lomode(0)}
        dwTemp = dbr_ddd_lomode | (dbr_ddd_out_mode<<2) | (dbr_ddd_hhalf<<4) | (dbr_extn_black<<6) | (dbr_brdlpf_en <<7);
        WR(D2D3_DBR_DDD_CTRL,dwTemp);

        //0x2f:{(9),lr_merge(8),lrd_ff0_sel(6),lrd_ff1_sel(4),lrd_lout_sel(2),lrd_rout_sel(0)}
        WR_REG_BITS(D2D3_DBR_LRDMX_CTRL,dbr_lr_merge,8,1); //set lr_merge

        //0x06:{scu18_iniph_v(24),scu18_iniph_h(16),(12),scd81_predrop_en(11),cg_csc_sel(9),scu18_rep_en(8),scu18_factor(4),scd81_factor(0)}
        if(scu18->dbr_scu18_step_en == 0)
        {
                scale_x_act = (dbr_ddd_hhalf) ? depth->dbr_scale_x-1 : depth->dbr_scale_x;
                WR_REG_BITS(D2D3_SCALER_CTRL,scale_x_act|(depth->dbr_scale_y<<2),4,8-4);  //Set SCD18 factor:
        }
        else
        {
                scu18_hstep = (dbr_ddd_hhalf) ? 
                ((scu18->dbr_scu18_isize_x<<9)/depth->dbr_size_x) : ((scu18->dbr_scu18_isize_x<<8)/depth->dbr_size_x);
                scu18_vstep = ((scu18->dbr_scu18_isize_y<<8)/depth->dbr_size_y);
                WR(D2D3_SCU18_INPIC_SIZE,((scu18->dbr_scu18_isize_y)<<16) |(scu18->dbr_scu18_isize_x));
                //0x0f:D2D3_SCU18_STEP: {scu18_step_en(16),scu18_hphs_step(8),scu18_vphs_step(0)}
                WR(D2D3_SCU18_STEP,((1<<16)|(scu18_hstep<<8)|scu18_vstep));
        }
        WR_REG_BITS(D2D3_SCALER_CTRL,dbr_scu18_rep_en,8,9-8);  //Set SCD18 factor:
        WR_REG_BITS(D2D3_SCALER_CTRL,scu18->dbr_scu18_iniph_h|(scu18->dbr_scu18_iniph_v<<8),16,32-16); //set phase

        //=== DRMIFf ===
        if(!scu18->dbr_scu18_step_en)
        {
                szxScaled = depth->depthsize_x;
                szyScaled = depth->depthsize_y;
        }
        else
        {  
                szxScaled = scu18->dbr_scu18_isize_x;
                szyScaled = scu18->dbr_scu18_isize_y;
        }
        //0x29:{drmif_end_x(16),drmif_start_x(0)}
        dwTemp = (0x0) | ((szxScaled-1)<<16); //drmif_start_x=0x0; drmif_end_x=szxScaled-1;
        WR(D2D3_DRMIF_HPOS, (0x0)|((szxScaled-1)<<16));

        //0x2a:{drmif_end_y(16),drmif_start_y(0)}
        WR(D2D3_DRMIF_VPOS,(0x0)|((szyScaled-1)<<16));

        //0x27:{drmif_vsize_m1(16),drmif_hsize_m1(0)}
        //WR(D2D3_DWMIF_SIZE,(szxScaled-1)|((szyScaled-1)<<16));

        //0x28:{(18),reg_dr_y_rev(17),reg_dr_x_rev(16),dr_clr_fifo_error(15),dr_little_endian(14),dr_pic_struct(12),
        //       dr_urgent(11),dr_burst_size(9),dr_req_en(8),dr_canvas_index(0)}
        //canvas reversed control h:16 v:17
        WR_REG_BITS(D2D3_DRMIF_CTRL,d2d3_canvas_ids[1],0,8-0);
        WR_REG_BITS(D2D3_DRMIF_CTRL,1,8,9-8); //dr_req_en=1
}


//Initial D2D3 Register
void init_d2d3_reg(struct d2d3_param_s *parm)
{
        unsigned int size_x, size_y;
        unsigned short scale_x,scale_y;
        int szxScaled,szyScaled;
        unsigned int dwTemp;

        int dbr_d2p_WRap_en, dbr_d2p_neg, dbr_d2p_brdwid, dbr_d2p_smode, dbr_d2p_out_mode;
        int dbr_d2p_lomode, dbr_d2p_1dtolr, dbr_ddd_hhalf, dbr_d2p_lar, dbr_d2p_lr_switch;
        int dbr_scu18_rep_en, dbr_ddd_out_mode, dbr_ddd_lomode;

        //Default size: 720x480
        //size_x  = 720; size_y  = 480;
        //Default scale: 1/8
        //scale_x = 3;   scale_y = 3;

        //Default size: 720x480
        size_x  = parm->width; 
        size_y  = parm->height;
        //Default scale: 1/8
        scale_x  = parm->scale_x; 
        scale_y  = parm->scale_y;

        //0x00: D2D3_GLB_CTRL (GLOBAL)
        //{rd_lock_en(31),sw_rst_nobuf(30),(28),clk_ctrl(16),(12),lo_chroma_sign(11),ro_chroma_sign(10),
        //vi0_chroma_sign(9),vi1_chroma_sign(8),(5),lg_en(4),mg_en(3),cg_en(2),dbr_en(1),dpg_en(0)}
        dwTemp = (1<<31)|(1<<11)|(1<<10)|(1<<8);  //rd_lock_en=1; lo_chroma_sign=1; ro_chroma_sign=1; vi0_chroma_sign =0; vi1_chroma_sign=1
        WR(D2D3_GLB_CTRL,dwTemp);

        //========== DPG ===============================
        //0x01: D2D3_DPG_INPIC_SIZE
        //{szx_m1(16),szy_m1(0)}
        dwTemp = ((size_x-1)<<16) | (size_y-1);
        WR(D2D3_DPG_INPIC_SIZE,dwTemp);

        //0x06: D2D3_SCALER_CTRL (DPG & DBR)
        //{scu18_iniph_v(24),scu18_iniph_h(16),(12),scd81_predrop_en(11),cg_csc_sel(9),scu18_rep_en(8),scu18_factor(4),scd81_factor(0)}
        dwTemp = (scale_x | (scale_y<<2)) |  ((scale_x | (scale_y<<2))<<4); //scu18_iniph_v=scu18_iniph_h=0; cg_csc_sel=0; scu18_rep_en=0;
        WR(D2D3_SCALER_CTRL,dwTemp);

        //0x05: D2D3_PRE_SCD_H (DPG)
        //{reg_scd81_hphs_step(16),reg_scd81_hphs_ini(0)}
        dwTemp = 0 | (0x4000<<16); //reg_scd81_hphs_ini=0; reg_scd81_hphs_step = 0x4000;
        WR(D2D3_PRE_SCD_H,dwTemp);

        //0x0B: D2D3_PRE_SCD_V (DPG)
        //{reg_scd81_vphs_step(16),reg_scd81_vphs_ini(0)}
        dwTemp = 0 | (0x4000<<16); //reg_scd81_vphs_ini=0; reg_scd81_vphs_step = 0x4000;
        WR(D2D3_PRE_SCD_V,dwTemp);

        //scaled size
        szxScaled = (size_x+(1<<scale_x)-1)>>scale_x;
        szyScaled = (size_y+(1<<scale_y)-1)>>scale_y;
        //check the scaled size
        //if(szxScaled>256 || (szyScaled>256) || (scale_x == 0))
        //  printk("Error: scd81_factor for down-scaling......\n");

        //0x03: D2D3_DGEN_WIN_HOR (DPG)
        //{dg_win_x_start(16),dg_win_x_end(0)}
        dwTemp = (0<<16) | (szxScaled-1);
        WR(D2D3_DGEN_WIN_HOR,dwTemp);

        //0x04: D2D3_DGEN_WIN_VER (DPG)
        //{dg_win_y_start(16),dg_win_y_end(0)}
        dwTemp = (3<<16) | (szyScaled-1);
        WR(D2D3_DGEN_WIN_VER,dwTemp);

        //=== CBDG ===
        //0x07: D2D3_CG_THRESHOLD_1
        //{cg_rpg_dth(24),cg_rpg_uth(16),cg_lum_dth(8),cg_lum_uth(0)}
        dwTemp = (0xff) | (0xc8<<8) | (0x1e<<16) | (0x0<<24); //cg_lum_uth=0xff; cg_lum_dth=0xc8; cg_rpg_uth=0x1e; cg_rpg_dth=0x0;
        WR(D2D3_CG_THRESHOLD_1,dwTemp);

        //0x08: D2D3_CG_THRESHOLD_2
        //{cg_rpb_dth(24),cg_rpb_uth(16),cg_bpg_dth(8),cg_bpg_uth(0)}
        dwTemp = (0x1e) | (0x0<<8) | (0xff<<16) | (0x0<<24); //cg_bpg_uth=0x1e; cg_bpg_dth=0x0; cg_rpb_uth=0xff; cg_rpb_dth=0x0;
        WR(D2D3_CG_THRESHOLD_2,dwTemp);

        //0x09: D2D3_CG_PARAM_1
        //{cg_vp_rel_k(24),cg_vp_y_thr(16),cg_meet_dval(8),cg_unmt_dval(0)}
        dwTemp = ((-64)&0xff) | ((63&0xff)<<8) | (0x10<<16) | (0x4<<24); //cg_unmt_dval=-64; cg_meet_dval=63; cg_vp_y_thr=0x10; cg_vp_rel_k=0x4;
        WR(D2D3_CG_PARAM_1,dwTemp);

        //0x0a: D2D3_CG_PARAM_2
        //{cg_vpos_thr(16),(8),cg_vpos_en(7),cg_vpos_adpt_en(6),cg_lpf_bypass(4),cg_vp_rel_s(0)}
        dwTemp = (3) | (szyScaled<<16);  //cg_vp_rel_s=3; cg_lpf_bypass=0; cg_vpos_adpt_en=0; cg_vpos_en=0; cg_vpos_thr=szyScaled; //TBD
        WR(D2D3_CG_PARAM_2,dwTemp);

        //=== MBDG ===
        //0x18: D2D3_MBDG_CTRL
        //{(18),mg_vp_en(17),mg_sw_en(16),mg_owin_fill(8),mg_iir_en(7),mg_iir(0)}
        dwTemp = (0x23)|(1<<7); //mg_iir=0x23; mg_iir_en=1; mg_owin_fill=0; mg_vp_en=0; mg_sw_en=0;
        WR(D2D3_MBDG_CTRL,dwTemp);

        //0x19: D2D3_MBDG_PARAM_0
        //{mg_dtl_pxl_left(28),mg_dtl_pxl_right(24),mg_cx_sw(16),mg_ux_sw(8),mg_dx_sw(0)}
        //mg_dx_sw=0xc0; mg_ux_sw=0xc0; mg_cx_sw=(szxScaled/2); mg_dtl_pxl_right=11; mg_dtl_pxl_left=11;
        dwTemp = ((0xc0)&0xff) | ((0xc0&0xff)<<8) | ((szxScaled/2)<<16) | (11<<24) | ((11)<<28);
        WR(D2D3_MBDG_PARAM_0,dwTemp);

        //0x1a: D2D3_MBDG_PARAM_1
        //{mg_dtl_pxl_up(28),mg_dtl_pxl_dn(24),mg_cy_sw(16),mg_uy_sw(8),mg_dy_sw(0)}
        //mg_dy_sw=0xc0; mg_uy_sw=0xc0; mg_cy_sw=(szxScaled/2); mg_dtl_pxl_dn=11; mg_dtl_pxl_up=11;
        dwTemp = ((0xc0)&0xff) | ((0xc0&0xff)<<8) | ((szyScaled/2)<<16) | (11<<24) | ((11)<<28);
        WR(D2D3_MBDG_PARAM_1,dwTemp);

        //0x1b: D2D3_MBDG_PARAM_2
        //{mg_dtl_ln_up(24),mg_dtl_ln_dn(16),mg_dtl_ln_left(8),mg_dtl_ln_right(0)}
        dwTemp = (((szxScaled/2)-1)) | (((szxScaled/2)-1)<<8) | (((szyScaled/2)-1)<<16) | (((szyScaled/2)-1)<<24);
        WR(D2D3_MBDG_PARAM_2,dwTemp);

        //0x1c: D2D3_MBDG_PARAM_3
        //{mg_y_max(24),mg_y_min(16),mg_x_max(8),mg_x_min(0)}
        dwTemp = ((0x4)&0xff) | ((0xb0&0xff)<<8) | ((0x10)<<16) | ((0xb0&0xff)<<24); //mg_x_min=4; mg_x_max=0xb0; mg_y_min=0x10; mg_y_max=0xb0;
        WR(D2D3_MBDG_PARAM_3,dwTemp);

        //0x1d: D2D3_MBDG_PARAM_4
        //{(27),mg_y_adapt_en(26),mg_xmm_adapt_en(25),mg_x_adapt_en(24),mg_ytrans_1(20),mg_xtrans_1(16),mg_yk_0(8),mg_xk_0(0)}
        //mg_xk_0=0xc; mg_yk_0=0xc; mg_xtrans_1=0xc; mg_ytrans_1=0xc; mg_x_adapt_en=0; mg_xmm_adapt_en=1; mg_y_adapt_en=0;
        dwTemp = (0x0) | (0x0<<8) | ((0xc&0xff)<<16) | ((0xc&0xff)<<20) | (0<<24) | (1<<25) | (0<<26);
        WR(D2D3_MBDG_PARAM_4,dwTemp);

        //0x1e: D2D3_MBDG_PARAM_5
        //{mg_ysu3(24),mg_ysu2(16),mg_ysu1(8),mg_ysu0(0)}
        dwTemp = (0x8) | (0x18<<8) | (0x38<<16) | (0x58<<24); //mg_ysu0=0x8; mg_ysu1=0x18; mg_ysu2=0x38; mg_ysu3=0x58;
        WR(D2D3_MBDG_PARAM_5,dwTemp);

        //0x1f: D2D3_MBDG_PARAM_6
        //{mg_xsu3(24),mg_xsu2(16),mg_xsu1(8),mg_xsu0(0)}
        dwTemp = (0x8) | (0x18<<8) | (0x38<<16) | (0x58<<24); //mg_xsu0=0x8; mg_xsu1=0x18; mg_xsu2=0x38; mg_xsu3=0x58;
        WR(D2D3_MBDG_PARAM_6,dwTemp);

        //0x20: D2D3_MBDG_PARAM_7
        //{(16),mg_xsu4(8),mg_ysu4(0)}
        dwTemp = (0x80) | (0x80<<8); //mg_ysu4=0x80; mg_xsu4=0x80;
        WR(D2D3_MBDG_PARAM_7,dwTemp);

        //=== LBDG ===
        //0x10: D2D3_DPF_LPF_CTRL
        //{(12),db_lpf_bpcoeff(20),lg_lpf_bpcoeff(18),cg_lpf_bpcoeff(16),(10),db_lpf_bypass(8),lg_lpf_bypass(6),lg_kc(0)}
        dwTemp = (8); //lg_kc=8;
        WR(D2D3_DPF_LPF_CTRL,dwTemp);

        //=== DBLD ===
        //0x11: D2D3_DBLD_CG_PARAM
        //{db_g2_cg(24),db_o2_cg(16),db_g1_cg(8),db_o1_cg(0)}
        dwTemp = (0x0) | (0x10<<8) | (0x0<<16) | (0x10<<24); //db_o1_cg=0x0; db_g1_cg=0x10; db_o2_cg=0x0; db_g2_cg=0x10;
        WR(D2D3_DBLD_CG_PARAM,dwTemp);

        //0x12: D2D3_DBLD_MG_PARAM
        //{db_g2_mg(24),db_o2_mg(16),db_g1_mg(8),db_o1_mg(0)}
        dwTemp = (0x0) | (0x8<<8) | (0x0<<16) | (0x8<<24); //db_o1_mg=0x0; db_g1_mg=0x8; db_o2_mg=0x0; db_g2_mg=0x8;
        WR(D2D3_DBLD_MG_PARAM,dwTemp);

        //0x13: D2D3_DBLD_LG_PARAM
        //{db_g2_lg(24),db_o2_lg(16),db_g1_lg(8),db_o1_lg(0)}
        dwTemp = (0x0) | (0x10<<8) | (0x0<<16) | (0x10<<24); //db_o1_lg=0x0; db_g1_lg=0x10; db_o2_lg=0x0; db_g2_lg=0x10;
        WR(D2D3_DBLD_LG_PARAM,dwTemp);

        //0x14: D2D3_DBLD_LPF_HCOEFF
        //{db_factor(24),db_hf_a(16),db_hf_b(8),db_hf_c(0)}
        dwTemp = (0x10) | (0x0c<<8) | (0x10<<16) | (0x0<<24); //db_hf_c=0x10; db_hf_b=0x0c; db_hf_a=0x0c; db_factor=0x0;
        WR(D2D3_DBLD_LPF_HCOEFF,dwTemp);

        //0x15: D2D3_DBLD_LPF_VCOEFF
        //{db_owin_fill(24),db_vf_a(16),db_vf_b(8),db_vf_c(0)}
        dwTemp = (0x10) | (0x0c<<8) | (0x10<<16) | (0x0<<24); //db_vf_c=0x10; db_vf_b=0x0c; db_vf_a=0x10; db_owin_fill=0x0;
        WR(D2D3_DBLD_LPF_VCOEFF,dwTemp);

        //0x16: D2D3_DBLD_PATH_CTRL
        //{(24),db_f1_ctrl(16),db_f2_ctrl(8),db_fifo0_sel(4),db_fifo1_sel(0)}
        dwTemp = (0x3) | (0x1<<4) | (0x40<<8) | (0x40<<16); //db_fifo1_sel=0x3; db_fifo0_sel=0x1; db_f2_ctrl=0x40; db_f1_ctrl=0x40;
        WR(D2D3_DBLD_PATH_CTRL,dwTemp);

        //=== dwmif ===
        //0x24: D2D3_DWMIF_CTRL
        //{x_rev(17),y_rev(16),dw_done_clr(15),dw_little_endian(14),dw_pic_struct(12),dw_urgent(11),
        //dw_clr_WRrsp(10),dw_canvas_WR(9),dw_req_en(8),dw_canvas_index(0)}
        dwTemp = (0x0);
        WR(D2D3_DWMIF_CTRL,dwTemp);

        //0x25: D2D3_DWMIF_HPOS
        //{dwmif_end_x(16),dwmif_start_x(0)}
        dwTemp = (0x0) | ((szxScaled-1)<<16); //dwmif_start_x=0x0; dwmif_end_x=szxScaled-1;
        WR(D2D3_DWMIF_HPOS,dwTemp);

        //0x26: D2D3_DWMIF_VPOS
        //{dwmif_end_y(16),dwmif_start_y(0)}
        dwTemp = (0x0) | ((szyScaled-1)<<16); //dwmif_start_y=0x0; dwmif_end_y=szyScaled-1;
        WR(D2D3_DWMIF_VPOS,dwTemp);

        //0x27: D2D3_DWMIF_SIZE
        //{dwmif_vsize_m1(16),dwmif_hsize_m1(0)}
        dwTemp = (szxScaled-1) | ((szyScaled-1)<<16); //dwmif_hsize_m1=szxScaled-1; dwmif_vsize_m1=szyScaled-1;
        WR(D2D3_DWMIF_SIZE,dwTemp);


        //========== DBR ==================
        //0x02: D2D3_DBR_OUTPIC_SIZE
        //{szx_m1(16),szy_m1(0)} (DBR)
        dwTemp = ((size_x-1)<<16) | (size_y-1);
        WR(D2D3_DBR_OUTPIC_SIZE,dwTemp);

        dbr_d2p_WRap_en = 0;
        dbr_d2p_neg = 0;
        dbr_d2p_brdwid = 0;
        //if((dbr_d2p_WRap_en == 1) && (dbr_d2p_brdwid == 0))
        //{
        //    printf("Warning: Force the d2p_brdwid=1 because the d2p_WRap is enabled\n");
        //    dbr_d2p_brdwid = 1; //should be > 0 when WRap_en = 1
        //}

        //line interleaved
        dbr_d2p_smode   = 3;    //00: no shift;01:left shift only; 10:right shift only; 11: left+right shift
        dbr_d2p_out_mode = 1;   //00: pixel interleave(lrlr...);
        //01 & ddd_hhalf=1: half line interleave(llllrrrr|llllrrrr)
        //01 & ddd_hhalf=0: line interleave (llll|rrrr|llll|rrrr)
        //10 or 11: all left or all right.
        dbr_d2p_lomode = 0;     //in each line:
        //00 or 01:all left or all right
        //10 : pixel interleave
        //11 : half line interleave
        dbr_d2p_1dtolr = 0;     //1: only when gD2D3Params.reg_d2p_ctrl_x0c.bitVec.d2p_out_mode=0
        dbr_ddd_hhalf = 0;      //1: in each line,half line interleave(llllrrrr|llllrrrr)
        dbr_d2p_lar    = 0;     //indicate the first output type (left or right),0 means left.
        dbr_d2p_lr_switch = 0;  //switch the output type for each field when d2p_out_mode =2 or 3
        dbr_scu18_rep_en = 0;   //repeat each line twice to the d2p
        dbr_ddd_out_mode = dbr_d2p_out_mode;
        dbr_ddd_lomode   = dbr_d2p_lomode;

        //0x0c: D2D3_D2P_PARAM_1
        //{d2p_brdwid(24),(22),d2p_lomode(20),d2p_neg(19),(18),d2p_WRap_en(17),d2p_lar(16),d2p_lr_switch(15),d2p_1dtolr(14),d2p_out_mode(12),(2),d2p_smode(8),d2p_offset(0)}
        dwTemp = ((64)&0xff) | (dbr_d2p_smode<<8) | (dbr_d2p_out_mode<<12) | (dbr_d2p_1dtolr<<14) |  //d2p_offset=64
                (dbr_d2p_lr_switch<<15) | (dbr_d2p_lar<<16) | (dbr_d2p_WRap_en <<17) |
                (dbr_d2p_neg<<19) | (dbr_d2p_lomode<<20) | (dbr_d2p_brdwid<<24);
        WR(D2D3_D2P_PARAM_1,dwTemp);

        //0x2c: D2D3_DBR_DDD_CTRL
        //{(8),brdlpf_en(7),extn_black(6),(5),ddd_hhalf(4),ddd_out_mode(2),ddd_lomode(0)}
        dwTemp = dbr_ddd_lomode | (dbr_ddd_out_mode<<2) | (dbr_ddd_hhalf<<4); //brdlpf_en=0; extn_black=0;
        WR(D2D3_DBR_DDD_CTRL,dwTemp);

        //0x0d: D2D3_D2P_PARAM_2
        //{pg0(24),pg1(16),pt(8),(7),plimit(0)}
        dwTemp = (0x3f) | (0x40<<8) | (0x4<<16) | (0x5<<24); //plimit=0x3f; pt=0x40; pg1=0x4; pg0=0x5;
        WR(D2D3_D2P_PARAM_2,dwTemp);

        //0x0e: D2D3_D2P_PARAM_3
        //{ng0(24),ng1(16),nt(8),(7),nlimit(0)}
        dwTemp = (0x3f) | (0x40<<8) | (0x4<<16) | (0x5<<24); //nlimit=0x3f; nt=0x40; ng1=0x4; ng0=0x5;
        WR(D2D3_D2P_PARAM_3,dwTemp);

        //0x2f: D2D3_DBR_LRDMX_CTRL
        //{(9),lr_merge(8),lrd_ff0_sel(6),lrd_ff1_sel(4),lrd_lout_sel(2),lrd_rout_sel(0)}
        dwTemp = (0x0) | (0x0<<2) | (0x0<<4) | (0x0<<6) | (0x0<<8); //lrd_rout_sel=0x0; lrd_lout_sel=0x0; lrd_ff1_sel=0x0; lrd_ff0_sel=0x0; lr_merge=0x0;
        WR(D2D3_DBR_LRDMX_CTRL,dwTemp);

        //=== drmif ===
        //0x28: D2D3_DRMIF_CTRL
        //{(18),reg_dr_y_rev(17),reg_dr_x_rev(16),dr_clr_fifo_error(15),dr_little_endian(14),dr_pic_struct(12),dr_urgent(11),dr_burst_size(9),dr_req_en(8),dr_canvas_index(0)}
        dwTemp = (0x0);
        WR(D2D3_DRMIF_CTRL,dwTemp);

        //0x29: D2D3_DRMIF_HPOS
        //{drmif_end_x(16),drmif_start_x(0)}
        dwTemp = (0x0) | ((szxScaled-1)<<16); //drmif_start_x=0x0; drmif_end_x=szxScaled-1;
        WR(D2D3_DRMIF_HPOS,dwTemp);

        //0x2a: D2D3_DRMIF_VPOS
        //{drmif_end_y(16),drmif_start_y(0)}
        dwTemp = (0x0) | ((szyScaled-1)<<16); //drmif_start_y=0x0; drmif_end_y=szyScaled-1;
        WR(D2D3_DRMIF_VPOS,dwTemp);

        //0x27: D2D3_DWMIF_SIZE
        //{drmif_vsize_m1(16),drmif_hsize_m1(0)}
        dwTemp = (szxScaled-1) | ((szyScaled-1)<<16); //drmif_hsize_m1=szxScaled-1; drmif_vsize_m1=szyScaled-1;
        WR(D2D3_DWMIF_SIZE,dwTemp);
}





//*********************************************************************
//      function to enable the D2D3 (DPG and DBR)
//*********************************************************************
//static void C_D2D3_Config(int dpg_size_x, int dpg_size_y, int dpg_scale_x, int dpg_scale_y, int d2d3_flow_mode, int DBR_Mode, int dbr_size_x, int dbr_size_y)
void d2d3_config(struct d2d3_dev_s *devp)
        //input:
        // d2d3_flow_mode:
        //   1: D2D3 DPG: NRW (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> DBR (D2D3) -> VPP (Scaler) -> TVENC
        //   2: D2D3 DPG: NRW (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> VPP (Scaler) -> DBR (D2D3) -> VPP (post process)
        //   3: D2D3 DPG: VDR (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> DBR (D2D3) -> VPP -> TVENC
        //   4: D2D3 DPG: VDR (DEINT) -> DPG (D2D3) -> Memory ; D2D3 DBR: VDR (DEINT) -> VPP (Scaler) -> DBR (D2D3) -> VPP (post process)
        // Note: input param {dbr_size_x,dbr_size_y} are valid only when d2d3_flow_mode=2 or d2d3_flow_mode=4
        //
        //  DBR_Mode:
        //   0: the 1st output field is left, field interleave (left/right)
        //   1: the 1st output field is right, field interleave (left/right)
        //   2: line interleave (left first): lllll|rrrrr|lllll|...
        //   3: half line interleave (left first): lllllrrrrr|lllllrrrrr|... (both left and right are half size)
        //   4: pixel interleave (left first): lrlrlrlr... (both left and right are half size)
        //   5: left/right output with full size : lllllrrrrr|lllllrrrrr|...
{
        unsigned short vpp2dbr, nrw2dpg;
        struct d2d3_param_s *parm;
        struct d2d3_depth_s depth;
        
        /*
        unsigned int dpg_size_x = parm->src_size_x;
        unsigned int dpg_size_y = parm->src_size_y;
        unsigned int dbr_size_x = parm->src_size_x;
        unsigned int dbr_size_y = parm->src_size_y;
        unsigned short dpg_scale_x = parm->scale_x;
        unsigned short dpg_scale_y = parm->scale_y;
        
        unsigned short nrw2dpg, vpp2dbr;
        int dbr_scale_x, dbr_scale_y;
        */
        parm = &devp->param; 
        depth.dpg_size_x = parm->width;
        depth.dpg_size_y = parm->height;
        parm->scale_x = D2D3_SCU18_MIN_SCALE_FACTOR;//no down scale
        parm->scale_y = D2D3_SCU18_MIN_SCALE_FACTOR;//no down scale
        //the width should be no bigger than 2048
        while(parm->width > D2D3_HORIZONTAL_PIXLE_MAX){
                parm->width>>=1;
                parm->scale_x++;
        }
        if(parm->scale_x > D2D3_SCU18_MAX_SCALE_FACTOR){
                printk("[d2d3..] %s width %u over the max input h size 2048.\n",__func__,parm->width);
                return;
        }
                
        depth.dpg_scale_x = parm->scale_x;        
        depth.dpg_scale_y = parm->scale_y;
        
        nrw2dpg =  (parm->flow_mode == 1) ||  (parm->flow_mode == 2);
        vpp2dbr =  (parm->flow_mode == 2) ||  (parm->flow_mode == 4);

        if(nrw2dpg) {
                depth.dbr_scale_x = depth.dpg_scale_x;
                depth.dbr_scale_y = depth.dpg_scale_y + 1;  //because the format of NRW is interlaced
                depth.dbr_size_x = depth.dpg_size_x;
                depth.dbr_size_y = depth.dpg_size_y<<1;
        }
        else if (!vpp2dbr) {
                depth.dbr_scale_x = depth.dpg_scale_x;
                depth.dbr_scale_y = depth.dpg_scale_y;
                depth.dbr_size_x  = depth.dpg_size_x;
                depth.dbr_size_y  = depth.dpg_size_y<<1;//why?
        }

        //depth size
        depth.depthsize_x  = (depth.dpg_size_x+(1<<depth.dpg_scale_x)-1)>>depth.dpg_scale_x;
        depth.depthsize_y  = (depth.dpg_size_y+(1<<depth.dpg_scale_y)-1)>>depth.dpg_scale_y;

        //enable clk_d2d3 & clk_d2d3_reg
        WR_REG_BITS (D2D3_INTF_CTRL0, 3, 22, 2); //D2D3_INTF_CTRL0[23:22]
        //DPG: v0 size_x for eol
        WR_REG_BITS(D2D3_INTF_LENGTH,depth.dpg_size_x -1,0,13); //[12:0]:v0_line_lengthm1;  [28:16]: v1_line_lengthm1
        //DBR: depth read hold lines
        WR_REG_BITS(D2D3_INTF_CTRL0,6,6,7);      //d2d3_intf_ctrl0[12:6] : hold lines

        //Disable D2D3 path at beginnig
        enable_d2d3dbr_path(false,parm->flow_mode);

        //set the default register
        init_d2d3_reg(parm);

        /*configure D2D3 DPG*/
        //enable CBDG MBDG LBDG
        devp->cbdg.cbdg_en = 1;
        //MBDG
        devp->mbdg.mbdg_en = 1;
        devp->mbdg.mg_xmm_adapt_en = 1;
        //LBDG
        devp->lbdg.lbdg_en = 1;
        /*d2d3_enable_dbg(dpg_size_x,     //size_x
          dpg_size_y,     //size_y
          dpg_scale_x,    //scale_x: 1/8
          dpg_scale_y,    //scale_y: 1/8
          D2D3_CANVAS_BASE_IDX,
          gCBDGParam,
          gMBDGParam,
          gLBDGParam);*/

        d2d3_enable_dbg(devp,&depth);   

        /*configure D2D3 DBR SCU18*/
        if(vpp2dbr) {  //image1 input from vpp scale
                devp->scu18.dbr_scu18_step_en = 1;
                devp->scu18.dbr_scu18_isize_x = depth.depthsize_x;
                devp->scu18.dbr_scu18_isize_y = depth.depthsize_y;
                devp->scu18.dbr_scu18_iniph_h = 0;
                devp->scu18.dbr_scu18_iniph_v = 0;
        }
        else {
                devp->scu18.dbr_scu18_step_en = 0;
        }

        //d2d3_enable_dbr(dbr_size_x,dbr_size_y,dbr_scale_x,dbr_scale_y,2,D2D3_CANVAS_BASE_IDX,0,0,0,1,gSCU18Param); //set lr_merge to 1
        d2d3_enable_dbr(devp,&depth);
        //WR_REG_BITS(D2D3_GLB_CTRL,0,1,2-1); //Set dbr_en = 0


        devp->scu18.dbr_scu18_step_en = 0;
        //d2d3_enable_dbr(size_x,size_y*2,scale_x,scale_y,DBR_Mode,D2D3_CANVAS_BASE_IDX,0,0,0,1,gSCU18Param); //always set merge=1
        d2d3_enable_dbr(devp,&depth);
        //WR_REG_BITS(D2D3_GLB_CTRL,0,1,2-1); //Set dbr_en = 0

        //clear  D2D3_INT
        //0x24:{x_rev(17),y_rev(16),dw_done_clr(15),dw_little_endian(14),dw_pic_struct(12),dw_urgent(11),
        //dw_clr_WRrsp(10),dw_canvas_WR(9),dw_req_en(8),dw_canvas_index(0)}
        WR_REG_BITS(D2D3_DWMIF_CTRL,1,15,16-15); //dw_done_clr = 1;
        WR_REG_BITS(D2D3_DWMIF_CTRL,0,15,16-15); //dw_done_clr = 0;

        if(nrw2dpg) { //setup d2d3_dpg interrupt.use the di replace
                printk("setup d2d3 interrupt!\n");
                // data32 = (*P_SYS_CPU_0_IRQ_IN4_INTR_STAT_CLR);
                // (*P_SYS_CPU_0_IRQ_IN4_INTR_MASK) |= (1 << INT_D2D3); //INT_D2D3=5
        }

        //enable the D2D3 path
        enable_d2d3dbr_path(true,parm->flow_mode);
}
/*
*canvas reverse work with di&vdin 
*/
inline void d2d3_canvas_reverse(bool reverse)
{        
        if(reverse)
                WR_REG_BITS(D2D3_DRMIF_CTRL,3,16,2); //dr_req_en=1
        else//normal
                WR_REG_BITS(D2D3_DRMIF_CTRL,0,16,2);
}
inline void d2d3_hw_disable()
{       //disable d2d3 clock,disable d2d3 reg clock ?
        WR_REG_BITS (D2D3_INTF_CTRL0, 0, 22, 1); 
}
