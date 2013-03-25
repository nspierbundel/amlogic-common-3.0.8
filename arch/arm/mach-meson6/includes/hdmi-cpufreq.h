/***********************************************************************
 * Meson CPUFREQ section
 **********************************************************************/
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#include <plat/cpufreq.h>

#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
static struct regulator *vcck;
static struct meson_cpufreq_config cpufreq_info;
static int tmpvolt=-1;

static unsigned int vcck_cur_max_freq(void)
{
//		printk("tmpvolt1 is %d\n",tmpvolt);
    if(tmpvolt) {//old pcb
        return meson_vcck_cur_max_freq(vcck, vcck_opp_table2, ARRAY_SIZE(vcck_opp_table2));
    }
    else {
        return meson_vcck_cur_max_freq(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table));
    }
}

static int vcck_scale(unsigned int frequency)
{
//		printk("tmpvolt2 is %d\n",tmpvolt);
    if(tmpvolt) {//old pcb
        return meson_vcck_scale(vcck, vcck_opp_table2, ARRAY_SIZE(vcck_opp_table2),frequency);
    }
    else  {
        return meson_vcck_scale(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table),frequency);
    }
}

static int vcck_regulator_init(void)
{
    vcck = regulator_get(NULL, vcck_data[0].supply);
    if (WARN(IS_ERR(vcck), "Unable to obtain voltage regulator for vcck;"
                    " voltage scaling unsupported\n")) {
        return PTR_ERR(vcck);
    }

    return 0;
}

static struct meson_cpufreq_config cpufreq_info = {
    .freq_table = NULL,
    .init = vcck_regulator_init,
    .cur_volt_max_freq = vcck_cur_max_freq,
    .voltage_scale = vcck_scale,
};
#endif //CONFIG_MESON_CS_DCDC_REGULATOR


static struct platform_device meson_cpufreq_device = {
    .name   = "cpufreq-meson",
    .dev = {
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
        .platform_data = &cpufreq_info,
#else
        .platform_data = NULL,
#endif
    },
    .id = -1,
};
#endif //CONFIG_CPU_FREQ


