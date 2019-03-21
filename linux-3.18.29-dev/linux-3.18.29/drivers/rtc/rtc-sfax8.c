/*
 * Based on rtc-ds1672.c by Alessandro Zummo <a.zummo@towertech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/mfd/sfax8.h>
#include <linux/platform_device.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#define NUM_TIME_REGS           (6)
#define RTC_NAME				"sfax8-rtc"
#define NSEC_PER_SEC	1000000000L
static struct device *m_dev;
struct sfax8_rtc{
	struct sfax8 *sfax8;
	struct rtc_device *rtc;
    int irq;
};

static int sfax8_read_time(struct device *dev, struct rtc_time *tm)
{

	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[NUM_TIME_REGS];
	int ret;
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_WATCH_SEC_REG, buf, NUM_TIME_REGS);
    if(ret) {
		dev_err(dev, "Failed to bulk read rtc_data: %d\n",ret);
		return ret;
	}
	tm->tm_sec  = buf[0];
	tm->tm_min  = buf[1];
	tm->tm_hour = buf[2];
	tm->tm_mday = buf[3];
	tm->tm_wday = (buf[4] & 0x70)>> 4;
	tm->tm_mon  = (buf[4] & 0xf);
	tm->tm_year = (buf[5] & 0x7f) + 100;
	return ret;
}

static int sfax8_set_time(struct device *dev, struct rtc_time *tm)
{

	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[NUM_TIME_REGS];
	int ret;
	buf[0] = (unsigned char)tm->tm_sec;
	buf[1] = (unsigned char)(u8)tm->tm_min;
	buf[2] = (unsigned char)tm->tm_hour;
	buf[3] = (unsigned char)tm->tm_mday;
	buf[4] = (unsigned char)((tm->tm_wday << 4) | tm->tm_mon);
	buf[5] = (unsigned char)(tm->tm_year % 100) & 0x7f;
	dev_dbg(dev, "set RTC date/time %4d-%02d-%02d(%d) %02d:%02d:%02d\n",
		1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_wday, tm->tm_hour , tm->tm_min, tm->tm_sec);
	ret = regmap_bulk_write(sfax8->regmap, SFAX8_IP6103_WATCH_SEC_REG, buf, NUM_TIME_REGS);
	if(ret) {
		dev_err(dev, "Failed to bull write rtc_data: %d\n",ret);
		return ret;
	}
	return 0;

}

static int sfax8_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[NUM_TIME_REGS];
	int ret;
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_ALARM_SEC_REG, buf, NUM_TIME_REGS);
    if(ret) {
		dev_err(dev, "Failed to bulk read rtc_data: %d\n",ret);
		return ret;
	}
	alrm->time.tm_sec  = buf[0];
	alrm->time.tm_min  = buf[1];
	alrm->time.tm_hour = buf[2];
	alrm->time.tm_mday = buf[3];
	alrm->time.tm_mon  = (buf[4] & 0xf);
	alrm->time.tm_year = (buf[5] & 0x7f) + 100;
	return ret;
}

static int sfax8_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[NUM_TIME_REGS];
	int ret;
	buf[0] = (unsigned char)alrm->time.tm_sec;
	buf[1] = (unsigned char)alrm->time.tm_min;
	buf[2] = (unsigned char)alrm->time.tm_hour;
	buf[3] = (unsigned char)alrm->time.tm_mday;
	buf[4] = (unsigned char)alrm->time.tm_mon;
	buf[5] = (unsigned char)(alrm->time.tm_year % 100) & 0x7f;
	dev_dbg(dev, "set RTC date/time %4d-%02d-%02d(%d) %02d:%02d:%02d\n",
		1900 + alrm->time.tm_year, alrm->time.tm_mon + 1, alrm->time.tm_mday,
		alrm->time.tm_wday, alrm->time.tm_hour , alrm->time.tm_min, alrm->time.tm_sec);
	ret = regmap_bulk_write(sfax8->regmap, SFAX8_IP6103_ALARM_SEC_REG, buf, NUM_TIME_REGS);
	if(ret) {
		dev_err(dev, "Failed to bull write rtc_data: %d\n",ret);
		return ret;
	}
	return 0;



}


static const struct rtc_class_ops sfax8_rtc_ops = {
	.read_time = sfax8_read_time,
	.set_time = sfax8_set_time,
	.read_alarm = sfax8_read_alarm,
	.set_alarm = sfax8_set_alarm,
};
/*
* enable alarm power-on the system
*/
static int sfax8_enable_alarm(struct device *dev)
{

	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[1];
	int ret;
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	buf[0] &= 0xfb; //disable vin ok wakeup
	buf[0] |= 0x2;	//enable alarm wakeup
	ret = regmap_bulk_write(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	return ret;
}

/*
* disable alarm power-on system
*/
static int sfax8_disable_alarm(struct device *dev)
{

	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[1];
	int ret;
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	buf[0] &= (~0x2);	//disable alarm wakeup
	ret = regmap_bulk_write(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	return ret;
}


/*
* check alarm power-on is enable or disable
* @return 1 -> alarm is on
*		  0 -> alarm is off
*/
static int sfax8_check_alarm_onoff(struct device *dev)
{
	struct sfax8_rtc *sfax8_rtc = dev_get_drvdata(dev);
	struct sfax8 *sfax8 = sfax8_rtc->sfax8;
	u8 buf[1];
	int ret;
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	buf[0] &= 0x2;
	if(buf[0])
		return 1;
	else
		return 0;
}


/*
* power on by a alarm, so make sure powoff by pmu after this opertion.
* @sec : after n secs, power on the board. if sec > 0, will set a power-on
* alarm, bu if sec == 0, the existed power-on alarm will be cancelled.
* @return : 0 -> set alarm successfully
*			else -> set alarm failed.
*/
static int sfax8_power_on_alarm(int sec)
{
	struct rtc_wkalrm alrm;
	int ret;
	unsigned long time;
	struct sfax8 *sfax8 = dev_get_drvdata(m_dev->parent);

	if(sfax8->type){
		ret = -ENODEV;
		goto err;
	}

	if(sec > 0){
		pr_info("Set system power-on alarm to %u s\n", sec);
		ret = sfax8_read_time(m_dev, &alrm.time);
		if(ret)
			goto err;
		rtc_tm_to_time(&alrm.time, &time);
		time += sec;
		rtc_time_to_tm(time, &alrm.time);

		ret = sfax8_set_alarm(m_dev, &alrm);
		if(ret)
			goto err;
		ret = sfax8_enable_alarm(m_dev);
	}else if(sec == 0){
		if(sfax8_check_alarm_onoff(m_dev)){
			pr_info("Cancel system power-on alarm\n");
			ret = sfax8_disable_alarm(m_dev);
		}else{
			pr_warn("Try to cancel system power-on alarm but no alarm configured!\n");
			ret = 0;
		}
	}
err:
	if(ret)
		pr_err("Config power_on alarm failed with err : %d!\n", ret);
	return ret;
}

static int poweron_show(struct seq_file *m, void *v)
{
	return 0;
}

static int poweron_open(struct inode *inode, struct file *file)
{
	return single_open(file, poweron_show, PDE_DATA(inode));
}

static ssize_t poweron_read(struct file *file, char __user *buffer,
							size_t count, loff_t *f_ops)
{
	char useage[] = "Power on the board after n secs.\n";
	int ret;
	if( *f_ops > 0)
		return 0;
	ret = sizeof(useage);
	if(copy_to_user(buffer, useage, ret))
		ret = -EFAULT;
	else
		*f_ops += ret;
	return ret;
}

static ssize_t poweron_write(struct file *file, const char __user *buffer,
							size_t count, loff_t *f_ops)
{
	unsigned int sec;
	sscanf(buffer, "%u", &sec);
	sfax8_power_on_alarm(sec);
	return count;
}

static struct file_operations poweron_ops = {
	.owner		= THIS_MODULE,
	.open		= poweron_open,
	.read		= poweron_read,
	.write		= poweron_write,
	.release	= single_release,
	.llseek		= seq_lseek,
};
static int sfax8_probe(struct platform_device *pdev)
{
	struct sfax8 *sfax8 = dev_get_drvdata(
			pdev->dev.parent);
	struct sfax8_rtc *sfax8_rtc;
	struct rtc_time tm;
	int ret;
	struct proc_dir_entry *file;
	char buf[1];
	/* There are two types of pmu in sfax8 board.
		but only ip6103 pmu have rtc function. When
		pmu rn5t567 is detected, rtc driver should not
		be registered.
	*/

	if(sfax8->type){
		pr_info("RTC not support by current pmu!\n");
		return -ENODEV;
	}
	sfax8_rtc = devm_kzalloc(&pdev->dev, sizeof(*sfax8_rtc),GFP_KERNEL);
	if(sfax8_rtc == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, sfax8_rtc);
	sfax8_rtc->sfax8 = sfax8;
	ret = sfax8_read_time(&pdev->dev, &tm);
	if(ret) {
		dev_err(&pdev->dev,"Failed to read RTC time\n");
		return ret;
	}
	ret = rtc_valid_tm(&tm);
	if(ret)
		dev_warn(&pdev->dev, "invalid date/time\n");
	device_init_wakeup(&pdev->dev, 1);
	sfax8_rtc->rtc = devm_rtc_device_register(&pdev->dev, RTC_NAME, &sfax8_rtc_ops, THIS_MODULE);
	if(IS_ERR(sfax8_rtc->rtc)){
		ret = PTR_ERR(sfax8_rtc->rtc);
		return ret;
	}
	m_dev = &pdev->dev;
	/*
	** we'd better disable alarm wakeup function in the rtc driver probe,
	** otherwise, when we set the rtc time to a last time, and the alarm time
	** is in furture, then the wakeup conditon will occur again out of our
	** exception.
	*/
	ret = regmap_bulk_read(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	if(ret)
		pr_warn("%s:Can't get alarm info\n", __func__);
	buf[0] |= 0x4; // enable vin ok wake up
	buf[0] &= 0xfd; // disable alarm wake up
	ret = regmap_bulk_write(sfax8->regmap, SFAX8_IP6103_WAKE1_REG, buf, 1);
	if(ret)
		pr_warn("%s:Can't set alarm\n", __func__);

	file = proc_create_data("poweron", 0644, NULL, &poweron_ops, NULL);
	if(!file){
		pr_err("Creat poweron file failed\n");
		return -ENOMEM;
	}

	return 0;
}

static struct platform_driver sfax8_rtc_driver = {
	.driver = {
		   .name = "sfax8-rtc",
	},
	.probe = &sfax8_probe,
};

module_platform_driver(sfax8_rtc_driver);

MODULE_DESCRIPTION("SIFLOWER SFAX8 RTC driver");
MODULE_LICENSE("GPL");
