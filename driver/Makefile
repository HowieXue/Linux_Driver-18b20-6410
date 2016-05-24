KERNELDIR :=/home/share/linux-2.6.38
PWD       := $(shell pwd)
	
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	
obj-m	:= ds18b20.o

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
