/***********************************************************************
 *WiFi power section
 **********************************************************************/
/* built-in usb wifi power ctrl, usb dongle must register NULL to power_ctrl! 1:power on  0:power off */
#ifdef CONFIG_AM_WIFI
#ifdef CONFIG_AM_WIFI_USB
static void usb_wifi_power(int is_power)
{
    printk(KERN_INFO "usb_wifi_power %s\n", is_power ? "On" : "Off");
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
    if (is_power)
        CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
    else
        SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
    return ;
}

static struct wifi_power_platform_data wifi_plat_data = {
    .usb_set_power = usb_wifi_power,
};
#elif defined(CONFIG_AM_WIFI_SD_MMC)&&defined(CONFIG_CARDREADER)
    wifi_plat_data = {

};
#endif

static struct platform_device wifi_power_device = {
    .name       = "wifi_power",
    .id     = -1,
    .dev = {
        .platform_data = &wifi_plat_data,
    },
};
#endif