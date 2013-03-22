/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: d2d3_drv.h
*  Description: IO function, structure, enum, used in d2d3 sub-module processing
*******************************************************************/

#ifndef D2D3_H_
#define D2D3_H_

#define D2D3_CANVAS_MAX_WIDTH           1920
#define D2D3_CANVAS_MAX_HEIGH           1080

#define D2D3_CANVAS_MAX_CNT             2
#define D2D3_DPG_CANVAS_INDEX         0
#define D2D3_DPR_CANVAS_INDEX         1

#define D2D3_HORIZONTAL_PIXLE_MAX               256

#define D2D3_SCU18_MIN_SCALE_FACTOR             0
#define D2D3_SCU18_MAX_SCALE_FACTOR            3

#define WR(reg,val)                                             WRITE_CBUS_REG(reg,val)
#define WR_REG_BITS(reg,val,start,len)            WRITE_CBUS_REG_BITS(reg,val,start,len)

typedef struct d2d3_cbdg_param_s {
    int cbdg_en;
    int cg_vpos_adpt_en;
    int cg_vpos_en;
    int cg_vp_y_thr;
    unsigned int yscaled;
} d2d3_cbdg_param_t;

typedef struct d2d3_mbdg_param_s{
    int mbdg_en;
    int mg_vp_en;
    int mg_y_adapt_en;
    int mg_xmm_adapt_en;
    int mg_x_adapt_en;
    int mg_sw_en;
    int mg_iir_en;
    int mg_iir;
} d2d3_mbdg_param_t;

typedef struct d2d3_lbdg_param_s{
    int lbdg_en;
    int lg_kc;
}d2d3_lbdg_param_t;

typedef struct d2d3_scu18_param_s{
    int dbr_scu18_step_en;
    int dbr_scu18_isize_x;
    int dbr_scu18_isize_y;
    int dbr_scu18_iniph_h;
    int dbr_scu18_iniph_v;
}d2d3_scu18_param_t;
typedef struct d2d3_param_s{
        unsigned int        width;
        unsigned int        height;
        unsigned int        reverse_flag;
        unsigned short     scale_x;        
        unsigned short     scale_y;
        unsigned short    flow_mode;
        unsigned short    dbr_mode;   
}d2d3_param_t;
typedef struct d2d3_depth_s{
        unsigned int        dpg_size_x;    
        unsigned int        dpg_size_y;
        unsigned short    dpg_scale_x;    
        unsigned short    dpg_scale_y;
        unsigned int        depthsize_x;
        unsigned int        depthsize_y;
        unsigned int        dbr_size_x;
        unsigned int        dbr_size_y;
        unsigned short    dbr_scale_x;    
        unsigned short    dbr_scale_y;
}d2d3_depth_t;

/* d2d3 device structure */
typedef struct d2d3_dev_s {    
        int                        index;
        dev_t                   devt;    
        struct cdev          cdev; 
        struct device        *dev;   
        char                    irq_name[12]; 
        struct mutex        mutex;    
        /* d2d3 memory */    
        unsigned int        mem_start;    
        unsigned int        mem_size;    
        unsigned int        *canvas_ids;   
        unsigned int        canvas_h;    
        unsigned int        canvas_w;   
        unsigned int        canvas_max_size; 
        unsigned int        canvas_max_num;     
        unsigned int        canvas_idx;
        /*buffer management*/
        unsigned int       dpg_addr;
        unsigned int       dpr_addr;
        /*d2d3 parameters*/
        struct d2d3_param_s param;
        struct d2d3_cbdg_param_s cbdg;
        struct d2d3_mbdg_param_s mbdg;
        struct d2d3_lbdg_param_s lbdg;
        struct d2d3_scu18_param_s scu18;
} d2d3_dev_t;

//function to enable the D2D3 (DPG and DBR)
extern void d2d3_config(struct d2d3_dev_s *devp);
extern void init_d2d3(struct d2d3_param_s *parm);
extern void d2d3_enable_dbg(struct d2d3_dev_s *devp,struct d2d3_depth_s *depthp);
extern void d2d3_enable_dbr(struct d2d3_dev_s *devp,struct d2d3_depth_s *depth);
extern void d2d3_config_dpg_dbld(int db_factor, int db_f1_ctrl, int db_f2_ctrl);
extern void d2d3_config_dpg_lbdg(struct d2d3_lbdg_param_s *lbdgp);
extern void d2d3_config_dpg_mbdg(struct d2d3_mbdg_param_s *mbdgp,struct d2d3_depth_s *depthp);
extern void d2d3_config_dpg_cbdg(struct d2d3_cbdg_param_s *cvdgp);

//Function to configure the D2D3 Data Flow
extern void enable_d2d3dbr_path(bool d2d3_en, int d2d3_flow_mode);
extern int    d2d3_canvas_init(struct d2d3_dev_s *devp);
extern void d2d3_default_config(void);
extern void d2d3_canvas_reverse(bool reverse);
extern void d2d3_config_dpg_canvas(struct d2d3_dev_s *devp);
extern void d2d3_config_dpr_canvas(struct d2d3_dev_s *devp);
extern void d2d3_hw_disable(void);
#endif /* D2D3_H_ */

