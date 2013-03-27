#if defined(CONFIG_AML_HDMI_TX)
static struct hdmi_phy_set_data brd_phy_data[] = {
	{-1,   -1},         //end of phy setting
};

static struct hdmi_config_platform_data aml_hdmi_pdata ={
	.hdmi_5v_ctrl = NULL,
	.hdmi_3v3_ctrl = NULL,
	.hdmi_pll_vdd_ctrl = NULL,
	.hdmi_sspll_ctrl = NULL,
	.phy_data = brd_phy_data,
};

static struct platform_device aml_hdmi_device = {
	.name = "amhdmitx",
	.id   = -1,
	.dev  = {
		.platform_data = &aml_hdmi_pdata,
	}
};
#endif
