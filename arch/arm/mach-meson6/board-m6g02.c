/*
 * arch/arm/mach-meson6/board-meson6-ref.c
 *
 * Copyright (C) 2011-2012 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/reboot.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <plat/platform.h>
#include <plat/plat_dev.h>
#include <plat/platform_data.h>

#include <linux/io.h>
#include <plat/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <plat/platform.h>
#include <plat/plat_dev.h>
#include <plat/platform_data.h>
#include <plat/lm.h>
#include <plat/regops.h>
#include <linux/io.h>
#include <plat/io.h>

#include <mach/map.h>
#include <mach/i2c_aml.h>
#include <mach/nand.h>
#include <mach/usbclock.h>
#include <mach/usbsetting.h>
#include <mach/pm.h>
#include <mach/gpio_data.h>
#include <mach/pinmux.h>

#include <linux/uart-aml.h>
#include <linux/i2c-aml.h>
#include "board-m6g02.h"

#ifdef CONFIG_MMC_AML
#include <mach/mmc.h>
#endif

#ifdef CONFIG_CARDREADER
#include <mach/card_io.h>
#endif // CONFIG_CARDREADER

#include <mach/gpio.h>
#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
#endif


#ifdef CONFIG_AW_AXP
#include <linux/power_supply.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/regulator/machine.h>
#include <mach/irqs.h>
#include "../../../drivers/amlogic/power/axp_power/axp-gpio.h"
#include "../../../drivers/amlogic/power/axp_power/axp-mfd.h"
#endif

#include "board-m6ref-power.h"

#include "includes/oimap.h"

#include"includes/fb.h"


#ifdef CONFIG_AM_WIFI
#include <plat/wifi_power.h>
#endif

#ifdef CONFIG_SND_SOC_RT5631
#include <sound/rt5631.h>
#endif

#ifdef CONFIG_SND_SOC_WM8960
#include <sound/wm8960.h>
#endif

#include"includes/hdmi-sensors.h"

//#include"includes/nohdmi-senors.h"

//#include"includes/nohdmi-csdcdc.h"
//#include"includes/nohdmi-cpufreq.h"
#include"includes/hdmi-csdcdc.h"
#include"includes/hdmi-cpufreq.h"

#ifdef CONFIG_SARADC_AM
#include <linux/saradc.h>
static struct platform_device saradc_device = {
    .name = "saradc",
    .id = 0,
    .dev = {
        .platform_data = NULL,
    },
};
#endif

#include"includes/hdmi-gpoi.h"  //vibrate function


#include"includes/hdmi-keys.h" // power and menu keys
//#include"includes/nohdmi-keys.h" // power and menu keys

#include"includes/hdmi-i2c.h" // i2c
//#include"includes/nohdmi-i2c.h" // i2c

#include"includes/uart.h"


/***********************************************************************
 * Nand Section
 **********************************************************************/

#ifdef CONFIG_AM_NAND
#include"m6g02-nand.h"
#endif

/***********************************************************************
 * WIFI  Section
 **********************************************************************/
/**
*/
#include"includes/hdmi-wifi.h" // Wifi
//#include"includes/nohdmi-wifi.h" // wifi

/***********************************************************************
 * IO Mapping
 **********************************************************************/

#include"includes/iomap.h"


/***********************************************************************
 *USB Setting section
 **********************************************************************/
#include"includes/usb.h"



/***********************************************************************
 *WiFi power section
 **********************************************************************/
#include"includes/hdmi-wifipwr.h" // Wifi
//#include"includes/nohdmi-wifipwr.h" // wifi


/***********************************************************************
 * efuse section
 **********************************************************************/

#include"includes/efuse.h"


/***********************************************************************
 * Audio section
 **********************************************************************/

#include"includes/sound.h"



/***********************************************************************
 * Device Register Section
 **********************************************************************/

#include"includes/devreg.h"



static int __init get_voltage_table(char *str)
{

 //   unsigned long  clk=clkparse(str, 0);
// 		char  *ptr=str;
		printk("machineid is %s\n",str);
//		printk("machineid2 is %s\n",ptr);
//		printk("tmpvolt3 is %d\n",tmpvolt);
    if(strcmp(str, "0601")==0) //if(ptr==0601)   1.36
    {
			meson_cs_dcdc_regulator_device.dev.platform_data = &vcck_pdata2,
			printk("1.36 pcb11111111111111111! \n");
			tmpvolt=1;	
    }
		else 
		{
		  meson_cs_dcdc_regulator_device.dev.platform_data = &vcck_pdata,
			printk("1.30 pcb2222222222222222! \n");
			tmpvolt=0;	
		}
//		printk("tmpvolt4 is %d\n",tmpvolt);
    return 0;
}

static __init void meson_init_machine(void)
{
//    meson_cache_init();
    setup_usb_devices();
    setup_devices_resource();
#ifdef CONFIG_AM_WIFI
    wifi_dev_init();
#endif
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
    aml_i2c_init();
#endif
#ifdef CONFIG_AM_WIFI_USB
    if(wifi_plat_data.usb_set_power)
        wifi_plat_data.usb_set_power(0);//power off built-in usb wifi
#endif

#ifdef CONFIG_MPU_SENSORS_MPU3050
    mpu3050_init_irq();
#endif
#ifdef CONFIG_AM_LCD_OUTPUT
  //  m6ref_lcd_init();
#endif
	pm_power_off = power_off;
}
static __init void meson_init_early(void)
{///boot seq 1

}

MACHINE_START(MESON6_G02, "Amlogic Meson6 g02 reference platform")
    .boot_params    = BOOT_PARAMS_OFFSET,
    .map_io         = meson_map_io,///2
    .init_early     = meson_init_early,///3
    .init_irq       = meson_init_irq,///0
    .timer          = &meson_sys_timer,
    .init_machine   = meson_init_machine,
    .fixup          = meson_fixup,///1
MACHINE_END






