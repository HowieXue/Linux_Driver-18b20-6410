#!/bin/sh
export PATH=/opt/FriendlyARM/toolschain/4.5.1/bin:$PATH

# 1 build kernel driver module
# make CROSS_COMPILE=arm-linux- ARCH=arm
cd driver
make CROSS_COMPILE=arm-linux- ARCH=arm modules

# 2 build ds18b20 test app
cd ../app
arm-linux-gcc test_ds18b20.c -o test_ds18b20

# 3 build ds18b20 read app
arm-linux-gcc read_ds18b20.c -o read_ds18b20