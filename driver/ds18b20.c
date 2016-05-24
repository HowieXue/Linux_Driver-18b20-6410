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
/* ������Ŷ���,�����Ժ���ֲ */
#define DEVICE_NAME "ds18b20"
#define DQ         8
#define CFG_IN     0
#define CFG_OUT    1

// ds18b20�����豸�ţ���̬���䣩
int ds18b20_major = 0;
int ds18b20_minor = 0;
int ds18b20_nr_devs = 1;

// �����豸����
static struct ds18b20_device {
    struct cdev cdev;
};
struct ds18b20_device ds18b20_dev;

static struct class *ds18b20_class;

/* �������� */
static int ds18b20_open(struct inode *inode, struct file *filp);
static int ds18b20_init(void);
static void write_byte(unsigned char data);
static unsigned char read_byte(void);
static ssize_t ds18b20_read(struct file *filp, char __user *buf,
                            size_t count, loff_t *f_pos);
void ds18b20_setup_cdev(struct ds18b20_device *dev, int index);




/*
 ��������: ds18b20_open()
 ��������: ���豸����ʼ��ds18b20
 ��ڲ���: inode:�豸�ļ���Ϣ; filp: ���򿪵��ļ�����Ϣ
 ���ڲ���: �ɹ�ʱ����0,ʧ�ܷ���-1
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
 ��������: ds18b20_init()
 ��������: ��λds18b20
 ��ڲ���: ��
 ���ڲ���: retval:�ɹ�����0,ʧ�ܷ���1
 ��    ע: ����ʱ���ds18b20 datasheet
*/

static int ds18b20_init(void)
{
    int retval = 0;

    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 0);

    s3c6410_gpio_setpin(DQ, 1);
    udelay(2);
    s3c6410_gpio_setpin(DQ, 0);        // ����ds18b20���ߣ���λds18b20
    udelay(500);                       // ���ָ�λ��ƽ500us

    s3c6410_gpio_setpin(DQ, 1);        // �ͷ�ds18b20����
    udelay(60);

    // ����λ�ɹ���ds18b20�����������壨�͵�ƽ������60~240us��
    s3c6410_gpio_cfgpin(DQ, CFG_IN);
    retval = s3c6410_gpio_getpin(DQ);

    udelay(500);
    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 0);
    s3c6410_gpio_setpin(DQ, 1);        // �ͷ�����
    
    return retval;
}





/*
 ��������: write_byte()
 ��������: ��18b20д��һ���ֽ�����
 ��ڲ���: data
 ���ڲ���: ��
*/
static void write_byte(unsigned char data)
{
    int i = 0;

    s3c6410_gpio_cfgpin(DQ, CFG_OUT);
    s3c6410_gpio_pullup(DQ, 1);

    for (i = 0; i < 8; i ++)
    {
        // ���ߴӸ������͵�ƽʱ���Ͳ���дʱ϶
        s3c6410_gpio_setpin(DQ, 1);
        udelay(2);
        s3c6410_gpio_setpin(DQ, 0);
        s3c6410_gpio_setpin(DQ, data & 0x01);
        udelay(60);
	data >>= 1;
    }
    s3c6410_gpio_setpin(DQ, 1);        // �����ͷ�ds18b20����
}





/*
 ��������: read_byte()
 ��������: ��ds18b20����һ���ֽ�����
 ��ڲ���: ��
 ���ڲ���: ����������
*/

static unsigned char read_byte(void)
{
    int i;
    unsigned char data = 0;

    for (i = 0; i < 8; i++)
    {
        // ���ߴӸ������ͣ�ֻ��ά�ֵ͵�ƽ17ts���ٰ��������ߣ��Ͳ�����ʱ϶
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
    s3c6410_gpio_setpin(DQ, 1);        // �ͷ�ds18b20����
    return data;
}





/*
 ��������: ds18b20_read()
&nbsp;��������: ����18b20���¶�
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

    result[0] = read_byte();    // �¶ȵͰ�λ
    result[1] = read_byte();    // �¶ȸ߰�λ
    
    err = copy_to_user(buf, &result, sizeof(result));
    return err ? -EFAULT : min(sizeof(result),count);
}





/*
  �ַ���������ĺ��ģ�Ӧ�ó��������õ�open,read�Ⱥ������ջ�
  ��������ṹ�еĶ�Ӧ����
*/
static struct file_operations ds18b20_dev_fops = {
    .owner = THIS_MODULE,
    .open = ds18b20_open,
    .read = ds18b20_read,
};


/*
 ��������: ds18b20_setup_cdev()
 ��������: ��ʼ��cdev
 ��ڲ���: dev:�豸�ṹ��; index��
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
 ��������: ds18b20_dev_init()
 ��������: Ϊ�¶ȴ���������ע���豸�ţ���ʼ��cdev
 ��ڲ���: ��
 ���ڲ���: ���ɹ�ִ�У�����0
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







 /*��������: ds18b20_dev_exit()
 &nbsp; ��������: ע���豸*/

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