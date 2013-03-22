/*
 * Debug register debug driver.
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#define DREG_DEV_NAME		"regd"
#define DREG_CLS_NAME		"register"


/* device driver variables */
static dev_t regd_devt;
static struct class *regd_clsp;
static struct cdev regd_cdev;
static struct device *regd_devp;

/* other variables */




static ssize_t reg_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	/* show help */
}

static ssize_t reg_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{


}

static CLASS_ATTR(reg, S_IWUSR | S_IRUGO, reg_show, reg_store);

static ssize_t bit_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	/* show help */
}

static ssize_t bit_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{

}

static CLASS_ATTR(bit, S_IWUSR | S_IRUGO, bit_show, bit_store);

static ssize_t gamma_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	/* show help */
}

static ssize_t gamma_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{

}

static CLASS_ATTR(gamma, S_IWUSR | S_IRUGO, gamma_show, gamma_store);




static int regd_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int regd_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long regd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
}

static struct file_operations regd_fops = {
	.owner   = THIS_MODULE,
	.open    = regd_open,
	.release = regd_release,
	.ioctl   = regd_ioctl,
};

static int __init regd_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&regd_devt, 0, 1, DREG_DEV_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}

	regd_clsp = class_create(THIS_MODULE, DREG_CLS_NAME);
	if (IS_ERR(regd_clsp)) {
		ret = PTR_ERR(regd_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	cdev_init(&regd_cdev, regd_fops);
	regd_cdev.owner = THIS_MODULE;
	cdev_add(&regd_cdev, regd_devt, 1);

	regd_devp = device_create(regd_clsp, NULL, regd_devt, NULL, DREG_DEV_NAME);
	if (IS_ERR(regd_devp)) {
		ret = PTR_ERR(regd_devp);
		goto fail_create_device;
	}

	class_create_file(regd_clsp, &class_attr_reg);
	class_create_file(regd_clsp, &class_attr_bit);
	class_create_file(regd_clsp, &class_attr_gamma);

	return 0;

fail_create_device:
	cdev_del(&regd_cdev);
	class_destroy(regd_clsp);
fail_class_create:
	unregister_chrdev_region(regd_devt, 1);
fail_alloc_cdev_region:
	return ret;
}


static void __exit regd_exit(void)
{
    device_destroy(regd_devp, regd_devt);
    cdev_del(&regd_cdev);
    class_destroy(regd_clsp);
    unregister_chrdev_region(regd_devt, 1);
}

module_init(regd_init);
module_exit(regd_exit);

MODULE_DESCRIPTION("Amlogic register debug driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");

