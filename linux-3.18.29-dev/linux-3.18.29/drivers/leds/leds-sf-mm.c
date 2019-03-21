/*
 *  siflwer multimedia led
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>


#define LED_COLOR_MASK (LED_RED | LED_GREEN | LED_BLUE)
#define LED_FLASH_MASK (LED_FLASH | LED_SINGLE | LED_COLOR_MASK)
#define LED_ON_MASK (LED_ALWAYS_ON | LED_COLOR_MASK)
#define LED_OFF_MASK (LED_ALWAYS_OFF | LED_COLOR_MASK)
#define LED_MODE_MASK  (LED_FLASH | LED_ALWAYS_ON | LED_ALWAYS_OFF)

enum sf_mm_led_modes{
	LED_FLASH = 1 << 0,
	LED_ALWAYS_ON = 1 << 1,
	LED_ALWAYS_OFF = 1 << 2,
	LED_RED	= 1 << 3,
	LED_GREEN = 1 << 4,
	LED_BLUE = 1 << 5,
	LED_SINGLE = 1 << 6,
};


struct sf_mm_led_data {
	unsigned int delay; //unit ms
	int mode;
	int red_led_gpio;
	int green_led_gpio;
	int blue_led_gpio;
	int count;
	struct timer_list timer;
};

static void sf_mm_led_timer_function(unsigned long data)
{
	struct sf_mm_led_data *led_data = (struct sf_mm_led_data *)data;
	//printk(KERN_DEBUG "%s: mode = 0x%x, count= 0x%x, ms %u, jiffies %lu\n", __func__, led_data->mode, led_data->count, led_data->delay, msecs_to_jiffies(led_data->delay));
	if (!(led_data->mode & LED_MODE_MASK)){
		printk(KERN_ERR "%s : unsupport led mode 0x%x\n", __func__, led_data->mode);
		goto exit;
	}

	gpio_set_value(led_data->red_led_gpio, 1);
	gpio_set_value(led_data->green_led_gpio, 1);
	gpio_set_value(led_data->blue_led_gpio, 1);

	if (led_data->mode & LED_FLASH){
		led_data->mode &= LED_FLASH_MASK;
		if (led_data->count == led_data->mode)
			led_data->count = 0;
		led_data->count |= LED_FLASH;
		// red led
		if ((led_data->mode & LED_RED) && !(led_data->count & LED_RED)){
			led_data->count |= LED_RED;
			gpio_set_value(led_data->red_led_gpio, 0);
			if (!(led_data->mode & LED_SINGLE))
				goto exit;
		} else if (led_data->count & LED_RED){
			if (led_data->mode & LED_SINGLE)
				led_data->count |= LED_SINGLE;
		}
		//green led
		if ((led_data->mode & LED_GREEN) && !(led_data->count & LED_GREEN)){
			led_data->count |= LED_GREEN;
			gpio_set_value(led_data->green_led_gpio, 0);
			if (!(led_data->mode & LED_SINGLE))
				goto exit;
		} else if (led_data->count & LED_GREEN){
			if (led_data->mode & LED_SINGLE)
				led_data->count |= LED_SINGLE;
		}
		//blue led
		if ((led_data->mode & LED_BLUE) && !(led_data->count & LED_BLUE)){
			led_data->count |= LED_BLUE;
			gpio_set_value(led_data->blue_led_gpio, 0);
			if (!(led_data->mode & LED_SINGLE))
				goto exit;
		} else if (led_data->count & LED_BLUE){
			if (led_data->mode & LED_SINGLE)
				led_data->count |= LED_SINGLE;
		}
	}else if (led_data->mode & LED_ALWAYS_ON){
		if (led_data->mode & LED_RED)
			gpio_set_value(led_data->red_led_gpio, 0);
		if (led_data->mode & LED_GREEN)
			gpio_set_value(led_data->green_led_gpio, 0);
		if (led_data->mode & LED_BLUE)
			gpio_set_value(led_data->blue_led_gpio, 0);
		del_timer(&led_data->timer);
		return ;
	}else if (led_data->mode & LED_ALWAYS_OFF){
		//led off
		del_timer(&led_data->timer);
		return ;
	}
exit:
	mod_timer(&led_data->timer, jiffies + msecs_to_jiffies(led_data->delay));

}

static ssize_t sf_mm_led_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf, "useage : echo time(ms) mode > sys/sf-mm-led\n");
	return strlen(buf) + 1;

}

static ssize_t sf_mm_led_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct sf_mm_led_data *led_data = dev_get_drvdata(dev);
	int num, mode, delay;
	//printk(KERN_DEBUG"recieved : %s", buf);
	num = sscanf(buf, "%d %d", &delay, &mode);
	if ( num != 2){
		dev_err(dev, "%s : invalid format\n", __func__);
		return size;
	}
	del_timer_sync(&led_data->timer);
	led_data->delay = delay;
	led_data->mode = mode;
	led_data->count = 0;
	mod_timer(&led_data->timer, jiffies + 1);
	return size;
}


static DEVICE_ATTR(sf_mm_led, 0644, sf_mm_led_show, sf_mm_led_store);

#define led_device_create_file(dev, attr) \
	device_create_file(dev, &dev_attr_ ## attr)
#define led_device_remove_file(dev, attr) \
	device_remove_file(dev, &dev_attr_ ## attr)


static int sf_mm_led_gpio_init(struct device *dev)
{
	int ret = 0;
	struct sf_mm_led_data *led_data = dev_get_drvdata(dev);
	ret = gpio_request(led_data->red_led_gpio, "SF RED GPIO");
	if (ret){
		dev_err(dev, "request red gpio %d failed!\n", led_data->red_led_gpio);
		goto err;
	}
	ret = gpio_direction_output(led_data->red_led_gpio, 1);
	if (ret)
		goto err1;

	ret = gpio_request(led_data->green_led_gpio, "SF GREEN GPIO");
	if (ret){
		dev_err(dev, "request green gpio %d failed!\n", led_data->green_led_gpio);
		goto err1;
	}
	ret = gpio_direction_output(led_data->green_led_gpio, 1);
	if (ret)
		goto err2;

	ret = gpio_request(led_data->blue_led_gpio, "SF BLUE GPIO");
	if (ret){
		dev_err(dev, "request blue gpio %d failed!\n", led_data->blue_led_gpio);
		goto err2;
	}
	ret = gpio_direction_output(led_data->blue_led_gpio, 1);
	if (ret)
		goto err3;

	return 0;
err3:
	gpio_free(led_data->blue_led_gpio);
err2:
	gpio_free(led_data->green_led_gpio);
err1:
	gpio_free(led_data->red_led_gpio);
err:
	return ret;
}

// red-led-gpio
// green-led-gpio
// blue-led-gpio
static int sf_mm_led_probe(struct platform_device *pdev)
{
	struct sf_mm_led_data *led_data;
	int ret;

	led_data = devm_kzalloc(&pdev->dev, sizeof(struct sf_mm_led_data), GFP_KERNEL);
	if (!led_data)
		return -ENOMEM;
	printk("%s entry !\n", __func__);

	if (!pdev->dev.of_node){
		dev_err(&pdev->dev, "%s can get of node info\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	led_data->red_led_gpio = of_get_named_gpio(pdev->dev.of_node, "red-led-gpio", 0);
	if (!gpio_is_valid(led_data->red_led_gpio)){
		dev_err(&pdev->dev, "%s can't get red-led-gpio\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	led_data->green_led_gpio = of_get_named_gpio(pdev->dev.of_node, "green-led-gpio", 0);
	if (!gpio_is_valid(led_data->green_led_gpio)){
		dev_err(&pdev->dev, "%s can't get green-led-gpio\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	led_data->blue_led_gpio = of_get_named_gpio(pdev->dev.of_node, "blue-led-gpio", 0);
	if (!gpio_is_valid(led_data->blue_led_gpio)){
		dev_err(&pdev->dev, "%s can't get blue-led-gpio\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "default-mode",
			&led_data->mode);
	if (ret){
		dev_err(&pdev->dev, "%s can't get led default mode\n", __func__);
		goto err;
	}
	printk("%s : default mode is 0x%x\n", __func__, led_data->mode);

	ret = of_property_read_u32(pdev->dev.of_node, "default-interval",
			&led_data->delay);
	if (ret){
		dev_err(&pdev->dev, "%s can't get led default interval\n", __func__);
		goto err;
	}
	dev_set_drvdata(&pdev->dev, led_data);

	ret = led_device_create_file(&pdev->dev, sf_mm_led);
	if (ret)
		goto err;

	ret = sf_mm_led_gpio_init(&pdev->dev);
	if (ret){
		dev_err(&pdev->dev, "%s, init led failed!\n", __func__);
		goto err1;
	}

	init_timer(&led_data->timer);
	led_data->timer.function = sf_mm_led_timer_function;
	led_data->timer.data = (unsigned long)led_data;
	mod_timer(&led_data->timer, jiffies + 1);

	return 0;
err1:
	led_device_remove_file(&pdev->dev, sf_mm_led);
err:
	devm_kfree(&pdev->dev, led_data);
	return ret;

}

static int sf_mm_led_remove(struct platform_device *pdev)
{
	struct sf_mm_led_data *led_data;
	led_data = dev_get_drvdata(&pdev->dev);
	gpio_free(led_data->red_led_gpio);
	gpio_free(led_data->green_led_gpio);
	gpio_free(led_data->blue_led_gpio);
	del_timer(&led_data->timer);
	led_device_remove_file(&pdev->dev, sf_mm_led);
	return 0;
}

static const struct of_device_id of_mm_leds_match[] = {
	{ .compatible = "sf-mm-led", },
	{},
};

static struct platform_driver sf_mm_led_driver = {
	.driver = {
		.name = "sf_mm_led",
		.owner = THIS_MODULE,
		.of_match_table = of_mm_leds_match,
	},
	.probe = sf_mm_led_probe,
	.remove = sf_mm_led_remove,
};
module_platform_driver(sf_mm_led_driver);

MODULE_DESCRIPTION("siflower multimedia leds");
MODULE_LICENSE("GPL");
