/***********************************************************************
 * I2C Section
 **********************************************************************/

#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
static bool pinmux_dummy_share(bool select)
{
    return select;
}

static pinmux_item_t aml_i2c_a_pinmux_item[] = {
    {
        .reg = 5,
        //.clrmask = (3<<24)|(3<<30),
        .setmask = 3<<26
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_a = {
    .wait_count             = 50000,
    .wait_ack_interval   = 5,
    .wait_read_interval  = 5,
    .wait_xfer_interval   = 5,
    .master_no          = AML_I2C_MASTER_A,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux      = {
        .chip_select    = pinmux_dummy_share,
        .pinmux         = &aml_i2c_a_pinmux_item[0]
    }
};

static pinmux_item_t aml_i2c_b_pinmux_item[]={
    {
        .reg = 5,
        //.clrmask = (3<<28)|(3<<26),
        .setmask = 3<<30
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_b = {
    .wait_count         = 50000,
    .wait_ack_interval = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no          = AML_I2C_MASTER_B,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux      = {
        .chip_select    = pinmux_dummy_share,
        .pinmux         = &aml_i2c_b_pinmux_item[0]
    }
};

static pinmux_item_t aml_i2c_ao_pinmux_item[] = {
    {
        .reg = AO,
        .clrmask  = (3<<1)|(3<<23),
        .setmask = 3<<5
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_ao = {
    .wait_count         = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no          = AML_I2C_MASTER_AO,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_100K,

    .master_pinmux      = {
        .pinmux         = &aml_i2c_ao_pinmux_item[0]
    }
};

static struct resource aml_i2c_resource_a[] = {
    [0] = {
        .start = MESON_I2C_MASTER_A_START,
        .end   = MESON_I2C_MASTER_A_END,
        .flags = IORESOURCE_MEM,
    }
};

static struct resource aml_i2c_resource_b[] = {
    [0] = {
        .start = MESON_I2C_MASTER_B_START,
        .end   = MESON_I2C_MASTER_B_END,
        .flags = IORESOURCE_MEM,
    }
};

static struct resource aml_i2c_resource_ao[] = {
    [0]= {
        .start =    MESON_I2C_MASTER_AO_START,
        .end   =    MESON_I2C_MASTER_AO_END,
        .flags =    IORESOURCE_MEM,
    }
};

static struct platform_device aml_i2c_device_a = {
    .name         = "aml-i2c",
    .id       = 0,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource_a),
    .resource     = aml_i2c_resource_a,
    .dev = {
        .platform_data = &aml_i2c_plat_a,
    },
};

static struct platform_device aml_i2c_device_b = {
    .name         = "aml-i2c",
    .id       = 1,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource_b),
    .resource     = aml_i2c_resource_b,
    .dev = {
        .platform_data = &aml_i2c_plat_b,
    },
};

static struct platform_device aml_i2c_device_ao = {
    .name         = "aml-i2c",
    .id       = 2,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource_ao),
    .resource     = aml_i2c_resource_ao,
    .dev = {
        .platform_data = &aml_i2c_plat_ao,
    },
};

#if defined (CONFIG_AMLOGIC_VIDEOIN_MANAGER)
static struct resource vm_resources[] = {
    [0] = {
        .start = VM_ADDR_START,
        .end   = VM_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device vm_device =
{
    .name = "vm",
    .id = 0,
    .num_resources = ARRAY_SIZE(vm_resources),
    .resource      = vm_resources,
};
#endif /* AMLOGIC_VIDEOIN_MANAGER */

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005
static int gt2005_have_inited = 0;
#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
static pinmux_item_t gc0308_pins[] = {
    {
        .reg = PINMUX_REG(9),
        .setmask = 1 << 12
    },
    PINMUX_END_ITEM
};

static pinmux_set_t gc0308_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &gc0308_pins[0]
};

static void gt2005_init(void)
{
    pinmux_set(&gc0308_pinmux_set);

    aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 1, 8, 5); //select XTAL as camera clock

    // reset low
    printk( "amlogic camera driver: gt2005_v4l2_init. \n");
    gpio_out(PAD_GPIOZ_0, 0);

    // set camera power disanable
    gpio_out(PAD_GPIOE_11, 0);

    msleep(20);

    // reset high
    gpio_out(PAD_GPIOZ_0, 1);
    msleep(20);

    // set camera power enable
    gpio_out(PAD_GPIOE_11, 1);
    msleep(20);
}

static void gt2005_v4l2_init(void)
{
    gt2005_have_inited=1;
    gt2005_init();
}
static void gt2005_v4l2_uninit(void)
{
    gt2005_have_inited=0;
  //  gpio_out(PAD_GPIOE_11, 0);    // set camera power disable
  //  msleep(5);
        aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 0, 8, 1); //close clock
}

static void gt2005_v4l2_disable(void)
{

}

static void gt2005_v4l2_early_suspend(void)
{

}

static void gt2005_v4l2_late_resume(void)
{
    pinmux_set(&gc0308_pinmux_set);

    aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 1, 8, 5); //select XTAL as camera clock
    msleep(20);

    gpio_out(PAD_GPIOZ_0, 1);
    msleep(20);

    // set camera power enable
    gpio_out(PAD_GPIOE_11, 1);
    msleep(20);
}

struct aml_camera_i2c_fig_s custom_gt2005_script[] = {
    {0x0101,0x11},
    {0xffff,0xff}, 
};

static aml_plat_cam_data_t video_gt2005_data = {
    .name="video-gt2005",
    .video_nr=0,   //    1
    .device_init= gt2005_v4l2_init,
    .device_uninit=gt2005_v4l2_uninit,
    .early_suspend = gt2005_v4l2_early_suspend,
    .late_resume = gt2005_v4l2_late_resume,
    .device_disable=gt2005_v4l2_disable,
		.custom_init_script = custom_gt2005_script,
};
#endif


static struct i2c_board_info __initdata aml_i2c_bus_info_a[] = {

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005
    {
        /*gt2005 i2c address is 0x78/0x79*/
        I2C_BOARD_INFO("gt2005_i2c",  0x78 >> 1 ),
        .platform_data = (void *)&video_gt2005_data
    },
#endif
#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN_G06
    {
        I2C_BOARD_INFO("ft5x06", 0x38),
        .platform_data = (void *)&ts_pdata,
    },
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_ao[] = {
#ifdef CONFIG_AW_AXP20
    {
        I2C_BOARD_INFO("axp20_mfd", AXP20_ADDR),
        .platform_data = &axp_pdata,
        .addr = AXP20_ADDR, //axp_cfg.pmu_twi_addr,
        .irq = INT_WATCHDOG,	//0 smp irq number base change to 32
    },
#endif

};


static struct i2c_board_info __initdata aml_i2c_bus_info_b[] = {
#ifdef CONFIG_BOSCH_BMA250
	{
		I2C_BOARD_INFO("bma250",  0x18),
		//.irq = INT_GPIO_1,
	},
#endif
#ifdef CONFIG_SND_SOC_WM8960
    {
        I2C_BOARD_INFO("wm8960", 0x1A),
        .platform_data = (void *)NULL,
    },
#endif
};

static int __init aml_i2c_init(void)
{
    i2c_register_board_info(0, aml_i2c_bus_info_a,
        ARRAY_SIZE(aml_i2c_bus_info_a));
    i2c_register_board_info(1, aml_i2c_bus_info_b,
        ARRAY_SIZE(aml_i2c_bus_info_b));
    i2c_register_board_info(2, aml_i2c_bus_info_ao,
        ARRAY_SIZE(aml_i2c_bus_info_ao));
    return 0;
}
#endif
