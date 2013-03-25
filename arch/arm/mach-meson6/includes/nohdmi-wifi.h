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

*WIFI_EN			-->GPIOC_7
*WIFI_PWREN		-->GPIOC_8
*BT/WLAN_RST		-->GPIOC_9
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
//set pinmux
	aml_clr_reg32_mask(P_PREG_PAD_GPIO2_EN_N,0x7<<7);
//set status
    //WIFI_EN WIFI_PWREN  WLAN_RST --->out	:0
   	gpio_set_status(PAD_GPIOC_7,gpio_status_out);
	gpio_set_status(PAD_GPIOC_8,gpio_status_out);
	gpio_set_status(PAD_GPIOC_9,gpio_status_out);

	//WIFI_WAKE -->1GPIOX_11   in	:
    	gpio_set_status(PAD_GPIOX_11,gpio_status_in);

	//set pull-up
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,0xf|1<<8|1<<9|1<<11|1<<12);
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<7|1<<8|1<<9);
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
	//WIFI_POWER-->GPIOC_8
	DBG_LINE_INFO();
    gpio_set_status(PAD_GPIOC_8,gpio_status_out);
    if(is_on){
		gpio_out(PAD_GPIOC_8,1);
		printk("BCM Module Power up!\n");
    }
	else{
        /* only set power_controll  pin when wifi and bt off */
      //  if((!WIFI_ON) && (!BT_ON)){
		   gpio_out(PAD_GPIOC_8,0);
            printk("BCM Module Power down!\n");
       // }
	}
}
EXPORT_SYMBOL(extern_wifi_set_power);

void wifi_set_enable(int is_on)
{
	DBG_LINE_INFO();
    gpio_set_status(PAD_GPIOC_7,gpio_status_out);//set wifi_en gpio mode out
    if(is_on){
		gpio_out(PAD_GPIOC_7,1);
		printk("WIFI  Enable! \n");
    		}
	else{
		gpio_out(PAD_GPIOC_7,0);
            	printk("WIFI  Disenable! \n");
	}
}
EXPORT_SYMBOL(wifi_set_enable);

void extern_wifi_reset(int is_on)
{
	DBG_LINE_INFO();
	gpio_set_status(PAD_GPIOC_9,gpio_status_out);//WIFI_RST
	if(is_on){
	gpio_out(PAD_GPIOC_9,1);
	   printk("WIFI  REST Up ! \n");
	}
    else{
	gpio_out(PAD_GPIOC_9,0);
	   printk("WIFI  REST Down! \n");
	}
    return;
}
EXPORT_SYMBOL(extern_wifi_reset);

static void reg_on_control(int is_on)
{
	DBG_LINE_INFO();
   //set wifi_en
    gpio_set_status(PAD_GPIOC_7,gpio_status_out);//wifi_en =
    if(is_on){
		gpio_out(PAD_GPIOC_7,1);
    }
	else{
        /* only pull donw reg_on pin when wifi and bt off */
        if((!WIFI_ON) && (!BT_ON)){
		gpio_out(PAD_GPIOC_7,0);
            	printk("WIFI BT Power down(WIFI_EN)\n");
        }
	}
}

void wifi_dev_init(void)
{
	DBG_LINE_INFO();
	wifi_clock_enable(1);
	udelay(200);
	wifi_gpio_init();
#if 1
	extern_wifi_set_power(0);//ainou for wl_reg_on
	udelay(200);
	udelay(200);
	extern_wifi_set_power(1);//ainou for wl_reg_on
#endif
}

EXPORT_SYMBOL(wifi_dev_init);

static void extern_wifi_power(int is_power)
{
	DBG_LINE_INFO();

	WIFI_ON = is_power;
	mdelay(200);
	reg_on_control(is_power);

}
//EXPORT_SYMBOL(extern_wifi_power);

#endif


/*WIFI 40181*/

#if defined(CONFIG_SDIO_DHD_CDC_WIFI_40181_MODULE_MODULE)
/******************************
*WL_REG_ON      -->GPIOC_8
*WIFI_32K               -->GPIOC_15(CLK_OUT1)
*WIFIWAKE(WL_HOST_WAKE)-->GPIOX_11
*******************************/
//#define WL_REG_ON_USE_GPIOC_6
void extern_wifi_set_enable(int enable)
{
        if(enable){
#ifdef WL_REG_ON_USE_GPIOC_6
                SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
#else
                SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
#endif
                printk("Enable WIFI  Module!\n");
        }
        else{
#ifdef WL_REG_ON_USE_GPIOC_6
                CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
#else
                CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
#endif
                printk("Disable WIFI  Module!\n");
        }
}
EXPORT_SYMBOL(extern_wifi_set_enable);

static void wifi_set_clk_enable(int on)
{
    //set clk for wifi
        printk("set WIFI CLK Pin GPIOC_15 32KHz ***%d\n",on);
        WRITE_CBUS_REG(HHI_GEN_CLK_CNTL,(READ_CBUS_REG(HHI_GEN_CLK_CNTL)&(~(0x7f<<0)))|((0<<0)|(1<<8)|(7<<9)) );
        CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<15));
        if(on)
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));
        else
                CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));
}

static void aml_wifi_bcm4018x_init()
{
        wifi_set_clk_enable(1);
        wifi_gpio_init();
        extern_wifi_set_enable(0);
        msleep(5);
        extern_wifi_set_enable(1);
}

#endif