/***********************************************************************
 * Device Register Section
 **********************************************************************/
static struct platform_device  *platform_devs[] = {
#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
    &aml_i2c_device_a,
    &aml_i2c_device_b,
    &aml_i2c_device_ao,
#endif
    &aml_uart_device,
    &meson_device_fb,
    &meson_device_vout,
#ifdef CONFIG_AM_STREAMING
    &meson_device_codec,
#endif
#if defined(CONFIG_AM_NAND)
    &aml_nand_device,
#endif
#if defined(CONFIG_TVIN_VDIN)
    &vdin_device,
#endif
#if defined(CONFIG_TVIN_BT656IN)
    &bt656in_device,
#endif
#ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
    &vm_device,
#endif
#if defined(CONFIG_CARDREADER)
    //&meson_card_device,
#endif // CONFIG_CARDREADER
#if defined(CONFIG_MMC_AML)
    &aml_mmc_device,
#endif
#if defined(CONFIG_SUSPEND)
    &aml_pm_device,
#endif
#ifdef CONFIG_EFUSE
    &aml_efuse_device,
#endif
#if defined(CONFIG_AML_RTC)
    &aml_rtc_device,
#endif
#ifdef CONFIG_SARADC_AM
        &saradc_device,
#endif
#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
    &adc_kp_device,
#endif
#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
    &input_device_key,
#endif
    &aml_audio,
    &aml_audio_dai,
#if defined(CONFIG_SND_SOC_RT5631)
    &aml_rt5631_audio,
#endif
#if defined(CONFIG_SND_SOC_WM8960)
    &aml_wm8960_audio,
#endif
#ifdef CONFIG_AM_WIFI
    &wifi_power_device,
#endif

#ifdef CONFIG_USB_ANDROID
    &android_usb_device,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
    &usb_mass_storage_device,
#endif
#endif

#ifdef CONFIG_POST_PROCESS_MANAGER
    &ppmgr_device,
#endif
#if defined(CONFIG_AM_DEINTERLACE) || defined (CONFIG_DEINTERLACE)
  &deinterlace_device,
#endif
#if defined(CONFIG_AM_TV_OUTPUT2)
    &vout2_device,
#endif
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
    &meson_cs_dcdc_regulator_device,
#endif
#ifdef CONFIG_CPU_FREQ
    &meson_cpufreq_device
#endif
#ifdef CONFIG_ANDROID_TIMED_GPIO
//	&amlogic_gpio_vibravor,
#endif
};
