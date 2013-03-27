#ifdef CONFIG_ANDROID_TIMED_GPIO
#ifndef _LINUX_TIMED_GPIO_H
#define _LINUX_TIMED_GPIO_H
#define TIMED_GPIO_NAME "timed-gpio"
struct timed_gpio {
	const char *name;
	unsigned 	gpio;
	int		max_timeout;
	u8 		active_low;
};

struct timed_gpio_platform_data {
	int 		num_gpios;
	struct timed_gpio *gpios;
};
#endif
static struct timed_gpio amlogic_gpio_vibravor_gpios[] ={
	{
		.name	="vibrator",
		.gpio	= PAD_GPIOC_14,	//gpioc_14
		.max_timeout	= 60000,											//15s
		.active_low	= 1,
	},
};

static struct timed_gpio_platform_data amlogic_gpio_vibravor_data= {
	.num_gpios	= 1,
	.gpios		= amlogic_gpio_vibravor_gpios,
};

static struct platform_device amlogic_gpio_vibravor = {
	.name = TIMED_GPIO_NAME,
	.dev={
		.platform_data= &amlogic_gpio_vibravor_data,
    },
};
#endif