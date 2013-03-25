/***********************************************************************
 * WIFI  Section
 **********************************************************************/
/**
*GPIOX_0			-->WIFI_SD_D0
*GPIOX_1			-->WIFI_SD_D1
*GPIOX_2			-->WIFI_SD_D2
*GPIOX_3			-->WIFI_SD_D3

*GPIOX_8			-->WIFI_SD_CMD
*GPIOX_9			-->WIFI_SD_CLK

*WIFI_EN			-->GPIOC_8
*WIFI_WAKE		-->GPIOX_11
*32K_CLOCK_OUT	-->GPIOX_12 (CLK_OUT3)
*/
#ifdef  CONFIG_AM_WIFI
#define DBG_LINE_INFO()  printk(KERN_INFO "[%s] in\n",__func__)

/* WIFI ON Flag */
static int WIFI_ON;
/* BT ON Flag */
static int BT_ON;
static void wifi_gpio_init(void)
{
//set status
    //WIFI_EN WIFI_PWREN  WLAN_RST --->out	:0
	gpio_set_status(PAD_GPIOC_8,gpio_status_out);
	//WIFI_WAKE -->1GPIOX_11   in	:
    	gpio_set_status(PAD_GPIOX_11,gpio_status_in);
	//set pull-up
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,0xf|1<<8|1<<9|1<<11|1<<12);
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<8);	
}

static void wifi_clock_enable(int is_on)
{
    //set clk 32k for wifi  
    //GPIOX_12 (CLK_OUT3)  //reg : 108b  sr_sl:22-25  div:13-19    enable:21
    DBG_LINE_INFO();

	gpio_set_status(PAD_GPIOX_12,gpio_status_out);			//set  GPIOX_12 out
	aml_set_reg32_mask(P_HHI_GEN_CLK_CNTL2,1<<22);//set clk source
	aml_clr_reg32_mask(P_HHI_GEN_CLK_CNTL2,0x3f<<13);//set div ==1
	aml_set_reg32_mask(P_HHI_GEN_CLK_CNTL2,1<<21);//set enable clk
	aml_set_reg32_mask(P_PERIPHS_PIN_MUX_3,0x1<<21);//set mode GPIOX_12-->CLK_OUT3
}

void extern_wifi_set_power(int is_on)
{
	
}
EXPORT_SYMBOL(extern_wifi_set_power);

void extern_wifi_set_enable(int is_on)
{
	DBG_LINE_INFO();
    gpio_set_status(PAD_GPIOC_8,gpio_status_out);//set wifi_en gpio mode out
    if(is_on){
		gpio_out(PAD_GPIOC_8,1);
		printk("WIFI  Enable! \n");
    		}
	else{
		gpio_out(PAD_GPIOC_8,0);
            	printk("WIFI  Disenable! \n");
	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

void extern_wifi_reset(int is_on)
{

}
EXPORT_SYMBOL(extern_wifi_reset);

void wifi_dev_init(void)
{
	DBG_LINE_INFO();
	wifi_clock_enable(1);
	udelay(200);
	wifi_gpio_init();
}
#endif