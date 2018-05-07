#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <termios.h>
#include <unistd.h>

#include "device.h"

void zgbmsginit(zgbmsg *msg)
{
	memset((char*)msg, 0, sizeof(zigbeemsg));
	msg->header = 0x2A;
	msg->payload.framecontrol[0] = 0x41;
	msg->payload.framecontrol[1] = 0x88;
	msg->payload.cmdid[0] = 0x25;
	msg->payload.cmdid[1] = 0x00;
	msg->footer = 0x23;
}

int airconcontrol(cJSON *device, char packetid)
{
	zgbaddress address;
	char data[69] = {0};
	char devicetype;
	char op;
	char* destaddress;
	destaddress = cJSON_GetObjectItem(device, "address")->valuestring;
	strncpy((char *)&address, destaddress, 8);
	devicetype = 0x02;
	op = cJSON_GetObjectItem(device, "operation")->valueint;

	data[0] = 0x11;
	data[1] = devicetype;
	data[2] = 0x23;
	data[3] = op;
	
	sendzgbmsg(address, data, packetid);
	return 0;
}

int freshaircontrol(cJSON *device, char packetid)
{
	return 0;
}

int socketcontrol(cJSON *device, char packetid)
{
	zgbaddress address;
	char data[69] = { 0 };
	char devicetype;
	char op;
	char* destaddress;
	destaddress = cJSON_GetObjectItem(device, "address")->valuestring;
	strncpy((char *)&address, destaddress, 8);
	devicetype = 0x02;
	op = cJSON_GetObjectItem(device, "operation")->valueint;

	data[0] = 0x11;
	data[1] = devicetype;
	data[2] = ZGB_SOCKET_CONTROL;
	data[3] = op;

	sendzgbmsg(address, data, packetid);
	return 0;
}

int sendzgbmsg(zgbaddress address, char *data, char packetid)
{
	int fd;
	zigbeemsg msg;
	struct termios tio;
	int i = 0;
	int sum = 0;

	zgbmsginit(&msg);
	if (strlen(data) > 69)
	{
		printf("msg data is too big!\n");
		return -1;
	}
	msg.msglength = 38 + strlen(data); //40为msg除了data以及head、length、check、footer的长度

	strncpy((char*)msg.payload.dest, (char*)address, 8);//目标地址赋值
	msg.payload.adf.length = strlen(data) + 3; //data + version + packetid + cmdid
	msg.payload.adf.index[0] = 0xA0;
	msg.payload.adf.index[1] = 0x0F;
	msg.payload.adf.length = strlen(data) + 3;
	msg.payload.adf.devmsg.version = 0x10;
	msg.payload.adf.devmsg.packetid = packetid;
	msg.payload.adf.devmsg.devicecmdid = 0x03;
	strncpy((char*)msg.payload.adf.devmsg.data, (char*)data, strlen(data));

	for (;i < msg.msglength; i++)
	{
		sum += (int)((char *)&msg.payload)[i];
	}
	msg.check = sum % 256;

	fd = open("/dev/ttyS1", O_WRONLY | O_NOCTTY | O_NDELAY);
	if (fd < 3)
	{
		perror("error: open ttyS1");
	}

	if ( write(fd, (char *)&msg, (int)(msg.msglength + 2)) != (msg.msglength + 2))
	{
		perror("com write error!\n");
		return -1;
	}
	if (write(fd, (char *)&msg+sizeof(msg)-2, 2) != 2)//发送check和footer
	{
		perror("com write check and footer error!\n");
		return -1;
	}
	return 0;
}


