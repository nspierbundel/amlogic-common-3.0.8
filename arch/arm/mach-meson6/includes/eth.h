/*
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

#ifdef CONFIG_AM_ETHERNET
static void aml_eth_reset(void)
{
	printk(KERN_INFO "****** aml_eth_reset() ******\n");

	aml_clr_reg32_mask(P_PREG_ETHERNET_ADDR0, 1);           // Disable the Ethernet clocks
	// ---------------------------------------------
	// Test 50Mhz Input Divide by 2
	// ---------------------------------------------
	// Select divide by 2
	aml_clr_reg32_mask(P_PREG_ETHERNET_ADDR0, (1<<3));     // desc endianess "same order" 
	aml_clr_reg32_mask(P_PREG_ETHERNET_ADDR0, (1<<2));     // ata endianess "little"
	aml_set_reg32_mask(P_PREG_ETHERNET_ADDR0, (1<<1));     // divide by 2 for 100M
	aml_set_reg32_mask(P_PREG_ETHERNET_ADDR0, 1);          // enable Ethernet clocks

    /* setup ethernet interrupt */
 #ifdef CONFIG_ARCH_MESON6

    aml_set_reg32_mask(P_SYS_CPU_0_IRQ_IN0_INTR_MASK, 1 << 8);
    aml_set_reg32_mask(P_SYS_CPU_0_IRQ_IN1_INTR_STAT, 1 << 8);
 #endif
 #ifdef CONFIG_ARCH_MESON3
    aml_set_reg32_mask(P_A9_0_IRQ_IN0_INTR_MASK, 1 << 8);
    aml_set_reg32_mask(P_A9_0_IRQ_IN1_INTR_STAT, 1 << 8);

 #endif
	udelay(100);
	/* hardware reset ethernet phy */
	gpio_direction_output(GPIO_ETH_RESET, 0);
	msleep(20);
	gpio_set_value(GPIO_ETH_RESET, 1);
}

static void aml_eth_clock_enable(void)
{
	unsigned int clk_divider = 0;
	unsigned int clk_invert = 0;
	unsigned int clk_source = 0;


	printk(KERN_INFO "****** aml_eth_clock_enable() ******\n");

#ifdef NET_EXT_CLK
	// old: eth_clk_set(ETH_CLKSRC_EXT_XTAL_CLK, (50 * CLK_1M), (50 * CLK_1M), 1);

	/* External Clock */
	clk_divider = 1;				// Clock Divider (50 / 50 )
	clk_invert  = 1;				// 1 = invert, 0 = non-invert clock signal
	clk_source  = ETH_CLKSRC_EXT_XTAL_CLK;		// Source, see clk_set.h
#else
	// old: eth_clk_set(ETH_CLKSRC_MISC_CLK, get_misc_pll_clk(), (50 * CLK_1M), 0);
	/* Internal Clock */
	// get_misc_pll_clk() = 480M
	clk_divider = (unsigned int)480/50;		// Clock Divider
	clk_invert  = 0;				// 1 = invert, 0 = non-invert clock signal
	clk_source  = ETH_CLKSRC_MISC_CLK;		// Source, see clk_set.h
#endif
	aml_write_reg32(P_HHI_ETH_CLK_CNTL, (
		(clk_divider - 1) << 0 |		// Clock Divider
		clk_source << 9 |			// Clock Source ETH_CLKSRC_MISC_CLK
		((clk_invert == 1) ? 1 : 0) << 14 |	// PAD signal invert
		1 << 8 					// enable clk
		)
	);
	printk("P_HHI_ETH_CLK_CNTL = 0x%x\n", aml_read_reg32(P_HHI_ETH_CLK_CNTL));

}

static void aml_eth_clock_disable(void)
{
    printk(KERN_INFO "****** aml_eth_clock_disable() ******\n");
    /* disable ethernet clk */
    aml_clr_reg32_mask(P_HHI_ETH_CLK_CNTL, 1 << 8);
}

static pinmux_item_t aml_eth_pins[] = {
    /* RMII pin-mux */
    {
		.reg = PINMUX_REG(6),
		.clrmask = (3<<17),
#ifdef NET_EXT_CLK
		.setmask = (1<<18), //(3<<17), // bit 18 = ETH_CLK_IN_GPIOY0_REG6_18, // BIT 17 = Ethernet in???
#else 
		.setmask = (1<<17), //(3<<17), // bit 18 = ETH_CLK_IN_GPIOY0_REG6_18, // BIT 17 = Ethernet in???
#endif
    },
    PINMUX_END_ITEM
};

static pinmux_set_t aml_eth_pinmux = {
    .chip_select = NULL,
    .pinmux = aml_eth_pins,
};

static void aml_eth_pinmux_setup(void)
{
    printk(KERN_INFO "****** aml_eth_pinmux_setup() ******\n");
	/* Old 2.6 old 
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6,(3<<17));//reg6[17/18]=0
		eth_set_pinmux(ETH_BANK0_GPIOY1_Y9, ETH_CLK_IN_GPIOY0_REG6_18, 0);
	
		results in:
	*/
	pinmux_clr(&aml_eth_pinmux);
	pinmux_set(&aml_eth_pinmux);
	aml_set_reg32_mask(P_PERIPHS_PIN_MUX_0, 0);
}

static void aml_eth_pinmux_cleanup(void)
{
    printk(KERN_INFO "****** aml_eth_pinmux_cleanup() ******\n");
    pinmux_clr(&aml_eth_pinmux);
}

static void aml_eth_init(void)
{
    aml_eth_pinmux_setup();
    aml_eth_clock_enable();
    aml_eth_reset();

	/* debug code */
	printk("P_PERIPHS_PIN_MUX_0 = 0x%8x\n", aml_read_reg32(P_PERIPHS_PIN_MUX_0));
	printk("P_PERIPHS_PIN_MUX_6 = 0x%8x\n", aml_read_reg32(P_PERIPHS_PIN_MUX_6));	
	printk("P_PREG_ETHERNET_ADDR0 = 0x%8x\n", aml_read_reg32(P_PREG_ETHERNET_ADDR0));
}

static struct aml_eth_platdata aml_eth_pdata __initdata = {
    .pinmux_items = aml_eth_pins,
    .pinmux_setup = aml_eth_pinmux_setup,
    .pinmux_cleanup = aml_eth_pinmux_cleanup,
    .clock_enable = aml_eth_clock_enable,
    .clock_disable = aml_eth_clock_disable,
    .reset = aml_eth_reset,
};

static void __init setup_eth_device(void)
{
    meson_eth_set_platdata(&aml_eth_pdata);
    aml_eth_init();
}
#endif

