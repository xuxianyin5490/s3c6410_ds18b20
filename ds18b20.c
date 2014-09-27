#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <asm/delay.h>

#define DEVICE_NAME "ds18b20"

#define GPECON 0x7F008080
#define GPEDAT 0x7F008084
#define GPEPUD 0x7F008088

static volatile long int *gpecon;
static volatile long int *gpedat;
static volatile long int *gpepud;

static int ds18b20_reset(void)
{
	int count = 0;
	iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
	iowrite32((ioread32(gpecon) | 0x01),gpecon);
	//复位电平保持500us
	iowrite16((ioread16(gpedat)&0xfffe),gpedat);		
	udelay(500);
	//再将总线拉高,等待ds1820的回应
	iowrite16((ioread16(gpedat)|0x01),gpedat);		
	udelay(20);
	//将IO口定义为输入
	iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
	count  = 0;
	//等待ds18b20将总线拉低
	while(1)
	{
		udelay(20);
		if(!(ioread16(gpedat)&0x01))
		{
			break;
		}
		count ++;
		printk("count:%d\n",count);
		if(count >= 4)
		{
			return -1;
		}
	}
	//等待ds18b20将总线拉高
	while(1)
	{
		udelay(50);
		if((ioread16(gpedat)&0x01))
		{
			break;
		}
		count ++;
		if(count >= 5)
		{
			return -1;
		}
	}
	iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
	iowrite32((ioread32(gpecon) | 0x01),gpecon);
	//最后再将总线拉高
	iowrite16((ioread16(gpedat)|0x01),gpedat);		
	udelay(405);
	return 0;
}

static unsigned char ds18b20_readbyte(void)
{
	unsigned char value=0;
	unsigned char i=0;
	for(i=0;i<8;i++)
	{
		iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
		iowrite32((ioread32(gpecon) | 0x01),gpecon);
		//先将总线拉低
		iowrite16((ioread16(gpedat)&0xfffe),gpedat);		
		udelay(1);
		iowrite16((ioread16(gpedat)|0x01),gpedat);		
		iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
		if((ioread16(gpedat)&0x01))
		{
			value |= 1<<i;
		}
		udelay(50);
	}
	return value;
}

static void ds18b20_writebyte(unsigned char value)
{
	unsigned char i=0;
	iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
	iowrite32((ioread32(gpecon) | 0x01),gpecon);
	for(i=0;i<8;i++)
	{
		//先将总线拉低
		iowrite16((ioread16(gpedat)&0xfffe),gpedat);		
		udelay(1);
		if(value&0x01)
		{
			iowrite16((ioread16(gpedat)|0x01),gpedat);		
		}
		else
		{
			iowrite16((ioread16(gpedat)&0xfffe),gpedat);		
		}
		//udelay(45);
		udelay(45);
		iowrite16((ioread16(gpedat)|0x01),gpedat);		
		udelay(1);
		value = (value >> 1);
	}
}


static ssize_t ds18b20_read(struct file *file, char __user *user, size_t size, loff_t *loff)
{
	unsigned char temp1,temp2;
	if(ds18b20_reset()<0)
	{
		printk("ds18b20_reset failure!\n");
	}
	udelay(120);
	ds18b20_writebyte(0xcc);
	ds18b20_writebyte(0x44);
	udelay(5);
	if(ds18b20_reset()<0)
	{
		printk("ds18b20_reset failure!\n");
	}
	udelay(200);
	ds18b20_writebyte(0xcc);
	ds18b20_writebyte(0xbe);
	temp1 = ds18b20_readbyte();
	temp2 = ds18b20_readbyte();
	if(ds18b20_reset()<0)
	{
		printk("ds18b20_reset failure!\n");
	}
	user[0] = temp1;
	user[1] = temp2;
	return 0;
}

static int ds18b20_open(struct inode *inode, struct file *file)
{
	iowrite32((ioread32(gpecon) & 0xFFFFFFF0),gpecon);
	iowrite32((ioread32(gpecon) | 0x1),gpecon);
	return 0;
}

static struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.read = &ds18b20_read,
	.open = &ds18b20_open,
};

static struct miscdevice md = 
{
	.name = DEVICE_NAME,
	.fops = &fops,
	.minor = MISC_DYNAMIC_MINOR,
};

static int __init ds18b20_init(void)
{
	int ret=0;
	if(!request_mem_region(GPECON,12,"s3c6410_gpe"))
	{
		ret = -EBUSY;
		printk("s3c6410_gpe request_mem_failed\n");
		goto request_mem_failed;
	}
	gpecon = ioremap(GPECON,4);
	if(gpecon==NULL)
	{
		ret = -EBUSY;
		printk("s3c6410_gpe gpecon failed\n");
		goto gpecon_failed;
	}
	gpedat = ioremap(GPEDAT,4);
	if(gpedat==NULL)
	{
		ret = -EBUSY;
		printk("s3c6410_gpe gpedat failed\n");
		goto gpedat_failed;
	}
	gpepud = ioremap(GPEPUD,4);
	if(gpepud==NULL)
	{
		ret = -EBUSY;
		printk("s3c6410_gpe gpepub failed\n");
		goto gpepud_failed;
	}
	ret = misc_register(&md);
	if(ret<0)
	{
		return -EBUSY;
		goto misc_regster_failed;
	}
	printk("s3c6410 ds18b20 initialize\n");
	return ret;
misc_regster_failed:
	misc_deregister(&md);
gpepud_failed:
	iounmap(gpepud);
gpedat_failed:
	iounmap(gpedat);
gpecon_failed:
	iounmap(gpecon);
request_mem_failed:
	release_mem_region(GPECON,12);
	return ret;
}

static void __exit ds18b20_exit(void)
{
	iounmap(gpepud);
	iounmap(gpedat);
	iounmap(gpecon);
	release_mem_region(GPECON,12);
	misc_deregister(&md);
}

module_init(ds18b20_init);
module_exit(ds18b20_exit);

MODULE_AUTHOR("Xu Xianyin");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ds18b20 driver");
