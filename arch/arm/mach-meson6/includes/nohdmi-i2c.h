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

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
static int gc0308_have_inited = 0;
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

static int gc0308_init(void)
{
    pinmux_set(&gc0308_pinmux_set);
    aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 1, 8, 5); //select XTAL as camera clock

    msleep(20);
    // set camera power enable
    gpio_out(PAD_GPIOE_11, 1);    // set camera power enable
    msleep(20);

    gpio_out(PAD_GPIOZ_0, 0);    // reset IO
    msleep(20);

    gpio_out(PAD_GPIOZ_0, 1);    // reset IO
    msleep(20);

    // set camera power enable
    gpio_out(PAD_GPIOE_11, 0);    // set camera power enable
    msleep(20);

    printk("gc0308_init OK!!!!\n");
    return 0;
}

static int gc0308_v4l2_init(void)
{
    gc0308_have_inited=1;
    gc0308_init();
}
static int gc0308_v4l2_uninit(void)
{
    gc0308_have_inited=0;
    printk( "amlogic camera driver: gc0308_v4l2_uninit. \n");
    gpio_out(PAD_GPIOE_11, 1);    // set camera power disable
    msleep(5);
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_OV2655)
    if (ov2655_have_inited == 0)
#endif
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
    if (gt2005_have_inited == 0)
#endif
        aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 0, 8, 1); //close clock
}
static void gc0308_v4l2_early_suspend(void)
{

}

static void gc0308_v4l2_late_resume(void)
{
    pinmux_set(&gc0308_pinmux_set);

    aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 1, 8, 5); //select XTAL as camera clock
    msleep(20);

    gpio_out(PAD_GPIOZ_0, 1);    // reset IO
    msleep(20);

    // set camera power enable
    gpio_out(PAD_GPIOE_11, 1);   // set camera power enable
    msleep(20);
}

static struct aml_camera_i2c_fig1_s gc0308_custom_init_script[] = {
    {0x14,0x11},
    {0xff,0xff},
};

static aml_plat_cam_data_t video_gc0308_data = {
    .name="video-gc0308",
    .video_nr=0,//1,
    .device_init= gc0308_v4l2_init,
    .device_uninit=gc0308_v4l2_uninit,
    .early_suspend = gc0308_v4l2_early_suspend,
    .late_resume = gc0308_v4l2_late_resume,
    .custom_init_script = gc0308_custom_init_script,
};
#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)

static void gt2005_init(void)
{
    pinmux_set(&gc0308_pinmux_set);

    aml_set_reg32_bits(P_HHI_GEN_CLK_CNTL, 1, 8, 5); //select XTAL as camera clock

    // reset low
    printk( "amlogic camera driver: gt2005_v4l2_init. \n");
    gpio_out(PAD_GPIOZ_0, 0);

    // set camera power disanable
    gpio_out(PAD_GPIOE_10, 0);

    msleep(20);

    // reset high
    gpio_out(PAD_GPIOZ_0, 1);
    msleep(20);

    // set camera power enable
    gpio_out(PAD_GPIOE_10, 1);
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
    gpio_out(PAD_GPIOE_10, 0);    // set camera power disable
    msleep(5);
    #if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
    if(gc0308_have_inited==0)
    #endif
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
    gpio_out(PAD_GPIOE_10, 1);
    msleep(20);
}

static aml_plat_cam_data_t video_gt2005_data = {
    .name="video-gt2005",
    .video_nr=1,   //    1
    .device_init= gt2005_v4l2_init,
    .device_uninit=gt2005_v4l2_uninit,
    .early_suspend = gt2005_v4l2_early_suspend,
    .late_resume = gt2005_v4l2_late_resume,
    .device_disable=gt2005_v4l2_disable,
};
#endif

static struct i2c_board_info __initdata aml_i2c_bus_info_a[] = {

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
    {
        /*gc0308 i2c address is 0x42/0x43*/
        I2C_BOARD_INFO("gc0308_i2c",  0x42 >> 1),
        .platform_data = (void *)&video_gc0308_data,
    },
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005
    {
        /*gt2005 i2c address is 0x78/0x79*/
        I2C_BOARD_INFO("gt2005_i2c",  0x78 >> 1 ),
        .platform_data = (void *)&video_gt2005_data
    },
#endif
#ifdef CONFIG_GOODIX_GT82X_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("Goodix-TS", 0x5d),
        .platform_data = (void *)&gt82x_data
    },
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_ao[] = {
#ifdef CONFIG_AW_AXP20
    {
        I2C_BOARD_INFO("axp20_mfd", AXP20_ADDR),
        .platform_data = &axp_pdata,
        .addr = AXP20_ADDR, //axp_cfg.pmu_twi_addr,
        .irq = AXP20_IRQNO, //axp_cfg.pmu_irq_id,
    },
#endif

};


static struct i2c_board_info __initdata aml_i2c_bus_info_b[] = {
#ifdef CONFIG_MPU_SENSORS_MPU3050
    {
        I2C_BOARD_INFO("mpu3050", 0x68),
        .irq = MPU3050_IRQ,
        .platform_data = (void *)&mpu3050_data,
    },
#endif
#ifdef CONFIG_SND_SOC_RT5631
    {
        I2C_BOARD_INFO("rt5631", 0x1A),
        .platform_data = (void *)NULL,
    },
#endif
#ifdef CONFIG_SND_SOC_WM8960
    {
        I2C_BOARD_INFO("wm8960", 0x1A),
        .platform_data = (void *)NULL,
    },
#endif

};

#ifdef CONFIG_EEPROM_AT24

void eeprom_write_protect(int enable)
{
	/* @todo eeprom_write_protect */
}

static struct eeprom_platform_data eeprom_pdata = {
	.byte_len   = (4 * 1024),
	.page_size  = 4096,
	.flags      = AT24_FLAG_ADDR16 | AT24_FLAG_IRUGO,
	.write_protect = eeprom_write_protect,
};
#endif /* CONFIG_EEPROM_AT24 */

#ifdef CONFIG_EEPROM_AT24_SLAVE
static struct eeprom_platform_data eeprom_slave_pdata = {
	.byte_len   = (4 * 1024),
	.page_size  = 4096,
	.flags      = AT24_FLAG_ADDR16 | AT24_FLAG_IRUGO | AT24_FLAG_SLAVE,
	.write_protect = eeprom_write_protect,
};
#endif /* CONFIG_EEPROM_AT24_SLAVE */

static struct i2c_board_info __initdata aml_i2c_eeprom_info[] = {
#ifdef CONFIG_EEPROM_AT24
	{
		I2C_BOARD_INFO("at24", 0x50),
		.platform_data = &eeprom_pdata,
	},
#endif
#ifdef CONFIG_EEPROM_AT24_SLAVE
	{
		I2C_BOARD_INFO("at24_slave", 0x51),
		.platform_data = &eeprom_slave_pdata,
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
	i2c_register_board_info(0, aml_i2c_eeprom_info,
		ARRAY_SIZE(aml_i2c_eeprom_info));
	return 0;
}
#endif