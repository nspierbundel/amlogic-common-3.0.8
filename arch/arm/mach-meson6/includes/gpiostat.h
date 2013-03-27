static void print_gpio_status(void)
{
    unsigned int pin, bank, bit, value;
    int mode;

    printk("### Print GPIO STATUS###\n");

// #define GPIOA_bank_bit0_27(bit) (PREG_PAD_GPIO0)
// #define GPIOA_bit_bit0_27(bit) (bit)

    /* GPIO A */
    printk("# GPIO A #\n");
    for (pin=0;pin<28;pin++) {
bank = GPIOA_bank_bit0_27(pin);
bit = GPIOA_bit_bit0_27(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO A%d: out: %x\n" , pin, value);
}
    }

//#define GPIOB_bank_bit0_23(bit) (PREG_PAD_GPIO1)
//#define GPIOB_bit_bit0_23(bit) (bit)

    /* GPIO B */
    printk("# GPIO B #\n");
    for (pin=0;pin<24;pin++) {
bank = GPIOB_bank_bit0_23(pin);
bit = GPIOB_bit_bit0_23(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO B%d: out: %x\n" , pin, value);
}
    }

// #define GPIOC_bank_bit0_15(bit) (PREG_PAD_GPIO2)
// #define GPIOC_bit_bit0_15(bit) (bit)

    /* GPIO C */
    printk("# GPIO C #\n");
    for (pin=0;pin<16;pin++) {
bank = GPIOC_bank_bit0_15(pin);
bit = GPIOC_bit_bit0_15(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO C%d: out: %x\n" , pin, value);
}
    }

// #define GPIOAO_bank_bit0_11(bit) (PREG_PAD_GPIOAO)
// #define GPIOAO_bit_bit0_11(bit) (bit)

    /* GPIO AO */
    printk("# GPIO AO #\n");
    for (pin=0;pin<11;pin++) {
bank = GPIOAO_bank_bit0_11(pin);
bit = GPIOAO_bit_bit0_11(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO AO%d: out: %x\n" , pin, value);
}
    }

// #define GPIOD_bank_bit0_9(bit) (PREG_PAD_GPIO2)
// #define GPIOD_bit_bit0_9(bit) (bit+16)

    /* GPIO D */
    printk("# GPIO D #\n");
    for (pin=0;pin<16;pin++) {
bank = GPIOD_bank_bit0_9(pin);
bit = GPIOD_bit_bit0_9(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO D%d: out: %x\n" , pin, value);
}
    }

// #define GPIOX_bank_bit0_31(bit) (PREG_PAD_GPIO4)
// #define GPIOX_bit_bit0_31(bit) (bit)

    /* GPIO X */
    printk("# GPIO X #\n");
    for (pin=0;pin<32;pin++) {
bank = GPIOX_bank_bit0_31(pin);
bit = GPIOX_bit_bit0_31(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO X%d: out: %x\n" , pin, value);
}
    }

// #define GPIOX_bank_bit32_35(bit) (PREG_PAD_GPIO3)
// #define GPIOX_bit_bit32_35(bit) (bit- 32 + 20)

    /* GPIO X */
    for (pin=32;pin<36;pin++) {
bank = GPIOX_bank_bit32_35(pin);
bit = GPIOX_bit_bit32_35(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO X%d: out: %x\n" , pin, value);
}
    }

// #define GPIOY_bank_bit0_22(bit) (PREG_PAD_GPIO5)
// #define GPIOY_bit_bit0_22(bit) (bit)

    /* GPIO Y */
    printk("# GPIO Y #\n");
    for (pin=0;pin<23;pin++) {
bank = GPIOY_bank_bit0_22(pin);
bit = GPIOY_bit_bit0_22(pin);
mode = get_gpio_mode(bank, bit);
if (mode == GPIO_OUTPUT_MODE) {
value = get_gpio_val(bank, bit);
printk("GPIO Y%d: out: %x\n" , pin, value);
}
    }
}


