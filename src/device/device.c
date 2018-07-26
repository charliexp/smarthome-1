#include <stdio.h>
#include <string.h>
#include <fcntl.h> 
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sqlite3.h> 

#include "../utils/utils.h"
#include "device.h"
#include "../log/log.h"

extern int g_queueid;
extern cJSON* g_devices_status_json;
extern sqlite3* g_db;

void zgbaddresstodbaddress(ZGBADDRESS addr, char* db_address)
{
	int i = 0;

	for (; i < sizeof(ZGBADDRESS); i++)
	{
		if (addr[i] / 16 < 10)
		{
			db_address[2*i] = addr[i] / 16 + 48;
		}
		else
		{
			db_address[2*i] = addr[i] / 16 + 55;
		}

		if (addr[i] % 16 < 10)
		{
			db_address[2*i + 1] = addr[i] % 16 + 48;
		}
		else
		{
			db_address[2*i + 1] = addr[i] % 16 + 55;
		}

	}
	return;
}

void dbaddresstozgbaddress(char* db_address, ZGBADDRESS addr)
{
	int i = 0;

	for (; i < 16; i = i + 2)
	{
		if (db_address[i] < 58)
		{
			addr[i / 2] = db_address[i] - 48;
		}
		else
		{
			addr[i / 2] = db_address[i] - 55;
		}

		if (db_address[i + 1] < 58)
		{
			addr[i / 2] = addr[i / 2] * 16 + db_address[i + 1] - 48;
		}
		else
		{
			addr[i / 2] = addr[i / 2] * 16 + db_address[i + 1] - 55;
		}
	}

	return;
}



void zgbmsginit(zgbmsg *msg)
{
	memset((char*)msg, 0, sizeof(zgbmsg));
	msg->header = 0x2A;
	msg->payload.framecontrol[0] = 0x41;
	msg->payload.framecontrol[1] = 0x88;
	msg->footer = 0x23;
}

void deviceneedregister(ZGBADDRESS addr)
{
    
}

void devices_status_query()
{
    ZGBADDRESS address = {0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF}; //广播报文
    sendzgbmsg(address, NULL, 0, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, 0);
}

int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid)
{
	int ret;
    zgbmsg msg;
    uartsendqueuemsg uartmsg;
	int i = 0;
	int sum = 0;

    MYLOG_INFO("Send a zgbmsg!The data is:");
    MYLOG_BYTE(data, length);
    
    zgbmsginit(&msg);
    msg.msglength = 46 + length;
	memcpy((char*)msg.payload.dest, (char*)address, 8);//目标地址赋值
	msg.payload.cmdid[0] = 0x25;
    msg.payload.cmdid[1] = 0x38;
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
		sum += ((BYTE *)&msg.payload)[i];
	}
    MYLOG_DEBUG("The sum is %d", sum);
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


void devices_status_json_init()
{
    int nrow = 0, ncolumn = 0;
	char **dbresult;     
    char sql[]={"select deviceid, devicetype from devices;"};
    char* zErrMsg = NULL;
    cJSON *device_status_json;
    char* deviceid;
    char devicetype;

    g_devices_status_json = cJSON_CreateArray();

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("There are not any device!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
        return;
    }

    for(int i=1; i< nrow; i++)
    {
        deviceid      = dbresult[i*ncolumn];
        devicetype    = *(dbresult[i*ncolumn+1]) - '0';
        device_status_json = create_device_status_json(deviceid, devicetype);
        MYLOG_INFO("The device_status is %s", cJSON_PrintUnformatted(device_status_json));
        cJSON_AddItemToArray(g_devices_status_json, device_status_json);
    }
    devices_status_query();
    sqlite3_free_table(dbresult);
}


cJSON* create_device_status_json(char* deviceid, char devicetype)
{
	cJSON* device = cJSON_CreateObject();
	cJSON* statusarray = cJSON_CreateArray();
    cJSON* status;

    cJSON_AddStringToObject(device, "deviceid", deviceid);
	cJSON_AddItemToObject(device, "status", statusarray);

    switch(devicetype)
    {
        case DEV_SOCKET:
        {
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_WORKING_STATUS);
        	cJSON_AddNullToObject(status, "value");
        	cJSON_AddItemToArray(statusarray, status);
            break;
        }
        case DEV_AIR_CON:
        {
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_WORKING_STATUS);
        	cJSON_AddNullToObject(status, "value");
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_WINDSPEED);
        	cJSON_AddNullToObject(status, "value");
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_TEMPERATURE);
        	cJSON_AddNullToObject(status, "value");
        	cJSON_AddItemToArray(statusarray, status);
            break;
        }
        default:
            device = NULL;
            
    }
    
	return device;    
}


cJSON* get_device_status_json(char* deviceid, char devicetype)
{
    int devicenum;
    cJSON* devicestatus = NULL;
    char* array_deviceid;

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        if(memcmp(deviceid, array_deviceid, 9) == 0)
        {
            return devicestatus;
        }
    }

    return devicestatus;
}

cJSON* get_attr_value_object_json(cJSON* device, char attrtype)
{
    cJSON* status,*attr;
    int arraynum = 0;
    char type;

    status = cJSON_GetObjectItem(device, "status");
    if(status == NULL)
    {
        MYLOG_ERROR("get_attr_value_object_json error, status is null");
        return NULL;
    }
    arraynum = cJSON_GetArraySize(status);

    for (int i=0; i < arraynum; i++)
    {
        attr = cJSON_GetArrayItem(status, i);
        type = cJSON_GetObjectItem(attr, "type")->valuestring[0];
        if(type == attrtype)
        {
            return attr;
        }
    }
    MYLOG_ERROR("Cannot find the attr");
    return NULL;
}
