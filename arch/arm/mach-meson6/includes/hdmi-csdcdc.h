/***********************************************************************
 * Meson CS DCDC section
 **********************************************************************/
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
#include <mach/voltage.h>
#include <linux/regulator/meson_cs_dcdc_regulator.h>
static struct regulator_consumer_supply vcck_data[] = {
    {
        .supply = "vcck-armcore",
    },
};

static struct regulator_init_data vcck_init_data = {
    .constraints = { /* VCCK default 1.2V */
        .name = "vcck",
        .min_uV =  931000,
        .max_uV =  1308000,
        .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
    },
    .num_consumer_supplies = ARRAY_SIZE(vcck_data),
    .consumer_supplies = vcck_data,
};

static struct meson_cs_pdata_t vcck_pdata = {
    .meson_cs_init_data = &vcck_init_data,
    .voltage_step_table = {
        1308000, 1283000, 1258000, 1232000,
        1207000, 1182000, 1157000, 1132000,
        1107000, 1082000, 1057000, 1032000,
        1007000, 981000,  957000,  931000,
    },
    .default_uV = 1308000,
};

static struct meson_opp vcck_opp_table[] = {
    /* freq must be in descending order */
    {
        .freq   = 1500000,
        .min_uV = 1308000,
        .max_uV = 1308000,
    },
    {
        .freq   = 1320000,
        .min_uV = 1232000,
        .max_uV = 1232000,
    },
    {
        .freq   = 1200000,
        .min_uV = 1207000,
        .max_uV = 1207000,
    },
    {
        .freq   = 1080000,
        .min_uV = 1132000,
        .max_uV = 1132000,
    },
    {
        .freq   = 840000,
        .min_uV = 1032000,
        .max_uV = 1032000,
    },
    {
        .freq   = 600000,
        .min_uV = 1007000,
        .max_uV = 1007000,
    },
    {
        .freq   = 200000,
        .min_uV = 981000,
        .max_uV = 981000,
    }
};//1.3v

static struct regulator_init_data vcck_init_data2 = {
    .constraints = { /* VCCK default 1.36V */
        .name = "vcck",
        .min_uV =  963000,
        .max_uV =  1351000,
        .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
    },
    .num_consumer_supplies = ARRAY_SIZE(vcck_data),
    .consumer_supplies = vcck_data,
};
static struct meson_cs_pdata_t vcck_pdata2 = {
	.meson_cs_init_data = &vcck_init_data2,
	.voltage_step_table = {
		1351000,	1326000,	1300000,	1274000,
		1248000,	1223000,	1197000,	1171000,
		1145000,	1119000,	1093000,	1067000,
		1041000,	1016000,	 990000,	 963000,
	},
	.default_uV = 1351000,
};//1.36v

static struct meson_opp vcck_opp_table2[] = {
    /* freq must be in descending order */
    {
        .freq   = 1500000,
        /*.min_uV = 1351000,
        .max_uV = 1351000,*/
        .min_uV = 1300000,
        .max_uV = 1300000,
    },
    {
        .freq   = 1320000,
        /*.min_uV = 1326000,
        .max_uV = 1326000,*/
        .min_uV = 1223000,
        .max_uV = 1223000,
    },
    {
        .freq   = 1200000,
        .min_uV = 1197000,
        .max_uV = 1197000,
    },
    {
        .freq   = 1080000,
        .min_uV = 1119000,
        .max_uV = 1119000,
    },
    {
        .freq   = 840000,
        .min_uV = 1041000,
        .max_uV = 1041000,
    },
    {
        .freq   = 600000,
        .min_uV = 1016000,
        .max_uV = 1016000,
    },
    {
        .freq   = 200000,
        .min_uV = 990000,
        .max_uV = 990000,
    }
};//1.36v

static struct platform_device meson_cs_dcdc_regulator_device = {
    .name = "meson-cs-regulator",
    .dev = {
        .platform_data = &vcck_pdata,
    }
};
#endif