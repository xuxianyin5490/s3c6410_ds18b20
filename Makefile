ifneq ($(KERNELRELEASE),)

obj-m := ds18b20.o

else
		
KDIR := /home/xuxianyin/workplace/linux-2.6.32.63
all:
		make -C $(KDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-
clean:
		rm -f *.ko *.o *.mod.o *.mod.c *.symvers  modul*

endif
