/***********************************************************************/
#ifdef CONFIG_EFUSE
static bool efuse_data_verify(unsigned char *usid)
{  int len;

    len = strlen(usid);
    if((len > 8)&&(len<31) )
        return true;
    else
        return false;
}

static struct efuse_platform_data aml_efuse_plat = {
    .pos = 454,
    .count = 58,
    .data_verify = efuse_data_verify,
};

static struct platform_device aml_efuse_device = {
    .name   = "efuse",
    .id = -1,
    .dev = {
                .platform_data = &aml_efuse_plat,
           },
};
#endif

#if defined(CONFIG_AML_RTC)
static  struct platform_device aml_rtc_device = {
            .name            = "aml_rtc",
            .id               = -1,
    };
#endif

#if defined(CONFIG_TVIN_VDIN)
static struct resource vdin_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,  //pbufAddr
        .end   = VDIN_ADDR_END,     //pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = VDIN_ADDR_START,
        .end   = VDIN_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [2] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
    [3] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
};

static struct platform_device vdin_device = {
    .name       = "vdin",
    .id         = -1,
    .num_resources = ARRAY_SIZE(vdin_resources),
    .resource      = vdin_resources,
};
#endif

#ifdef CONFIG_TVIN_BT656IN
//add pin mux info for bt656 input
#if 0
static struct resource bt656in_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,      //pbufAddr
        .end   = VDIN_ADDR_END,             //pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {     //bt656/camera/bt601 input resource pin mux setting
        .start =  0x3000,       //mask--mux gpioD 15 to bt656 clk;  mux gpioD 16:23 to be bt656 dt_in
        .end   = PERIPHS_PIN_MUX_5 + 0x3000,
        .flags = IORESOURCE_MEM,
    },

    [2] = {         //camera/bt601 input resource pin mux setting
        .start =  0x1c000,      //mask--mux gpioD 12 to bt601 FIQ; mux gpioD 13 to bt601HS; mux gpioD 14 to bt601 VS;
        .end   = PERIPHS_PIN_MUX_5 + 0x1c000,
        .flags = IORESOURCE_MEM,
    },

    [3] = {         //bt601 input resource pin mux setting
        .start =  0x800,        //mask--mux gpioD 24 to bt601 IDQ;;
        .end   = PERIPHS_PIN_MUX_5 + 0x800,
        .flags = IORESOURCE_MEM,
    },

};
#endif

static struct platform_device bt656in_device = {
    .name       = "amvdec_656in",
    .id         = -1,
//    .num_resources = ARRAY_SIZE(bt656in_resources),
//    .resource      = bt656in_resources,
};
#endif

//tmp fix by Elvis Yu
static void m6ref_set_vccx2(int power_on)
{
/*    if (power_on) {
        printk(KERN_INFO "%s() Power ON\n", __FUNCTION__);
        axp_gpio_set_io(1,1);	//AXP 202 GPIO1 VCCX2 
		axp_gpio_set_value(1, 0);	//set AXP 202 GPIO1 low
    }
    else {
        printk(KERN_INFO "%s() Power OFF\n", __FUNCTION__);
        axp_gpio_set_io(1,1);	//GPIO1 VCCX2 
		axp_gpio_set_value(1, 1);	////set AXP 202 GPIO1 high
    }
    */
}
#if defined(CONFIG_SUSPEND)
static struct meson_pm_config aml_pm_pdata = {
    .pctl_reg_base = (void *)IO_APB_BUS_BASE,
    .mmc_reg_base = (void *)APB_REG_ADDR(0x1000),
    .hiu_reg_base = (void *)CBUS_REG_ADDR(0x1000),
    .power_key = (1<<8),
    .ddr_clk = 0x00110820,
    .sleepcount = 128,
    .set_vccx2 = m6ref_set_vccx2,
    .core_voltage_adjust = 7,  //5,8
};

static struct platform_device aml_pm_device = {
    .name           = "pm-meson",
    .dev = {
        .platform_data  = &aml_pm_pdata,
    },
    .id             = -1,
};
#endif
