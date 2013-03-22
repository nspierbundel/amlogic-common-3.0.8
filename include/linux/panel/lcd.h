/*
 * LCD Panel
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2012 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AML_PANEL_LCD_H
#define __AML_PANEL_LCD_H


struct panel_pm_ops {
	void (*lcd_power_on)(void);
	void (*lcd_power_off)(void);
	void (*bl_power_on)(void);
	void (*bl_power_off)(void);
};

struct lcd_platform_data {
	struct panel_pm_ops pm_ops;
	/* it indicates whether lcd panel was enabled
	   from bootloader or not.
	*/
	int lcd_enable;

	/* stable time needing to become lcd power on. */
	unsigned int lcd_power_on_delay;
	/* stable time needing to become lcd power off. */
	unsigned int lcd_power_off_delay;

	/* stable time needing to become backlight power on. */
	unsigned int bl_power_on_delay;
	/* stable time needing to become backlight power off. */
	unsigned int bl_power_off_delay;

	/* it could be used for any purpose. */
	void *private_data;
};


#endif /* __AML_PANEL_LCD_H */
