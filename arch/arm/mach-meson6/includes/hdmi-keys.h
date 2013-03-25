#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
#include <linux/input.h>
#include <linux/adc_keypad.h>

static struct adc_key adc_kp_key[] = {
    {KEY_MENU,                 "menu", CHAN_4, 9, 40},
    {KEY_VOLUMEDOWN,    "vol-", CHAN_4, 150, 40},
    {KEY_VOLUMEUP,          "vol+", CHAN_4, 275, 40},
    {KEY_HOME,                "home", CHAN_4, 392, 40},
    {KEY_BACK,                "back", CHAN_4, 513, 40},
};

static struct adc_kp_platform_data adc_kp_pdata = {
    .key = &adc_kp_key[0],
    .key_num = ARRAY_SIZE(adc_kp_key),
};

static struct platform_device adc_kp_device = {
    .name = "m1-adckp",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &adc_kp_pdata,
    }
};
#endif

/***********************************************************************
 * Power Key Section
 **********************************************************************/


#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
#include <linux/input.h>
#include <linux/input/key_input.h>

static int _key_code_list[] = {KEY_POWER};

static inline int key_input_init_func(void)
{
    WRITE_AOBUS_REG(AO_RTC_ADDR0, (READ_AOBUS_REG(AO_RTC_ADDR0) &~(1<<11)));
    WRITE_AOBUS_REG(AO_RTC_ADDR1, (READ_AOBUS_REG(AO_RTC_ADDR1) &~(1<<3)));
    return 0;
}
static inline int key_scan(void* data)
{
    int *key_state_list = (int*)data;
    int ret = 0;
    key_state_list[0] = ((READ_AOBUS_REG(AO_RTC_ADDR1) >> 2) & 1) ? 0 : 1;
    return ret;
}

static  struct key_input_platform_data  key_input_pdata = {
    .scan_period = 20,
    .fuzz_time = 60,
    .key_code_list = &_key_code_list[0],
    .key_num = ARRAY_SIZE(_key_code_list),
    .scan_func = key_scan,
    .init_func = key_input_init_func,
    .config = 0,
};

static struct platform_device input_device_key = {
    .name = "meson-keyinput",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &key_input_pdata,
    }
};
#endif


#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN_G06
#include <linux/ft5x06_ts.h>
#define FT_IRQ	INT_GPIO_0

static void ts_power(int on)
{
  gpio_out(PAD_GPIOD_6, on);
}

static int ts_init_irq(void)
{
	gpio_set_status(PAD_GPIOA_16, gpio_status_in);
	gpio_irq_set(PAD_GPIOA_16, GPIO_IRQ( (FT_IRQ-INT_GPIO_0), GPIO_IRQ_FALLING));
	return 0;
}

static struct ts_platform_data ts_pdata = {
	.irq = FT_IRQ,
	.init_irq = ts_init_irq,
	.power = ts_power,
	.screen_max_x=1024,
	.screen_max_y=600,
	.xpol = 1,
	.ypol = 1,
};
#endif