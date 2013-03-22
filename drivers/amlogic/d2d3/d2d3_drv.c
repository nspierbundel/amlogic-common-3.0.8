#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
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
#include <linux/proc_fs.h>
//#include <linux/aml_common.h>
#include <asm/uaccess.h>

#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/amports/canvas.h>
#include <mach/am_regs.h>

#include "d2d3_drv.h"
#include "../deinterlace/deinterlace.h"
#define D2D3_NAME               "d2d3"
#define D2D3_DRIVER_NAME        "d2d3"
#define D2D3_MODULE_NAME        "d2d3"
#define D2D3_DEVICE_NAME        "d2d3"
#define D2D3_CLASS_NAME         "d2d3"

#define VFM_NAME        "d2d3"

#define D2D3_COUNT 1


static int version = 0.0;
module_param(version, int, 0664);
MODULE_PARM_DESC(version, "\n version\n");


static d2d3_dev_t *d2d3_devp;
static struct vframe_provider_s * prov = NULL;
static dev_t d2d3_devno;
static struct class *d2d3_clsp;
struct semaphore thread_sem;
static unsigned short d2d3_mode = 4;
module_param(d2d3_mode, ushort, 0644);
MODULE_PARM_DESC(d2d3_mode,"the mode of d2d3 processing.");

static bool d2d3_dbg_en = 0;
module_param(d2d3_dbg_en, bool, 0664);
MODULE_PARM_DESC(d2d3_dbg_en, "\n d2d3_dbg_en\n");

#define d2d3_pr      if(d2d3_dbg_en) printk
                                        
/*****************************
 * d2d3 processing mode
 * mode0:    DE_PRE-->DPG-->Memory
 * mode0:   DE_POST-->DBG-->VPP_SCALER
 *
 * mode1:    DE_PRE-->DPG-->Memory
 * mode1:VPP_SCALER-->DBG-->VPP_BLENDING
 *
 * mode2:   DE_POST-->DPG-->Memory
 * mode2:   DE_POST-->DBG-->VPP_SCALER
 *
 * mode3:VPP_SCALER-->DPG-->Memory
 * mode3:VPP_SCALER-->DBG-->VPP_BLENDING
 ******************************/

static int d2d3_receiver_event_fun(int type, void* data, void* arg);

static const struct vframe_receiver_op_s d2d3_vf_receiver =
{
        .event_cb = d2d3_receiver_event_fun
};

static struct vframe_receiver_s d2d3_vf_recv;

static vframe_t *d2d3_vf_peek(void* arg);
static vframe_t *d2d3_vf_get(void* arg);
static void d2d3_vf_put(vframe_t *vf, void* arg);
static int d2d3_event_cb(int type, void *data, void *private_data);

static const struct vframe_operations_s d2d3_vf_provider =
{
        .peek = d2d3_vf_peek,
        .get  = d2d3_vf_get,
        .put  = d2d3_vf_put,
        .event_cb = d2d3_event_cb,
};

static struct vframe_provider_s d2d3_vf_prov;

/*****************************
 *    attr management :
 ******************************/
static ssize_t store_dbg(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
        return count;
}

static DEVICE_ATTR(debug, S_IWUGO | S_IRUGO, NULL, store_dbg);


/*****************************
 *    d2d3 process :
 ******************************/
#define D2D3_IDX_MAX  2
typedef struct{
        int (*pre_early_process_fun)(void* arg);
        int (*pre_process_fun)(void* arg, unsigned zoom_start_x_lines,
                        unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines);
        void* pre_private_data;
        vframe_t* vf;
        /*etc*/
}d2d3_devnox_t;
d2d3_devnox_t  d2d3_devnox[D2D3_IDX_MAX];

static unsigned char have_process_fun_private_data = 0;

static int d2d3_early_process_fun(void* arg)
{//return 1, make video set video_property_changed
        /* arg is vf->private_data */
        int ret = 0;
        int idx = (int)arg;
        if(idx>=0 && idx<D2D3_IDX_MAX){
                d2d3_devnox_t* p_d2d3_devnox = &d2d3_devnox[idx];
                if(have_process_fun_private_data && p_d2d3_devnox->pre_early_process_fun){
                        ret = p_d2d3_devnox->pre_early_process_fun(p_d2d3_devnox->pre_private_data);
                }
        }
        return ret;
}
/*
 *check the format , flow mode,out mode...
 */
static int d2d3_check_param(unsigned int width,unsigned int height, unsigned int reverse_flag)
{
        int ret = 0;
        if(width != d2d3_devp->param.width ||height != d2d3_devp->param.height){                
                d2d3_pr("[d2d3..]fmt changed from %ux%u to %ux%u.\n",d2d3_devp->param.width, d2d3_devp->param.height, width, height);
                
                d2d3_devp->param.width  = width;
                d2d3_devp->param.height = height;
                /*update the canvas size*/
                d2d3_canvas_init(d2d3_devp);
                ret = 1;
        }
        if(reverse_flag != d2d3_devp->param.reverse_flag)
        {                
                d2d3_pr("[d2d3..]reverse flag changed from %u to %u.\n",d2d3_devp->param.reverse_flag,reverse_flag); 
                d2d3_devp->param.reverse_flag = reverse_flag;
                d2d3_canvas_reverse(d2d3_devp->param.reverse_flag);
        }
        if(d2d3_mode != d2d3_devp->param.flow_mode){
                d2d3_pr("[d2d3..]mode changed from %u to %u.\n",d2d3_devp->param.flow_mode,d2d3_mode); 
                d2d3_devp->param.flow_mode = d2d3_mode;
                ret = 1;
        }
        if(ret)
                d2d3_config(d2d3_devp);
        
        return ret;
}

static int d2d3_post_process_fun(void* arg, unsigned zoom_start_x_lines,
                unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines)
{
        int ret = 0;
        int idx = (int)arg;
        d2d3_devnox_t* p_d2d3_devnox = NULL;
        di_buf_t *di_buf_p = NULL;
        if((idx>=0) && (idx<D2D3_IDX_MAX)){
                p_d2d3_devnox = &d2d3_devnox[idx];
                if(have_process_fun_private_data && p_d2d3_devnox->pre_process_fun){
                        ret = p_d2d3_devnox->pre_process_fun(p_d2d3_devnox->pre_private_data, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);
                }
        }

        zoom_start_x_lines = zoom_start_x_lines&0xffff;
        zoom_end_x_lines = zoom_end_x_lines&0xffff;
        zoom_start_y_lines = zoom_start_y_lines&0xffff;
        zoom_end_y_lines = zoom_end_y_lines&0xffff;

        di_buf_p = p_d2d3_devnox->vf->private_data;
        /* d2d3 process start, to do ... */
        if(1 == d2d3_mode || 2 == d2d3_mode){
                //use the depth addr from di update the dbr canvas addr
                d2d3_devp->dpr_addr = di_buf_p->dp_buf_adr;
                d2d3_config_dpr_canvas(d2d3_devp);
        }
        else{//mode 3 mode 4
                //swap_the_wr_rd_canvas_addr();
                if(!d2d3_check_param(p_d2d3_devnox->vf->width,p_d2d3_devnox->vf->height,0)){
                        d2d3_config_dpg_canvas(d2d3_devp);
                        d2d3_config_dpr_canvas(d2d3_devp);
                        swap(d2d3_devp->dpg_addr,d2d3_devp->dpr_addr);
                }
        }
        if(p_d2d3_devnox->pre_process_fun)
                p_d2d3_devnox->pre_process_fun(p_d2d3_devnox->pre_private_data, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);
        /* d2d3 process end */
        return ret;
}

/*****************************
 *    d2d3 vfm interface :
 ******************************/

static vframe_t *d2d3_vf_peek(void* arg)
{
        vframe_t* vframe_ret = NULL;

        if (prov && prov->ops && prov->ops->peek){
                //printk("%s\n", __func__);
                vframe_ret = prov->ops->peek(prov->op_arg);
                //printk("%s %x\n", __func__, vframe_ret);
        }
        return vframe_ret;
}

static vframe_t *d2d3_vf_get(void* arg)
{
        vframe_t* vframe_ret = NULL;
        int i;
        if (prov && prov->ops && prov->ops->get){
                //printk("%s\n", __func__);
                vframe_ret = prov->ops->get(prov->op_arg);

                if(vframe_ret){
                        for(i=0; i<D2D3_IDX_MAX; i++){
                                if(d2d3_devnox[i].vf == NULL){
                                        break;
                                }
                        }
                        if(i==D2D3_IDX_MAX){
                                printk("D2D3 Error, idx is not enough\n");
                        }
                        else{
                                d2d3_devnox[i].vf = vframe_ret;
                                /* backup early_process_fun/process_fun/private_data */
                                d2d3_devnox[i].pre_early_process_fun = vframe_ret->early_process_fun;
                                d2d3_devnox[i].pre_process_fun = vframe_ret->process_fun;
                                d2d3_devnox[i].pre_private_data = vframe_ret->private_data;

                                vframe_ret->early_process_fun = d2d3_early_process_fun;
                                vframe_ret->process_fun = d2d3_post_process_fun;
                                vframe_ret->private_data = (void*)i;

                                /* d2d3 process code start*/

                                /*d2d3 process code*/

                                //printk("%s %d %x\n", __func__, i, vframe_ret);
                        }
                }

        }

        return vframe_ret;
}

static void d2d3_vf_put(vframe_t *vf, void* arg)
{
        if (prov && prov->ops && prov->ops->put){
                int idx = (int)(vf->private_data);
                //printk("%s %d\n", __func__,idx);
                /* d2d3 process code start*/

                /*d2d3 process code*/
                if((idx<D2D3_IDX_MAX)&&(idx>=0)){
                        d2d3_devnox[idx].vf = NULL;
                        /* restore early_process_fun/process_fun/private_data */
                        vf->early_process_fun = d2d3_devnox[idx].pre_early_process_fun;
                        vf->process_fun = d2d3_devnox[idx].pre_process_fun;
                        vf->private_data = d2d3_devnox[idx].pre_private_data;
                }
                else{
                        printk("[d2d3..] %s,error, return vf->private_data %x is not in the"\
                        "range.\n", __func__,idx);
                }
                prov->ops->put(vf, prov->op_arg);
        }
}

static int d2d3_event_cb(int type, void *data, void *private_data)
{
        return 0;
}

static int d2d3_receiver_event_fun(int type, void* data, void* arg)
{
        int i;
        if((type == VFRAME_EVENT_PROVIDER_UNREG)||
                        (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME)){
                prov = NULL;

                vf_unreg_provider(&d2d3_vf_prov);
                //disable hw
                d2d3_pr("provider unregister,disable d2d3.\n");
                        
        }
        else if(type == VFRAME_EVENT_PROVIDER_REG){
                char* provider_name = (char*)data;
                if(strcmp(provider_name, "deinterlace")==0){
                        have_process_fun_private_data = 1;
                }
                else{
                        have_process_fun_private_data = 0;
                }
                vf_reg_provider(&d2d3_vf_prov);
                for(i=0; i<D2D3_IDX_MAX; i++){
                        d2d3_devnox[i].vf = NULL;
                }
                prov = vf_get_provider(VFM_NAME);
                /*init the hardware*/        
                d2d3_canvas_init(d2d3_devp);
                d2d3_config(d2d3_devp);
                d2d3_pr("provider register,enable d2d3 hw.\n");
        }
        else if((VFRAME_EVENT_PROVIDER_DPBUF_CONFIG == type) &&
                        ((1 == d2d3_mode)||(2 == d2d3_mode)))
        {
                vframe_t * vf = (vframe_t*)data;
                struct di_buf_s *di_buf = vf->private_data;
                /*check the format & config canvas*/
                if(!d2d3_check_param(vf->width,vf->height,di_buf->reverse_flag)){
                        d2d3_devp->dpg_addr = di_buf->dp_buf_adr;
                        d2d3_config_dpg_canvas(d2d3_devp);
                }
        } 
        return 0;
}



/*****************************
 *    d2d3 driver file_operations
 *
 ******************************/
static struct platform_device* d2d3_platform_device = NULL;

static int d2d3_open(struct inode *node, struct file *file)
{
        d2d3_dev_t *d2d3_in_devp;

        /* Get the per-device structure that contains this cdev */
        d2d3_in_devp = container_of(node->i_cdev, d2d3_dev_t, cdev);
        file->private_data = d2d3_in_devp;
        return 0;
}


static int d2d3_release(struct inode *node, struct file *file)
{
        file->private_data = NULL;
        return 0;
}

static int d2d3_add_cdev(struct cdev *cdevp, const struct file_operations 
*file_ops,int minor)
{
        int ret = 0;
        dev_t devno = MKDEV(MAJOR(d2d3_devno),minor);
        cdev_init(cdevp,file_ops);
        cdevp->owner = THIS_MODULE;
        ret = cdev_add(cdevp,devno,1);
        return ret;
}
static struct device *d2d3_create_device(struct device *parent, int min)
{
        dev_t devno = MKDEV(MAJOR(d2d3_devno),min);
        return device_create(d2d3_clsp,parent,devno,NULL, D2D3_DEVICE_NAME);
}
const static struct file_operations d2d3_fops = {
        .owner    = THIS_MODULE,
        .open     = d2d3_open,
        .release  = d2d3_release,
};

static int d2d3_probe(struct platform_device *pdev)
{
        int ret;
        struct d2d3_dev_s *devp;
        struct resource *res;

        printk("d2d3_probe\n");
        /* kmalloc d2d3 dev */
        devp = kmalloc(sizeof(struct d2d3_dev_s), GFP_KERNEL);
        if (!devp)
        {
                printk("[d2d3..]: failed to allocate memory for d2d3 device.\n");
                ret = -ENOMEM;
                goto failed_kmalloc_devp;
        }        
        memset(devp,0,sizeof(d2d3_dev_t));
        d2d3_devp = devp;
        
         /*create cdev and register with sysfs*/
        ret = d2d3_add_cdev(&devp->cdev, &d2d3_fops,0);
        if (ret) {
                printk("[d2d3..]%s failed to add device\n",__func__);
                /* @todo do with error */
                goto failed_add_cdev;
        }
        /*create the udev node /dev/...*/
        devp->dev = d2d3_create_device(&pdev->dev,0);
        if (devp->dev == NULL) {
                printk("[d2d3..] %s device_create create error\n",__func__);
                ret = -EEXIST;
                goto failed_create_device;
        }
        device_create_file(devp->dev, &dev_attr_debug);
        /* get device memory */
        res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!res) {
                pr_err("[d2d3..]: can't get memory resource\n");
                ret = -EFAULT;
                goto failed_get_resource;
        }
        d2d3_devp->mem_start = res->start;
        d2d3_devp->mem_size  = res->end - res->start + 1;
        printk("[d2d3..] mem_start = 0x%x, mem_size = 0x%x\n", d2d3_devp->mem_start,d2d3_devp->mem_size);

        vf_receiver_init(&d2d3_vf_recv, VFM_NAME, &d2d3_vf_receiver, NULL);
        vf_reg_receiver(&d2d3_vf_recv);

        vf_provider_init(&d2d3_vf_prov, VFM_NAME, &d2d3_vf_provider, NULL);
failed_get_resource:
        device_remove_file(&pdev->dev,&dev_attr_debug);
failed_create_device:
        cdev_del(&devp->cdev);
failed_add_cdev:
        kfree(devp);
failed_kmalloc_devp:
        return ret;
}

static int d2d3_remove(struct platform_device *pdev)
{
        vf_unreg_provider(&d2d3_vf_prov);
        vf_unreg_receiver(&d2d3_vf_recv);

        /* Remove the cdev */
        device_remove_file(d2d3_devp->dev, &dev_attr_debug);

        cdev_del(&d2d3_devp->cdev);
        kfree(d2d3_devp);
        d2d3_devp = NULL;
        device_del(d2d3_devp->dev);
        return 0;
}

static struct platform_driver d2d3_driver = {
        .probe      = d2d3_probe,
        .remove     = d2d3_remove,
        .driver     = {
                .name   = D2D3_DEVICE_NAME,
                .owner  = THIS_MODULE,
        }
};

static int  __init d2d3_drv_init(void)
{
        int ret = 0;
        printk("d2d3_init\n");
#if 0
        d2d3_platform_device = platform_device_alloc(D2D3_DEVICE_NAME,0);
        if (!d2d3_platform_device) {
                printk("failed to alloc d2d3_platform_device\n");
                return -ENOMEM;
        }

        if(platform_device_add(d2d3_platform_device)){
                platform_device_put(d2d3_platform_device);
                printk("failed to add d2d3_platform_device\n");
                return -ENODEV;
        }
        if (platform_driver_register(&d2d3_driver)) {
                printk("failed to register d2d3 module\n");

                platform_device_del(d2d3_platform_device);
                platform_device_put(d2d3_platform_device);
                return -ENODEV;
        }
#else
        /*allocate major device number*/
        ret = alloc_chrdev_region(&d2d3_devno, 0, D2D3_COUNT, D2D3_DEVICE_NAME);
        if (ret < 0) {
                printk("[d2d3..]%s can't register major for d2d3 device.\n",__func__);
                goto failed_alloc_cdev_region;
        }
        d2d3_clsp = class_create(THIS_MODULE, D2D3_DEVICE_NAME);
        if (IS_ERR(d2d3_clsp)){
                ret = PTR_ERR(d2d3_clsp);
                goto failed_create_class;
        }
        if (platform_driver_register(&d2d3_driver)) {
                printk("[d2d3..]%s failed to register d2d3 driver.\n",__func__);
                goto failed_register_driver;
        }
#endif
failed_register_driver:
        class_destroy(d2d3_clsp);
failed_create_class:
        unregister_chrdev_region(d2d3_devno, D2D3_COUNT);
failed_alloc_cdev_region:
        return ret;
}




static void __exit d2d3_drv_exit(void)
{
        printk("[d2d3..]%s d2d3_exit.\n",__func__);
        class_destroy(d2d3_clsp);        
        unregister_chrdev_region(d2d3_devno, D2D3_COUNT);
        platform_driver_unregister(&d2d3_driver);
        //platform_device_unregister(d2d3_platform_device);
        d2d3_platform_device = NULL;
        return ;
}


module_init(d2d3_drv_init);
module_exit(d2d3_drv_exit);

MODULE_DESCRIPTION("AMLOGIC D2D3 driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");


