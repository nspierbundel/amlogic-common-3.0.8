/***********************************************************************
 * Audio section
 **********************************************************************/

static struct resource aml_m6_audio_resource[] = {
    [0] =   {
        .start      =   0,
        .end        =   0,
        .flags      =   IORESOURCE_MEM,
    },
};

static struct platform_device aml_audio = {
    .name           = "aml-audio",
    .id             = 0,
};

static struct platform_device aml_audio_dai = {
    .name           = "aml-dai",
    .id             = 0,
};

extern char* get_vout_mode_internal(void);

/* Check current mode, 0: panel; 1: !panel*/
int get_display_mode(void) {
	int ret = 0;
	if(strncmp("panel", get_vout_mode_internal(), 5)){
		ret = 1;
	}

	return ret;
}


#if defined(CONFIG_SND_SOC_RT5631)

static pinmux_item_t rt5631_pinmux[] = {
    /* I2S_MCLK I2S_BCLK I2S_LRCLK I2S_DIN I2S_DOUT */
    {
        .reg = PINMUX_REG(9),
        .setmask = (1 << 7) | (1 << 5) | (1 << 9) | (1 << 11) | (1 << 4),
        .clrmask = (7 << 19) | (7 << 1) | (1 << 10) | (1 << 6),
    },
    {
        .reg = PINMUX_REG(8),
        .clrmask = (0x7f << 24),
    },
    PINMUX_END_ITEM
};

static pinmux_set_t rt5631_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &rt5631_pinmux[0],
};

static void rt5631_device_init(void)
{
    /* audio pinmux */
    pinmux_set(&rt5631_pinmux_set);

    /* GPIOA_19 PULL_UP_REG0 bit19 */
    aml_set_reg32_bits(P_PAD_PULL_UP_REG0, 1, 19, 1);
}

static void rt5631_device_deinit(void)
{
    pinmux_clr(&rt5631_pinmux_set);
}

static int rt5631_hp_detect(void)
{
#define HP_PLUG     1
#define HP_UNPLUG   0

    int val = HP_UNPLUG;
    uint32_t level = 0;

    if(get_display_mode() != 0) {   //if !panel, return HP_PLUG
        return HP_PLUG;
    }

    /* GPIOA_19 */
    aml_set_reg32_bits(P_PREG_PAD_GPIO0_EN_N, 1, 19, 1);    // mode
    level = aml_get_reg32_bits(P_PREG_PAD_GPIO0_I, 19, 1);  // value
    if (level == 0) {
        val = HP_PLUG;
    }

    return val;
}

static struct rt5631_platform_data rt5631_pdata = {
    .hp_detect      = rt5631_hp_detect,
    .device_init    = rt5631_device_init,
    .device_uninit  = rt5631_device_deinit,

    .spk_watt       = RT5631_SPK_0_5W,
    .spk_output     = RT5631_SPK_STEREO,
    .mic_input      = RT5631_MIC_DIFFERENTIAL,
};

static struct platform_device aml_rt5631_audio = {
    .name           = "aml_rt5631_audio",
    .id             = 0,
    .resource       = aml_m6_audio_resource,
    .num_resources  = ARRAY_SIZE(aml_m6_audio_resource),
    .dev = {
        .platform_data = &rt5631_pdata,
    },
};

#endif

#if defined(CONFIG_SND_SOC_WM8960)

static pinmux_item_t wm8960_pinmux[] = {
    /* I2S_MCLK I2S_BCLK I2S_LRCLK I2S_DIN I2S_DOUT */
    {
        .reg = PINMUX_REG(9),
        .setmask = (1 << 7) | (1 << 5) | (1 << 9) | (1 << 11) | (1 << 4),
        .clrmask = (7 << 19) | (7 << 1) | (1 << 10) | (1 << 6),
    },
    {
        .reg = PINMUX_REG(8),
        .clrmask = (0x7f << 24),
    },
    PINMUX_END_ITEM
};

static pinmux_set_t wm8960_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &wm8960_pinmux[0],
};

static void wm8960_device_init(void)
{
    /* audio pinmux */
    pinmux_set(&wm8960_pinmux_set);

    /* GPIOA_19 PULL_UP_REG0 bit19 */
    aml_set_reg32_bits(P_PAD_PULL_UP_REG0, 1, 19, 1);
}

static void wm8960_device_deinit(void)
{
    pinmux_clr(&wm8960_pinmux_set);
}

static int wm8960_hp_detect(void)
{
#define HP_PLUG     1
#define HP_UNPLUG   0

    int val = HP_UNPLUG;
    uint32_t level = 0;

    if(get_display_mode() != 0) {   //if !panel, return HP_PLUG
        return HP_PLUG;
    }

    /* GPIOA_19 */
    aml_set_reg32_bits(P_PREG_PAD_GPIO0_EN_N, 1, 19, 1);    // mode
    level = aml_get_reg32_bits(P_PREG_PAD_GPIO0_I, 19, 1);  // value
    if (level == 0) {
        val = HP_PLUG;
    }

    return val;
}

static struct wm8960_data wm8960_pdata = {
    .hp_detect      = wm8960_hp_detect,
    .device_init    = wm8960_device_init,
    .device_uninit  = wm8960_device_deinit,
    .capless        = 0,
    .dres           = WM8960_DRES_600R,

};

static struct platform_device aml_wm8960_audio = {
    .name           = "aml_wm8960_audio",
    .id             = 0,
    .resource       = aml_m6_audio_resource,
    .num_resources  = ARRAY_SIZE(aml_m6_audio_resource),
    .dev = {
        .platform_data = &wm8960_pdata,
    },
};

#endif


static void power_off(void)
{
	kernel_restart("charging_reboot");
}

