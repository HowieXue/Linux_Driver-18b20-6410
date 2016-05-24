#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/ioctl.h>

// 函数声明
//void ds18b20_delay(int i);

int main()
{
    int fd, i;
    unsigned char result[2];           // 从ds18b20读出的结果，result[0]存放低八位
    unsigned char integer_value = 0;
    float temperature, decimal_value;  // 温度数值,decimal_value为小数部分的值
    fd = open("/dev/ds18b20", 0);
    if (fd < 0)
    {
        perror("open device failed\n");
	exit(1);
    }
    while (1)
    {
        usleep(10*1000*1000); // 10s
		i++;
        read(fd, &result, sizeof(result));
        integer_value = ((result[0] & 0xf0) >> 4) | ((result[1] & 0x07) << 4);
        // 精确到0.25度
        decimal_value = 0.5 * ((result[0] & 0x0f) >> 3) + 0.25 * ((result[0] & 0x07) >> 2);
        temperature = (float)integer_value + decimal_value;
	    printf("The temperature is %6.2f\n",temperature);
	    if (i % 20 == 0)
	        printf("\n");
	    
    }
	
}


