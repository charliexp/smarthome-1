#include <stdio.h>
#include <stdlib.h>
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
extern int g_system_mode;

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

void device_closeallfan()
{
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    BYTE data[5] = {ATTR_DEVICESTATUS, 0x0, 0x0, 0x0, TLV_VALUE_POWER_OFF};
    sendzgbmsg(address, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_FAN_COIL, 0xFF, getpacketid());    
}

void change_panel_mode(int mode)
{
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    BYTE data[5] = {ATTR_SYSMODE, 0x0, 0x0, 0x0, mode};
    sendzgbmsg(address, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_CONTROL_PANEL, 0xFF, getpacketid());        
}

void devices_status_query()
{
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    sendzgbmsg(address, NULL, 0, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, getpacketid());
}

int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid)
{
	int ret;
    zgbmsg msg;
    uartsendqueuemsg uartmsg;
	int i = 0;
	int sum = 0;
    
    zgbmsginit(&msg);
    msg.msglength = 42 + length;
	memcpy((char*)msg.payload.dest, (char*)address, 8);//目标地址赋值
	msg.payload.cmdid[0] = 0x25;
    msg.payload.cmdid[1] = 0x00;
	msg.payload.adf.index[0] = 0xA0;
	msg.payload.adf.index[1] = 0x0F;
	msg.payload.adf.length = length + 7;
	msg.payload.adf.data.magicnum = 0xAA;
    msg.payload.adf.data.length = length + 7;
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
    //MYLOG_DEBUG("The sum is %d", sum);
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

/*针对同一类设备发送zgb消息*/
void sendzgbmsgfordevices(BYTE devicetype, BYTE *data, char length, char msgtype)
{
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    sendzgbmsg(address, data, length, msgtype, devicetype, 0, getpacketid());    
}

/*内存中维护设备状态的json表*/
void devices_status_json_init()
{
    size_t nrow = 0, ncolumn = 0;
	char **dbresult;     
    char sql[]={"select deviceid, devicetype from devices;"};
    char* zErrMsg = NULL;
    cJSON *device_status_json;
    char* deviceid;
    size_t devicetype;

    g_devices_status_json = cJSON_CreateArray();

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("There are not any device!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
        return;
    }

    for(int i=1; i<= nrow; i++)
    {
        deviceid      = dbresult[i*ncolumn];
        devicetype    = atoi(dbresult[i*ncolumn+1]);
        device_status_json = create_device_status_json(deviceid, devicetype);     
        cJSON_AddItemToArray(g_devices_status_json, device_status_json);
    }
    MYLOG_INFO("The g_device_status is %s", cJSON_PrintUnformatted(g_devices_status_json));
    devices_status_query();
    sqlite3_free_table(dbresult);
}

/*创建一个设备的json状态指针*/
cJSON* create_device_status_json(char* deviceid, char devicetype)
{
	cJSON* device = cJSON_CreateObject();
	cJSON* statusarray = cJSON_CreateArray();
    cJSON* status;

    cJSON_AddStringToObject(device, "deviceid", deviceid);
	cJSON_AddItemToObject(device, "status", statusarray);
	cJSON_AddItemToObject(device, "online", cJSON_CreateNumber(0));	

    switch(devicetype)
    {
        case DEV_GATEWAY:
        {
	        status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_GATEWAY);
        	cJSON_AddItemToArray(statusarray, status); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SYSMODE);
        	int mode = get_gateway_mode();
        	g_system_mode = mode;
        	change_panel_mode(mode);
        	cJSON_AddNumberToObject(status, "value", mode);
        	cJSON_AddItemToArray(statusarray, status);     	
            break;            
        }
        case DEV_SOCKET:
        {
	        status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_SOCKET);
        	cJSON_AddItemToArray(statusarray, status); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_V);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);       	
            break;
        }
        case DEV_AIR_CON:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_AIR_CON);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_AIR_CONDITION_MODE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);           	
            break;
        }        
        case SEN_ENV_BOX:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", SEN_ENV_BOX);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_PM25);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_CO2);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_FORMALDEHYDE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);                	
            break;
        }
        case DEV_CONTROL_PANEL: //控制面板
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_CONTROL_PANEL);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 20);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 50);
        	cJSON_AddItemToArray(statusarray, status); 
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_SYSMODE);
        	cJSON_AddNumberToObject(status, "value", TLV_VALUE_COND_COLD);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_CONNECTED_AIRCONDITON);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;
        }  
        case DEV_BOILER:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_BOILER);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;
        }
        case SEN_WATER_FLOW:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", SEN_WATER_FLOW);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;        
        }
        case SEN_WIND_PRESSURE:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", SEN_WIND_PRESSURE);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_V);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;        
        }
        case DEV_FAN_COIL:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_FAN_COIL);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WINDSPEED);
        	cJSON_AddNumberToObject(status, "value", 2);
        	cJSON_AddItemToArray(statusarray, status); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SETTING_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 26);
        	cJSON_AddItemToArray(statusarray, status);           	        	
            break;            
        }
        case DEV_FRESH_AIR:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_FRESH_AIR);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WINDSPEED);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);        	        	
            break;            
        }        
        case DEV_FLOOR_HEAT:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_FLOOR_HEAT);
        	cJSON_AddItemToArray(statusarray, status); 	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 1);
        	cJSON_AddItemToArray(statusarray, status); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SETTING_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 26);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;            
        }
        case SEN_WATER_TEMPERATURE:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", SEN_WATER_TEMPERATURE);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);           	
            break;            
        }
        case DEV_HUMIDIFIER:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_HUMIDIFIER);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);         	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WINDSPEED);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);        	
            break;            
        }
        case DEV_DEHUMIDIFIER:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_DEHUMIDIFIER);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WINDSPEED);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);        	
            break;            
        }        
        case DEV_WATER_PURIF:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_WATER_PURIF);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WATERPRESSURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);           	
            break;               
        }
        //热水器
        case DEV_WATER_HEAT:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_WATER_HEAT);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WATER_TEMPERATURE_TARGET);
        	cJSON_AddNumberToObject(status, "value", 30);
        	cJSON_AddItemToArray(statusarray, status);     
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WATER_TEMPERATURE_CURRENT);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);    
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WATER_TEMPERATURE_CLEAN);
        	cJSON_AddNumberToObject(status, "value", 80);
        	cJSON_AddItemToArray(statusarray, status);    
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WATER_TIME);
        	cJSON_AddNumberToObject(status, "value", 60);
        	cJSON_AddItemToArray(statusarray, status);            	
            break;               
        }
        case DEV_WATER_PUMP:
        {
	        status = cJSON_CreateObject(); 
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICETYPE);
        	cJSON_AddNumberToObject(status, "value", DEV_WATER_PUMP);
        	cJSON_AddItemToArray(statusarray, status); 	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_CONNECTED_SOCKET);
        	cJSON_AddStringToObject(status, "value", "");
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_CONNECTED_WATER_SEN);
        	cJSON_AddStringToObject(status, "value", "");
        	cJSON_AddItemToArray(statusarray, status);           	
            break;               
        }        
        default:
            device = NULL;
            
    }
    
	return device;    
}


cJSON* get_device_status_json(char* deviceid)
{
    int devicenum;
    cJSON* devicestatus = NULL;
    char* array_deviceid;

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        if(strncmp(deviceid, array_deviceid, 17) == 0)
        {
            return cJSON_Duplicate(devicestatus, 1);
        }
    }

    return NULL;
}

cJSON* get_attr_value_object_json(cJSON* device, char attrtype)
{
    cJSON* status,*attr, *temp;
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
        temp = cJSON_GetObjectItem(attr, "type");
        if(temp != NULL)
        {
            type = temp->valueint;
            if(type == attrtype)
            {
                return attr;
            }        
        }

    }
    MYLOG_ERROR("Cannot find the attr,the attr is %d", attrtype);
    return NULL;
}

void change_device_attr_value(char* deviceid, char attr, int value)
{
    int devicenum;
    cJSON* devicestatus = NULL;
    cJSON* attr_json = NULL;
    cJSON* replace_value_json = cJSON_CreateNumber(value);
    char* array_deviceid;

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        if(strncmp(deviceid, array_deviceid, 17) == 0)
        {
            attr_json = get_attr_value_object_json(devicestatus, attr); 
            cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);            
        }
    }    
}

/* MQTT的操作数据转为ZGB的DATA */
int mqtttozgb(cJSON* op, BYTE* zgbdata, int devicetype)
{
    if(!op)
        return 0;
    if(!zgbdata)
        return 0;

    cJSON* item=NULL;
    int attr=0;
    int value=0;
    int index=0;
    char* name = NULL;
    int opnum = cJSON_GetArraySize(op);
    MYLOG_INFO("the operationnum = %d", opnum);
    MYLOG_INFO("Begin to trans mqtt to zgb");

    for(int i=0 ; i<opnum; i++)
    {
        item = cJSON_GetArrayItem(op, i);
        attr = cJSON_GetObjectItem(item, "type")->valueint;
        
        if(attr != ATTR_DEVICENAME)
            value = cJSON_GetObjectItem(item, "value")->valueint;
        else
            name = cJSON_GetObjectItem(item, "value")->valuestring;

        /*在此添加联动操作*/
        
        switch (attr)
        {
            case ATTR_DEVICESTATUS://如果做数据检查在每个枚举下面进行
            case ATTR_AIR_CONDITION_MODE:
            case ATTR_WINDSPEED:
            case ATTR_WINDSPEED_NUM:
            case ATTR_SETTING_TEMPERATURE:
            {
                int j = 4;
                int attrvalue = value;
                zgbdata[index] = attr;
                while(j)
                {                    
                    zgbdata[index+j] = attrvalue%256;
                    attrvalue = attrvalue/256; 
                    j--;
                }
                index = index + 5;
                break;
            }

            case ATTR_DEVICENAME:
                zgbdata[index++] = attr;
                memcpy(zgbdata+index, name, strlen(name));
                index += strlen(name);
                zgbdata[index++] = '\0';
                break;
            default:
                break;
        }
    }

    return index--;
}

void gatewayproc(cJSON* op)
{
    cJSON* item=NULL;
    int attr=0;
    int value=0;
    int opnum = cJSON_GetArraySize(op);
    cJSON* device_json = get_device_status_json(GATEWAY_ID);
    for(int i=0 ; i<opnum; i++)
    {
        item = cJSON_GetArrayItem(op, i);
        attr = cJSON_GetObjectItem(item, "type")->valueint;
        if(attr == ATTR_SYSMODE)
        {
            value = cJSON_GetObjectItem(item, "value")->valueint;
            if(value == TLV_VALUE_COND_HEAT || value == TLV_VALUE_COND_COLD || value == TLV_VALUE_BOILER_HEAT)
            {
                if(g_system_mode != value)
                {
                    change_device_attr_value(GATEWAY_ID, attr, value);                
                    set_gateway_mode(value);
                    change_panel_mode(value);
                    device_closeallfan();
                }
            }
        }
    }
}


void change_devices_offline()
{
    int devicenum, online;
    cJSON* devicestatus = NULL;
    char* array_deviceid;
    cJSON* offline = cJSON_CreateNumber(0);

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        online = cJSON_GetObjectItem(devicestatus, "online")->valueint;
        if(online == 1)
        {
            cJSON_GetObjectItem(devicestatus, "online")->valueint = TLV_VALUE_OFFLINE;
            cJSON_GetObjectItem(devicestatus, "online")->valuedouble = TLV_VALUE_OFFLINE;
        }
    }    
}

void change_device_online(char* deviceid, char status)
{
    int devicenum;
    cJSON* devicestatus = NULL;
    char* array_deviceid;
    cJSON* offline = cJSON_CreateNumber(status);

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        if(strncmp(deviceid, array_deviceid, 17) == 0)
        {
            cJSON_GetObjectItem(devicestatus, "online")->valueint = status;
            cJSON_GetObjectItem(devicestatus, "online")->valuedouble = status;
            return;
        }
    }      
}

int check_device_online(char* deviceid)
{
    int devicenum;
    cJSON* devicestatus = NULL;
    char* array_deviceid;

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        if(strncmp(deviceid, array_deviceid, 17) == 0)
        {            
            return cJSON_GetObjectItem(devicestatus, "online")->valueint;
        }
    }

    return -1;
}

int get_gateway_mode()
{
    size_t nrow = 0, ncolumn = 0;
	char **dbresult;     
    char sql[]={"select mode from gatewaycfg;"};
    char* zErrMsg = NULL;
    
    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("Can not get gatewaycfg mode!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
        return -1;
    }

    int mode = atoi(dbresult[1]);

    return mode;
}

void set_gateway_mode(int mode)
{
    char sql[100];
    g_system_mode = value;
    memset(sql, 0, 100);
    sprintf(sql, "replace into gatewaycfg(rowid, mode) values(1, %d)", mode);
    exec_sql_create(sql);
}

void change_system_mode(int mode){
    char topic[TOPIC_LENGTH] = {0};   
    cJSON* device = get_device_status_json(GATEWAY_ID);
    cJSON* attr = get_attr_value_object_json(device, ATTR_SYSMODE);
    
    sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_STATUS);

    set_gateway_mode(int mode);
    change_device_attr_value(GATEWAY_ID, ATTR_SYSMODE, mode);
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(device), 0, 0);
}

