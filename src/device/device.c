#include <stdio.h>
#include <stdlib.h>
#include "device.h"
#include "cJSON.h"
#include <string.h>
#include <fcntl.h> 
#include <termios.h>
#include <unistd.h>
char g_current_packetid = 0;

void zgbmsginit(zigbeemsg msg)
{
	memset((char*)&msg, sizeof(msg), 0);
	msg.header = 0x2A;
	msg.payload.framecontrol[0] = 0x41;
	msg.payload.framecontrol[1] = 0x88;
	msg.payload.cmdid[0] = 0x25;
	msg.payload.cmdid[1] = 0x00;
	msg.footer = 0x23;
}

char getpacketid(void)
{
	return (g_current_packetid++) % 256;
}

int airconditioningcontrol(cJSON *op)
{
	zgbaddress address;
	char* data = "12345678";
	strncpy((char *)&address, data, 8);
	
	sendzgbmsg(address, "abcadf2dafeaafad", 16);
	return 0;
}

int airconcontrol(cJSON *op)
{
	return 0;
}

int sendzgbmsg(zgbaddress address, char *data, char length)
{
	int fd;
	zigbeemsg msg;
	struct termios tio;
	int i = 0;
	int sum = 0;

	zgbmsginit(msg);
	msg.lenght = 39 + length;
	strncpy((char*)msg.payload.dest, (char*)address, 8);
	msg.payload.adf.length = length;
	strncpy((char*)msg.payload.adf.data, (char*)data, length);

	for (;i < msg.lenght; i++)
	{
		sum += (int)((char *)&msg.payload)[i];
	}
	msg.check = sum % 256;

	system("stty -F /dev/ttyS1 speed 57600 cs8 -parenb -cstopb  -echo");
	fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 3)
	{
		perror("error: open ttyS1");
	}

	//if (tcgetattr(fd, &tio) != 0)
	//{
	//	perror("error:SetupSerial 1\n");
	//	return -1;
	//}
	//tio.c_cflag |= CLOCAL | CREAD;
	//tio.c_cflag &= ~CSIZE;
	//cfsetispeed(&tio, B57600);
	//cfsetospeed(&tio, B57600);
	//tcflush(fd, TCIFLUSH);
	//if (tcsetattr(fd, TCSANOW, &tio) != 0)
	//{
	//	perror("com set error\n");
	//	return -1;
	//}
	
	if ( write(fd, (char *)&msg, (int)(msg.lenght + 4)) != (msg.lenght + 4))
	{
		perror("com write error!\n");
		return -1;
	}
	return 0;
}


