/***********************************************************************
 *USB Setting section
 **********************************************************************/
static void set_usb_a_vbus_power(char is_power_on)
{
//M6 SKT: GPIOD_9,  OEN: 0x2012, OUT:0x2013 , Bit25

#define USB_A_POW_GPIO  PREG_EGPIO
#define USB_A_POW_GPIO_BIT  25
#define USB_A_POW_GPIO_BIT_ON   1
#define USB_A_POW_GPIO_BIT_OFF  0
    if(is_power_on){
        printk( "set usb port power on (board gpio %d)!\n",USB_A_POW_GPIO_BIT);

        //aml_set_reg32_bits(CBUS_REG_ADDR(0x2012),0,USB_A_POW_GPIO_BIT,1);//mode
       // aml_set_reg32_bits(CBUS_REG_ADDR(0x2013),1,USB_A_POW_GPIO_BIT,1);//out
        gpio_out(PAD_GPIOD_9, 1); 
        //set_gpio_mode(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,GPIO_OUTPUT_MODE);
        //set_gpio_val(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,USB_A_POW_GPIO_BIT_ON);
    }
    else    {
        printk("set usb port power off (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
        //aml_set_reg32_bits(CBUS_REG_ADDR(0x2012),0,USB_A_POW_GPIO_BIT,1);//mode
        //aml_set_reg32_bits(CBUS_REG_ADDR(0x2013),0,USB_A_POW_GPIO_BIT,1);//out
        gpio_out(PAD_GPIOD_9, 0); 
    }
}

static  int __init setup_usb_devices(void)
{
    struct lm_device * usb_ld_a, *usb_ld_b;
    usb_ld_a = alloc_usb_lm_device(USB_PORT_IDX_A);
    usb_ld_b = alloc_usb_lm_device(USB_PORT_IDX_B);
    usb_ld_a->param.usb.set_vbus_power = set_usb_a_vbus_power;
    //usb_ld_a->param.usb.port_type = USB_PORT_TYPE_HOST;
    lm_device_register(usb_ld_a);
    lm_device_register(usb_ld_b);
    return 0;
}