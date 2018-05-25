#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>

#include "../utils/utils.h"
#include "device.h"
#include "../log/log.h"

extern int g_queueid;



unsigned long long zgbaddresstodbaddress(ZGBADDRESS addr)
{
    unsigned long long dbaddress = 0;
    dbaddress = (unsigned long long)addr[0] + (unsigned long long)addr[1] * 256 + (unsigned long long)addr[2] * 256 * 256 + 
        (unsigned long long)addr[3] * 256 * 256 * 256 + 
        (unsigned long long)addr[4] * 256 * 256 * 256 * 256 + 
        (unsigned long long)addr[5] * 256 * 256 * 256 * 256 * 256 + 
        (unsigned long long)addr[6] * 256 * 256 * 256 * 256 * 256 * 256 + 
        (unsigned long long)addr[7] * 256 * 256 * 256 * 256 * 256 * 256 * 256;
    return dbaddress;
}


void zgbmsginit(zgbmsg *msg)
{
	memset((char*)msg, 0, sizeof(zgbmsg));
	msg->header = 0x2A;
	msg->payload.framecontrol[0] = 0x41;
	msg->payload.framecontrol[1] = 0x88;
	msg->payload.cmdid[0] = 0x25;
	msg->payload.cmdid[1] = 0x00;
    msg->payload.adf.data.magicnum = 0xAA;
	msg->footer = 0x23;
}

void deviceneedregister(ZGBADDRESS addr)
{
    
}

int airconcontrol(cJSON *device, char packetid)
{
	ZGBADDRESS address;
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
	
	sendzgbmsg(address, data, 0x04, 0x03, 0x01, 0x01, packetid);
	return 0;
}

int freshaircontrol(cJSON *device, char packetid)
{
	return 0;
}

int socketcontrol(cJSON *device, char packetid)
{
	ZGBADDRESS address;
	BYTE data[65] = { 0 };
	char devicetype;
	char op;
	char* destaddress;
	destaddress = cJSON_GetObjectItem(device, "address")->valuestring;
	strncpy((char *)&address, destaddress, 8);
	devicetype = 0x02;
	op = cJSON_GetObjectItem(device, "operation")->valueint;

	data[0] = TLV_TYPE_SOCKET_STATUS;
	data[1] = 0x01;
	data[2] = TLV_VALUE_SOCKET_ON;
	data[3] = op;

	sendzgbmsg(address, data, 0x04, 0x03, 0x01, 0x01, packetid);
	return 0;
}

int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid)
{
	int ret;
    zgbmsg msg;
    uartsendqueuemsg uartmsg;
	int i = 0;
	int sum = 0;

    zgbmsginit(&msg);
    msg.msglength = 46 + length;
	memcpy((char*)msg.payload.dest, (char*)address, 8);//目标地址赋值
	msg.payload.adf.index[0] = 0xA0;
	msg.payload.adf.index[1] = 0x0F;
	msg.payload.adf.length = length + 7;
    msg.payload.adf.data.length = length;
	msg.payload.adf.data.version = ZGB_VERSION_10;
	msg.payload.adf.data.packetid = packetid;
	msg.payload.adf.data.msgtype = msgtype;
    msg.payload.adf.data.devicetype = devicetype;
    msg.payload.adf.data.deviceindex = deviceindex;
	memcpy((char*)msg.payload.adf.data.pdu, (char*)data, length);

	for (;i < msg.msglength; i++)
	{
		sum += (int)((char *)&msg.payload)[i];
	}
	msg.check = sum % 256;

    uartmsg.msgtype = QUEUE_MSG_UART;
    uartmsg.msg = msg;

    if (ret = msgsnd(g_queueid, &uartmsg, sizeof(uartmsg), 0) != 0)
    {
		MYLOG_ERROR("send uartsendqueuemsg fail!");
        return -1;
	}

	return 0;
}

