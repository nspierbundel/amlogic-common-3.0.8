#obj-y += axp-mfd.o

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

MAKE_FOR_LIB := n
ifeq ($(MAKE_FOR_LIB), n)
ifeq ($(CONFIG_UBOOT_BATTERY_PARAMETERS), y) 
$(obj)/axp20-with_uboot.o:
	@cp $(TOP_KBUILD_SRC)drivers/amlogic/power/axp_power/axp20-with_uboot.z \
        drivers/amlogic/power/axp_power/axp20-with_uboot.o
else
$(obj)/axp20-without_uboot.o:
	@cp $(TOP_KBUILD_SRC)drivers/amlogic/power/axp_power/axp20-without_uboot.z \
        drivers/amlogic/power/axp_power/axp20-without_uboot.o
endif
endif

#Make this built-in, so that it will be loaded before I2C driver
obj-$(CONFIG_AW_AXP18) += axp18-board.o
obj-$(CONFIG_AW_AXP18) += axp18-regu.o
obj-$(CONFIG_AW_AXP18) += axp18-sply.o
obj-$(CONFIG_AW_AXP18) += virtual18.o
obj-$(CONFIG_AW_AXP18) += virtual18_dev.o

obj-$(CONFIG_AW_AXP19) += axp19-board.o
obj-$(CONFIG_AW_AXP19) += axp19-regu.o
obj-$(CONFIG_AW_AXP19) += axp19-sply.o
obj-$(CONFIG_AW_AXP19) += axp19-gpio.o
obj-$(CONFIG_AW_AXP19) += virtual19.o
obj-$(CONFIG_AW_AXP19) += virtual19_dev.o

#obj-$(CONFIG_AW_AXP20) += axp20-board.o
#obj-$(CONFIG_AW_AXP20) += axp20-regu.o
#obj-$(CONFIG_AW_AXP20) += axp20-sply.o
#obj-$(CONFIG_AW_AXP20) += axp20-gpio.o
#obj-$(CONFIG_AW_AXP20) += virtual20.o
#obj-$(CONFIG_AW_AXP20) += virtual20_dev.o

ifeq ($(CONFIG_UBOOT_BATTERY_PARAMETERS), y) 
    ifeq ($(MAKE_FOR_LIB), y)
        axp20-with_uboot-y := axp-mfd.o axp20-regu.o axp20-sply.o axp20-gpio.o virtual20.o virtual20_dev.o
        obj-y += axp20-with_uboot.o
    else
        obj-$(CONFIG_UBOOT_BATTERY_PARAMETERS) += axp20-with_uboot.o
    endif
else
    ifeq ($(MAKE_FOR_LIB), y)
        axp20-without_uboot-y := axp-mfd.o axp20-regu.o axp20-sply.o axp20-gpio.o virtual20.o virtual20_dev.o
        obj-y += axp20-without_uboot.o
    else
        obj-y += axp20-without_uboot.o
    endif
endif

