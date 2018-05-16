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
#include <fcntl.h>      /*�ļ����ƶ���*/    
#include <termios.h>    /*PPSIX �ն˿��ƶ���*/    
#include <errno.h>      /*����Ŷ���*/   

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

/*˯��msec����*/
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
* ���ƣ�                  init_uart  
* ���ܣ� ��ʼ�����ڣ�һ���ǳ�ʼ��UART1��  
* ��ڲ�����     port :���ں�(ttyS0,ttyS1,ttyS2)  
* ���ڲ�����     ��ȷ����Ϊ1�����󷵻�Ϊ0  
*******************************************************************/   
int init_uart(char* port)
{
    struct termios options;
    int fd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);    
    if (false == fd)    
    {    
        perror("Can't Open Serial Port");    
        return 0;    
    }    
    //�ָ�����Ϊ����״̬                                   
    if(fcntl(fd, F_SETFL, 0) < 0)    
    {    
        printf("fcntl failed!\n");    
        return 0;    
    }

    cfsetispeed(&options, B57600);     
    cfsetospeed(&options, B57600); 
    //�޸Ŀ���ģʽ����֤���򲻻�ռ�ô���    
    options.c_cflag |= CLOCAL;    
    //�޸Ŀ���ģʽ��ʹ���ܹ��Ӵ����ж�ȡ��������    
    options.c_cflag |= CREAD; 
    options.c_cflag &= ~CRTSCTS;//��ʹ�������� 
    options.c_cflag &= ~PARENB;     
    options.c_iflag &= ~INPCK; 
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    //�޸����ģʽ��ԭʼ�������    
    options.c_oflag &= ~OPOST;    
    options.c_cflag &= ~CSTOPB;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);       
       
    //���õȴ�ʱ�����С�����ַ�    
    options.c_cc[VTIME] = 1; /* ��ȡһ���ַ��ȴ�1*(1/10)s */      
    options.c_cc[VMIN] = 1; /* ��ȡ�ַ������ٸ���Ϊ1 */    
       
    //�����������������������ݣ����ǲ��ٶ�ȡ ˢ���յ������ݵ��ǲ���    
    tcflush(fd,TCIFLUSH);    
       
    //�������� (���޸ĺ��termios�������õ������У�    
    if (tcsetattr(fd,TCSANOW,&options) != 0)      
    {    
        perror("uart set error!\n");      
        return (FALSE);     
    }    
    return (TRUE);    
}


