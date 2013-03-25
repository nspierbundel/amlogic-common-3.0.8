/***********************************************************************
 * Sensors Section
 **********************************************************************/



#ifdef CONFIG_AW_AXP20
static struct i2c_board_info __initdata aml_i2c_bus_info_ao[];

extern void axp_power_off(void);

/* Reverse engineered partly from Platformx drivers */
enum axp_regls {

    vcc_ldo1,
    vcc_ldo2,
    vcc_ldo3,
    vcc_ldo4,
    vcc_ldo5,

    vcc_buck2,
    vcc_buck3,
    vcc_ldoio0,
};

/* The values of the various regulator constraints are obviously dependent
 * on exactly what is wired to each ldo.  Unfortunately this information is
 * not generally available.  More information has been requested from Xbow
 * but as of yet they haven't been forthcoming.
 *
 * Some of these are clearly Stargate 2 related (no way of plugging
 * in an lcd on the IM2 for example!).
 */

static struct regulator_consumer_supply ldo1_data[] = {
    {
        .supply = "axp20_rtc",
    },
};


static struct regulator_consumer_supply ldo2_data[] = {
    {
        .supply = "axp20_analog/fm",
    },
};

static struct regulator_consumer_supply ldo3_data[] = {
    {
        .supply = "axp20_pll",
    },
};

static struct regulator_consumer_supply ldo4_data[] = {
    {
        .supply = "axp20_hdmi",
    },
};

static struct regulator_consumer_supply ldoio0_data[] = {
    {
        .supply = "axp20_mic",
    },
};


static struct regulator_consumer_supply buck2_data[] = {
    {
        .supply = "axp20_core",
    },
};

static struct regulator_consumer_supply buck3_data[] = {
    {
        .supply = "axp20_ddr",
    },
};

static struct axp_cfg_info axp_cfg = {
    .pmu_twi_id = 2,		//AXP20_I2CBUS
    .pmu_irq_id = INT_WATCHDOG,
    .pmu_twi_addr = AXP20_ADDR,
	.pmu_battery_rdc = 132,
	.pmu_battery_cap = 3800,
    .pmu_init_chgcur = 1000000,		//set initial charging current limite
	.pmu_suspend_chgcur = 1200000,	//set suspend charging current limite
    .pmu_resume_chgcur = 1000000,							//set resume charging current limite
    .pmu_shutdown_chgcur = 1200000,		//set shutdown charging current limite
    .pmu_init_chgvol = 4200,			//set initial charing target voltage
    .pmu_init_chgend_rate = 15,		//set initial charing end current	rate
    .pmu_init_chg_enabled = 1,		//set initial charing enabled
	.pmu_init_adc_freq = 25,		//set initial adc frequency
	.pmu_init_adc_freqc = 100,		//set initial coulomb adc coufrequency
	.pmu_init_chg_pretime = 50,		//set initial pre-charging time
	.pmu_init_chg_csttime = 480,		//set initial pre-charging time
	.pmu_usbvol_limit = 1,
    .pmu_usbvol = 4000,
    .pmu_usbcur_limit = 0,
    .pmu_usbcur = 900,
    .pmu_pwroff_vol = 2600,
    .pmu_pwron_vol = 2600,
    .pmu_pekoff_time = 6000,
    .pmu_pekoff_en  = 1,
    .pmu_peklong_time = 1500,
    .pmu_pwrok_time   = 64,
    .pmu_pwrnoe_time = 2000,
    .pmu_intotp_en = 1,
    .pmu_pekon_time = 128,		//powerkey hold time for power on
};

static struct regulator_init_data axp_regl_init_data[] = {
    [vcc_ldo1] = {
        .constraints = { /* board default 1.25V */
            .name = "axp20_ldo1",
            .min_uV =  1300 * 1000,
            .max_uV =  1300 * 1000,
        },
        .num_consumer_supplies = ARRAY_SIZE(ldo1_data),
        .consumer_supplies = ldo1_data,
    },
    [vcc_ldo2] = {
        .constraints = { /* board default 3.0V */
            .name = "axp20_ldo2",
            .min_uV = 1800000,
            .max_uV = 3300000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
            .initial_state = PM_SUSPEND_STANDBY,
            .state_standby = {
                //.uV = ldo2_vol * 1000,
                .enabled = 1,
            }
        },
        .num_consumer_supplies = ARRAY_SIZE(ldo2_data),
        .consumer_supplies = ldo2_data,
    },
    [vcc_ldo3] = {
        .constraints = {/* default is 1.8V */
            .name = "axp20_ldo3",
            .min_uV =  700 * 1000,
            .max_uV =  3500* 1000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
            .initial_state = PM_SUSPEND_STANDBY,
            .state_standby = {
                //.uV = ldo3_vol * 1000,
                .enabled = 1,
            }
        },
        .num_consumer_supplies = ARRAY_SIZE(ldo3_data),
        .consumer_supplies = ldo3_data,
    },
    [vcc_ldo4] = {
        .constraints = {
            /* board default is 3.3V */
            .name = "axp20_ldo4",
            .min_uV = 1250000,
            .max_uV = 3300000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
            .initial_state = PM_SUSPEND_STANDBY,
            .state_standby = {
                //.uV = ldo4_vol * 1000,
                .enabled = 1,
            }
        },
        .num_consumer_supplies = ARRAY_SIZE(ldo4_data),
        .consumer_supplies = ldo4_data,
    },
    [vcc_buck2] = {
        .constraints = { /* default 1.24V */
            .name = "axp20_buck2",
            .min_uV = 700 * 1000,
            .max_uV = 2275 * 1000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
            .initial_state = PM_SUSPEND_STANDBY,
            .state_standby = {
                .uV = 1500 * 1000,  //axp_cfg.dcdc2_vol * 1000,
                .enabled = 1,
            }
        },
        .num_consumer_supplies = ARRAY_SIZE(buck2_data),
        .consumer_supplies = buck2_data,
    },
    [vcc_buck3] = {
        .constraints = { /* default 2.5V */
            .name = "axp20_buck3",
            .min_uV = 700 * 1000,
            .max_uV = 3500 * 1000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
            .initial_state = PM_SUSPEND_STANDBY,
            .state_standby = {
                .uV = 1100 * 1000,  //axp_cfg.dcdc3_vol * 1000,
                .enabled = 1,
            }
        },
        .num_consumer_supplies = ARRAY_SIZE(buck3_data),
        .consumer_supplies = buck3_data,
    },
    [vcc_ldoio0] = {
        .constraints = { /* default 2.5V */
            .name = "axp20_ldoio0",
            .min_uV = 1800 * 1000,
            .max_uV = 3300 * 1000,
            .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
        },
        .num_consumer_supplies = ARRAY_SIZE(ldoio0_data),
        .consumer_supplies = ldoio0_data,
    },
};

static struct axp_funcdev_info axp_regldevs[] = {
    {
        .name = "axp20-regulator",
        .id = AXP20_ID_LDO1,
        .platform_data = &axp_regl_init_data[vcc_ldo1],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_LDO2,
        .platform_data = &axp_regl_init_data[vcc_ldo2],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_LDO3,
        .platform_data = &axp_regl_init_data[vcc_ldo3],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_LDO4,
        .platform_data = &axp_regl_init_data[vcc_ldo4],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_BUCK2,
        .platform_data = &axp_regl_init_data[vcc_buck2],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_BUCK3,
        .platform_data = &axp_regl_init_data[vcc_buck3],
    }, {
        .name = "axp20-regulator",
        .id = AXP20_ID_LDOIO0,
        .platform_data = &axp_regl_init_data[vcc_ldoio0],
    },
};

static struct power_supply_info battery_data ={
    .name ="PTI PL336078",
    .technology = POWER_SUPPLY_TECHNOLOGY_LiFe,
    .voltage_max_design = 4200000,	//set initial charing target voltage
    .voltage_min_design = 2600 * 1000,  //axp_cfg.pmu_pwroff_vol * 1000,
	.energy_full_design = 3800,		//battery capability
    .use_for_apm = 1,
};

static struct axp_supply_init_data axp_sply_init_data = {
   /* .battery_info = &battery_data,
    .chgcur = 500000,		//set initial charging current limite
    .chgvol = 4200000,	//set initial charing target voltage
    .chgend = 10,		//set initial charing end current	rate
    .chgen = 1,			//set initial charing enabled
    .sample_time = 100,	//set initial coulomb adc coufrequency
    .chgpretime = 50,		//set initial pre-charging time
    .chgcsttime = 480,		//set initial pre-charging time
*/
};

static struct axp_funcdev_info axp_splydev[]={
    {
        .name = "axp20-supplyer",
            .id = AXP20_ID_SUPPLY,
      .platform_data = &axp_sply_init_data,
    },
};

static axp_gpio_cfg_t axp_init_gpio_cfg[] = {
    {
        .gpio = AXP_GPIO1,			//AXP202 GPIO1 ==> VCCX2 
        .dir = AXP_GPIO_OUTPUT,
        .level = AXP_GPIO_LOW,		//set AXP202 GPIO1 low
    },
    {
        .gpio = AXP_GPIO2,			//AXP202 GPIO2 ==> HEMI2V_EN 
        .dir = AXP_GPIO_OUTPUT,
        .level = AXP_GPIO_LOW,		//set AXP202 GPIO2 low
    },
    AXPGPIO_CFG_END_ITEM
};


static struct axp_funcdev_info axp_gpiodev[]={
    {   .name = "axp20-gpio",
        .id = AXP20_ID_GPIO,
        .platform_data = axp_init_gpio_cfg,
    },
};

static struct axp_platform_data axp_pdata = {
    .num_regl_devs = ARRAY_SIZE(axp_regldevs),
    .num_sply_devs = ARRAY_SIZE(axp_splydev),
    .num_gpio_devs = ARRAY_SIZE(axp_gpiodev),
    .regl_devs = axp_regldevs,
    .sply_devs = axp_splydev,
    .gpio_devs = axp_gpiodev,
    .gpio_base = 0,
    .axp_cfg = &axp_cfg,
};

#endif
