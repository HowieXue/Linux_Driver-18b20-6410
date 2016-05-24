#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <mach/regs-gpio.h>
#include <linux/device.h>
#include <mach/hardware.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/errno.h>

#include "ds18b20.h"

//#define DEBUG
/* 相关引脚定义,方便以后移植 */
#define DEVICE_NAME "ds18b20"
#define DQ         8
#define CFG_IN     0
#define CFG_OUT    1

// ds18b20主次设备号（动态分配）
int ds18b20_major = 0;
int ds18b20_minor = 0;
int ds18b20_nr_devs = 1;

// 定义设备类型
static struct ds18b20_device {
    struct cdev cdev;
};
struct ds18b20_device ds18b20_dev;

static struct class *ds18b20_class;

/* 函数声明 */
static int ds18b20_open(struct inode *inode, struct file *filp);
static int ds18b20_init(void);
static void write_byte(unsigned char data);
static unsigned char read_byte(void);
static ssize_t ds18b20_read(struct file *filp, char __user *buf,
                            size_t count, loff_t *f_pos);
void ds18b20_setup_cdev(struct ds18b20_device *dev, int index);




/*
 函数名称: ds18b20_open()
 函数功能: 打开设备，初始化ds18b20
 入口参数: inode:设备文件信息; filp: 被打开的文件的信息
 出口参数: 成功时返回0,失败返回-1
*/

static int ds18b20_open(struct inode *inode, struct file *filp)
{
    int flag = 0;
    /*struct ds18b20_device *dev;
    dev = container_of(inode->i_cdev, struct ds18b20_device, cdev);
    filp->private_data = dev;*/

    flag = ds18b20_init();
    if(flag & 0x01)
    {
#ifdef DEBUG
        printk(KERN_WARNING "open ds18b20 failed\n");
#endif
	return -1;
    }
#ifdef DEBUG
    printk(KERN_NOTICE "open ds18b20 successful\n");
#endif
    return 0;
}






/*
 函数名称: ds18b20_init()
 函数功能: 复位ds18b20
 入口参数: 无
 出口参数: retval:成功返回0,失败返回1
 备    注: 操作时序见ds18b20 datasheet
*/

static int ds18b20_init(void)
{
    int retval = 0;

    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 0);

    s3c6410_gpio_setpin(DQ, 1);
    udelay(2);
    s3c6410_gpio_setpin(DQ, 0);        // 拉低ds18b20总线，复位ds18b20
    udelay(500);                       // 保持复位电平500us

    s3c6410_gpio_setpin(DQ, 1);        // 释放ds18b20总线
    udelay(60);

    // 若复位成功，ds18b20发出存在脉冲（低电平，持续60~240us）
    s3c6410_gpio_cfgpin(DQ, CFG_IN);
    retval = s3c6410_gpio_getpin(DQ);

    udelay(500);
    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 0);
    s3c6410_gpio_setpin(DQ, 1);        // 释放总线
    
    return retval;
}





/*
 函数名称: write_byte()
 函数功能: 向18b20写入一个字节数据
 入口参数: data
 出口参数: 无
*/
static void write_byte(unsigned char data)
{
    int i = 0;

    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 1);

    for (i = 0; i < 8; i ++)
    {
        // 总线从高拉至低电平时，就产生写时隙
        s3c6410_gpio_setpin(DQ, 1);
        udelay(2);
        s3c6410_gpio_setpin(DQ, 0);
        s3c6410_gpio_setpin(DQ, data & 0x01);
        udelay(60);
	data >>= 1;
    }
    s3c6410_gpio_setpin(DQ, 1);        // 重新释放ds18b20总线
}





/*
 函数名称: read_byte()
 函数功能: 从ds18b20读出一个字节数据
 入口参数: 无
 出口参数: 读出的数据
*/

static unsigned char read_byte(void)
{
    int i;
    unsigned char data = 0;

    for (i = 0; i < 8; i++)
    {
        // 总线从高拉至低，只需维持低电平17ts，再把总线拉高，就产生读时隙
        s3c6410_gpio_cfgpin(DQ, CFG_OUT);
        s3c6410_gpio_pullup(DQ, 0);
        s3c6410_gpio_setpin(DQ, 1);
        udelay(2);
        s3c6410_gpio_setpin(DQ, 0);
        udelay(2);
	s3c6410_gpio_setpin(DQ, 1);
        udelay(8);
        data >>= 1;
	s3c6410_gpio_cfgpin(DQ, CFG_IN);
	if (s3c6410_gpio_getpin(DQ))
	    data |= 0x80;
	udelay(50);
    }
    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 0);
    s3c6410_gpio_setpin(DQ, 1);        // 释放ds18b20总线
    return data;
}





/*
 函数名称: ds18b20_read()
&nbsp;函数功能: 读出18b20的温度
*/
static ssize_t ds18b20_read(struct file *filp, char __user *buf,
                            size_t count, loff_t *f_pos)
{
    int flag;
    unsigned long err;
    unsigned char result[2] = {0x00, 0x00};
    //struct ds18b20_device *dev = filp->private_data;

    flag = ds18b20_init();
    if (flag)
    {
#ifdef DEBUG
        printk(KERN_WARNING "ds18b20 init failed\n");
#endif
        return -1;
    }
    
    write_byte(0xcc);
    write_byte(0x44);

    flag = ds18b20_init();
    if (flag)
        return -1;

    write_byte(0xcc);
    write_byte(0xbe);

    result[0] = read_byte();    // 温度低八位
    result[1] = read_byte();    // 温度高八位
    
    err = copy_to_user(buf, &result, sizeof(result));
    return err ? -EFAULT : min(sizeof(result),count);
}





/*
  字符驱动程序的核心，应用程序所调用的open,read等函数最终会
  调用这个结构中的对应函数
*/
static struct file_operations ds18b20_dev_fops = {
    .owner = THIS_MODULE,
    .open = ds18b20_open,
    .read = ds18b20_read,
};


/*
 函数名称: ds18b20_setup_cdev()
 函数功能: 初始化cdev
 入口参数: dev:设备结构体; index：
*/
void ds18b20_setup_cdev(struct ds18b20_device *dev, int index)
{
    int err, devno = MKDEV(ds18b20_major, ds18b20_minor + index);

    cdev_init(&dev->cdev, &ds18b20_dev_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&(dev->cdev), devno, 1);
    if (err)
    {
#ifdef DEBUG
        printk(KERN_NOTICE "ERROR %d add ds18b20\n", err);
#endif
    }
}






/*
 函数名称: ds18b20_dev_init()
 函数功能: 为温度传感器分配注册设备号，初始化cdev
 入口参数: 无
 出口参数: 若成功执行，返回0
*/

static int __init ds18b20_dev_init(void)
{
    ds18b20_major = register_chrdev(ds18b20_major, DEVICE_NAME, &ds18b20_dev_fops);
    if (ds18b20_major<0)
    {
	printk(DEVICE_NAME " Can't register major number!\n");
	return -EIO;
    }

    ds18b20_class = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(ds18b20_class, NULL, MKDEV(ds18b20_major, ds18b20_minor), NULL, DEVICE_NAME);
#ifdef DEBUG
	printk(KERN_WARNING "register ds18b20 driver successful!\n");
#endif
    return 0;
}







 /*函数名称: ds18b20_dev_exit()
 &nbsp; 函数功能: 注销设备*/

static void __exit ds18b20_dev_exit(void)
{
    device_destroy(ds18b20_class, MKDEV(ds18b20_major,ds18b20_minor));
    class_unregister(ds18b20_class);
    class_destroy(ds18b20_class);
    unregister_chrdev(ds18b20_major, DEVICE_NAME);
#ifdef DEBUG
	printk(KERN_WARNING "Exit ds18b20 driver!\n");
#endif
}

module_init(ds18b20_dev_init);
module_exit(ds18b20_dev_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Howie");