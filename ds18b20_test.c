#include<stdio.h>
#include <fcntl.h>
void main(void)
{
	int fd ;
	char buffer[2];
	fd = open("/dev/ds18b20",O_RDONLY);
	while(1)
	{
		if(read(fd,buffer,2)>=0)
		{
			printf("The current temperature is %.2f\n",((buffer[1]<<8)|buffer[0])*0.0625);
		}
		else
		{
			printf("open file  failure\n");
		}
		sleep(1);
	}
	close(fd);
}

