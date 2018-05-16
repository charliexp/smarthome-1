/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <fcntl.h>      /*文件控制定义*/    
#include <termios.h>    /*PPSIX 终端控制定义*/    
#include <errno.h>      /*错误号定义*/   

#include "tools.h"

char g_current_packetid = 0;

char getpacketid(void)
{
	char packetid;
	packetid = (g_current_packetid++) % 256;

	return packetid;
}

int getmac(char *address)
{
	int fd;
	ssize_t ret;
	fd = open("/sys/class/net/eth0/address", O_RDONLY, S_IRUSR);
	if (fd == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -1;
	}

	ret = read(fd, (void*)address, 20);
	if (ret == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -2;
	}
	address[strlen(address) - 1] = 0;
	close(fd);
	return 0;	
}

/*睡眠msec毫秒*/
void milliseconds_sleep(unsigned long msec) 
{
	if (msec < 0)
		return;
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	
	select(0, NULL, NULL, NULL, &tv);
}


/*******************************************************************  
* 名称：                  init_uart  
* 功能： 初始化串口（一般是初始化UART1）  
* 入口参数：     port :串口号(ttyS0,ttyS1,ttyS2)  
* 出口参数：     正确返回为1，错误返回为0  
*******************************************************************/   
int init_uart(char* port)
{
    struct termios options;
    g_uartfd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);    
    if (g_uartfd < 3)    
    {    
        perror("Can't Open Serial Port");    
        return -1;    
    }    
    //恢复串口为阻塞状态                                   
    if(fcntl(g_uartfd, F_SETFL, 0) < 0)    
    {    
        printf("fcntl failed!\n");    
        return -1;    
    }

    cfsetispeed(&options, B57600);     
    cfsetospeed(&options, B57600); 
    //修改控制模式，保证程序不会占用串口    
    options.c_cflag |= CLOCAL;    
    //修改控制模式，使得能够从串口中读取输入数据    
    options.c_cflag |= CREAD; 
    options.c_cflag &= ~CRTSCTS;//不使用流控制 
    options.c_cflag &= ~PARENB;     
    options.c_iflag &= ~INPCK; 
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    //修改输出模式，原始数据输出    
    options.c_oflag &= ~OPOST;    
    options.c_cflag &= ~CSTOPB;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);       
       
    //设置等待时间和最小接收字符    
    options.c_cc[VTIME] = 1; /* 读取一个字符等待1*(1/10)s */      
    options.c_cc[VMIN] = 1; /* 读取字符的最少个数为1 */    
       
    //如果发生数据溢出，接收数据，但是不再读取 刷新收到的数据但是不读    
    tcflush(g_uartfd,TCIFLUSH);    
       
    //激活配置 (将修改后的termios数据设置到串口中）    
    if (tcsetattr(g_uartfd,TCSANOW,&options) != 0)      
    {    
        perror("uart set error!\n");      
        return -1;     
    }    
    return 0;    
}


