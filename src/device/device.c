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
#include "../mqtt.h"

extern int g_queueid,g_zgbqueueid;
extern cJSON* g_devices_status_json;
extern sqlite3* g_db;
extern int g_system_mode;
extern char g_topicroot[20];
extern char g_boilerid[20];
extern char g_hotwatersystem_socket[20];
extern char g_hotwatersystem_temperaturesensor[20];
extern int g_hotwatersystem_settingtemperature;

extern pthread_mutex_t g_devices_status_mutex;

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

void change_panels_mode(int mode)
{
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    BYTE data[5] = {ATTR_SYSMODE, 0x0, 0x0, 0x0, mode};
    sendzgbmsg(address, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_CONTROL_PANEL, 0x0, getpacketid());        
}

void change_panel_mode(char* deviceid, int mode)
{
    ZGBADDRESS address;
	MYLOG_INFO("Change panel mode: %s-> %d", deviceid, mode);
	dbaddresstozgbaddress(deviceid, address);
    BYTE data[5] = {ATTR_SYSMODE, 0x0, 0x0, 0x0, mode};
    sendzgbmsg(address, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_CONTROL_PANEL, 0x0, getpacketid());        
}

void change_boiler_mode(int mode)
{   
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文 
    BYTE data[5] = {ATTR_DEVICESTATUS, 0x0, 0x0, 0x0, mode};
    sendzgbmsg(address, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_BOILER, 0x0, getpacketid());    
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
    
	msg.check = sum % 256;

    uartmsg.msgtype = QUEUE_MSG_UART;
    uartmsg.msg = msg;

    if (ret = msgsnd(g_zgbqueueid, &uartmsg, sizeof(uartmsg), 0) != 0)
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

    pthread_mutex_lock(&g_devices_status_mutex);
    g_devices_status_json = cJSON_CreateArray();
    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("There are not any device!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
        pthread_mutex_unlock(&g_devices_status_mutex);
		
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return;
    }

    for(int i=1; i<= nrow; i++)
    {
        deviceid      = dbresult[i*ncolumn];
        devicetype    = atoi(dbresult[i*ncolumn+1]);
        device_status_json = create_device_status_json(deviceid, devicetype);     
        cJSON_AddItemToArray(g_devices_status_json, device_status_json);
    }
    pthread_mutex_unlock(&g_devices_status_mutex);
    sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
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
	cJSON_AddItemToObject(device, "online-check", cJSON_CreateNumber(0));	

    switch(devicetype)
    {
        case DEV_GATEWAY:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_GATEWAY);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SYSMODE);
        	int mode = get_gateway_mode();
        	g_system_mode = mode;
        	cJSON_AddNumberToObject(status, "value", mode);
        	cJSON_AddItemToArray(statusarray, status);     	
            status = cJSON_CreateObject();
            cJSON_AddNumberToObject(status, "type", ATTR_SYSTEM_BOILER);
            int ret = get_system_boiler(g_boilerid);
            if(ret == -1)
            {
                cJSON_AddStringToObject(status, "value", "");
            }
            else
            {
                cJSON_AddStringToObject(status, "value", g_boilerid);
            }
            cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_TEMPERATURE_TARGET);
            ret = get_hotwatersystem_targettemperature();
            if(ret == -1)
            {
                cJSON_AddNumberToObject(status, "value", HOTWATER_DEFAULTTEMPERATURE);
            }
            else
            {
                cJSON_AddNumberToObject(status, "value", (double)g_hotwatersystem_settingtemperature);
            }			
        	cJSON_AddItemToArray(statusarray, status);     
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_TEMPERATURE_CURRENT);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);    
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_TEMPERATURE_CLEAN);
        	cJSON_AddNumberToObject(status, "value", 80);
        	cJSON_AddItemToArray(statusarray, status);    
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_TIME);
        	cJSON_AddNumberToObject(status, "value", 60);
        	cJSON_AddItemToArray(statusarray, status);
            status = cJSON_CreateObject();
            cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_CONNECTED_SOCKET);
            ret = get_hotwatersystem_socket(g_hotwatersystem_socket);
            if(ret == -1)
            {
                cJSON_AddStringToObject(status, "value", "");
            }
            else
            {
                cJSON_AddStringToObject(status, "value", g_hotwatersystem_socket);
            }
            cJSON_AddItemToArray(statusarray, status);
            status = cJSON_CreateObject();
            cJSON_AddNumberToObject(status, "type", ATTR_HOTWATER_CONNECTED_TEMPERATURE_SEN);
            ret = get_hotwatersystem_temperaturesensor(g_hotwatersystem_temperaturesensor);
            if(ret == -1)
            {
                cJSON_AddStringToObject(status, "value", "");
            }
            else
            {
                cJSON_AddStringToObject(status, "value", g_hotwatersystem_temperaturesensor);
            }
            cJSON_AddItemToArray(statusarray, status); 
                break;            
            }
        case DEV_SOCKET:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_SOCKET);
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
        case DEV_LIGHT:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_LIGHT);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);       	
            break;
        }		
        case SEN_ELECTRICITY_METER:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_ELECTRICITY_METER);
        	status = cJSON_CreateObject();
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	break;
        }
        case DEV_AIR_CON:
        {    	
        	cJSON_AddNumberToObject(device, "devicetype", DEV_AIR_CON);	 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
	        status = cJSON_CreateObject();            
        	cJSON_AddNumberToObject(status, "type", ATTR_AIR_CONDITION_MODE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);        	
            break;
        }        
        case SEN_ENV_BOX:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_ENV_BOX);	 
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
        	cJSON_AddNumberToObject(device, "devicetype", DEV_CONTROL_PANEL);	
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
        case SEN_WATER_FLOW:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_WATER_FLOW); 
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_DEVICESTATUS);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WATER_YIELD);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);        	
            break;        
        }
        case SEN_WIND_PRESSURE:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_WIND_PRESSURE);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WINDPRESSURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);          	
            break;        
        }
        case DEV_FAN_COIL:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_FAN_COIL); 	 
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
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);        	
            break;            
        }
        case DEV_FRESH_AIR:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_FRESH_AIR);	 
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
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SOCKET_E);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);        	
            break;            
        }        
        case DEV_FLOOR_HEAT:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_FLOOR_HEAT);	 
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
        	cJSON_AddNumberToObject(device, "devicetype", SEN_WATER_TEMPERATURE);	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_ENV_TEMPERATURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);           	
            break;            
        }
        case DEV_HUMIDIFIER:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_HUMIDIFIER);
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
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SETTING_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);         	
            break;            
        }
        case DEV_DEHUMIDIFIER:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_DEHUMIDIFIER); 	
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
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SETTING_HUMIDITY);
        	cJSON_AddNumberToObject(status, "value", 0);        	
        	cJSON_AddItemToArray(statusarray, status);         	
            break;            
        }        
        case DEV_WATER_PURIF:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_WATER_PURIF);	
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WATERPRESSURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);           	
            break;               
        }
        //热水器
        case DEV_WATER_HEAT:
        {
        	cJSON_AddNumberToObject(device, "devicetype", DEV_WATER_HEAT);	
        	status = cJSON_CreateObject();       	
            break;               
        }
        case SEN_ANEMOGRAPH:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_ANEMOGRAPH);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WINDSPEED);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
			break;
        }
        case SEN_WATER_IMMERSION:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_WATER_IMMERSION);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_WARNING_FINDING_WATER);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
			break;
        }
        case SEN_WATER_PRESSURE:
        {
        	cJSON_AddNumberToObject(device, "devicetype", SEN_WATER_PRESSURE);
        	status = cJSON_CreateObject();
        	cJSON_AddNumberToObject(status, "type", ATTR_SEN_WATERPRESSURE);
        	cJSON_AddNumberToObject(status, "value", 0);
        	cJSON_AddItemToArray(statusarray, status);
			break;
        }		
        default:
            device = NULL;
            
    }
    
	return device;    
}


cJSON* dup_device_status_json(char* deviceid)
{
    int devicenum,length;
    cJSON* devicestatus = NULL;
    char* array_deviceid;

	if(deviceid == NULL)
	{
		return NULL;
	}

    pthread_mutex_lock(&g_devices_status_mutex);
    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        length = (strlen(array_deviceid) > strlen(deviceid))?strlen(array_deviceid):strlen(deviceid);
        if(strncmp(deviceid, array_deviceid, length) == 0)
        {
            cJSON *dev = cJSON_Duplicate(devicestatus, 1);
            cJSON_DeleteItemFromObject(dev, "online-check");
            pthread_mutex_unlock(&g_devices_status_mutex);			
            return dev;
        }
    }
    pthread_mutex_unlock(&g_devices_status_mutex);
    return NULL;
}

cJSON* get_device_status_json(char* deviceid)
{
    int devicenum,length;
    cJSON* devicestatus = NULL;
    char* array_deviceid;

    pthread_mutex_lock(&g_devices_status_mutex);
    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        length = (strlen(array_deviceid) > strlen(deviceid))?strlen(array_deviceid):strlen(deviceid);
        if(strncmp(deviceid, array_deviceid, length) == 0)
        {
            pthread_mutex_unlock(&g_devices_status_mutex);
            return devicestatus;
        }
    }
    pthread_mutex_unlock(&g_devices_status_mutex);
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
    return NULL;
}

void change_device_attr_value(char* deviceid, char attr, int value)
{
    int devicenum,length;
    cJSON* devicestatus = NULL;
    cJSON* attr_json = NULL;
    cJSON* replace_value_json = cJSON_CreateNumber(value);
    char* array_deviceid;

    pthread_mutex_lock(&g_devices_status_mutex);
    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        length = (strlen(array_deviceid) > strlen(deviceid))?strlen(array_deviceid):strlen(deviceid);
        if(strncmp(deviceid, array_deviceid, length) == 0)
        {
            attr_json = get_attr_value_object_json(devicestatus, attr);
            if(attr_json != NULL){
                cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);                
            }
			else
			{
				cJSON_Delete(replace_value_json);
			}
            pthread_mutex_unlock(&g_devices_status_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_devices_status_mutex);
}

/* MQTT的操作数据转为ZGB的DATA */
int mqtttozgb(cJSON* op, BYTE* zgbdata, int devicetype, Operationlog* log)
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

    for(int i=0; i<opnum; i++)
    {
        item = cJSON_GetArrayItem(op, i);
        attr = cJSON_GetObjectItem(item, "type")->valueint;
        
        if(attr != ATTR_DEVICENAME)
        {
			log->deviceattr = attr;		
            value = cJSON_GetObjectItem(item, "value")->valueint;			        
			log->deviceattrvalue = value;
        }
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
            case ATTR_SYSMODE:
            case ATTR_SETTING_HUMIDITY:
            case ATTR_SOCKET_E:
            case ATTR_SOCKET_WORKTIME:
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

int gatewayproc(cJSON* op, Operationlog* log)
{
    cJSON* item,* temp;
    int attr=0;
    int value=0;
    int opnum = cJSON_GetArraySize(op);

    for(int i=0 ; i<opnum; i++)
    {
        item = cJSON_GetArrayItem(op, i);
        attr = cJSON_GetObjectItem(item, "type")->valueint;
		log->deviceattr = attr;
        if(attr == ATTR_SYSMODE)
        {
            temp = cJSON_GetObjectItem(item, "value");
            if(temp == NULL)
            {
                return MQTT_MSG_ERRORCODE_FORMATERROR;
            }
            value = temp->valueint;
			log->deviceattrvalue = value;
            if(value == TLV_VALUE_COND_HEAT || value == TLV_VALUE_COND_COLD || value == TLV_VALUE_BOILER_HEAT)
            {
                if(g_system_mode != value)
                {          
                    change_system_mode(value);                   
                }
            }
        }
        else if(attr == ATTR_SYSTEM_BOILER)
        {
            temp = cJSON_GetObjectItem(item, "value");
            if(temp == NULL)
            {
                return MQTT_MSG_ERRORCODE_FORMATERROR;
            }
            char *id = temp->valuestring;
			if(strcmp(id,"") != 0)
			{
	            temp = get_device_status_json(id);
	            if(temp == NULL)
	            {
	                return MQTT_MSG_ERRORCODE_DEVICENOEXIST;
	            }
	            int type = cJSON_GetObjectItem(temp, "devicetype")->valueint;
	            if(type != DEV_SOCKET)
	            {
	                return MQTT_MSG_ERRORCODE_FORMATERROR;
	            }				
			}

            change_system_boiler(id);
        }
		else if(attr == ATTR_HOTWATER_CONNECTED_SOCKET)
		{
            temp = cJSON_GetObjectItem(item, "value");
            if(temp == NULL)
            {
                return MQTT_MSG_ERRORCODE_FORMATERROR;
            }
            char *id = temp->valuestring;
			if(strcmp(id,"") != 0)
			{
	            temp = get_device_status_json(id);
	            if(temp == NULL)
	            {
	                return MQTT_MSG_ERRORCODE_DEVICENOEXIST;
	            }
	            int type = cJSON_GetObjectItem(temp, "devicetype")->valueint;
	            if(type != DEV_SOCKET)
	            {
	                return MQTT_MSG_ERRORCODE_FORMATERROR;
	            }				
			}

            change_hotwatersystem_socket(id);			
		}
		else if(attr == ATTR_HOTWATER_CONNECTED_TEMPERATURE_SEN)
		{
            temp = cJSON_GetObjectItem(item, "value");
            if(temp == NULL)
            {
                return MQTT_MSG_ERRORCODE_FORMATERROR;
            }
            char *id = temp->valuestring;
			if(strcmp(id,"") != 0)
			{
	            temp = get_device_status_json(id);
	            if(temp == NULL)
	            {
	                return MQTT_MSG_ERRORCODE_DEVICENOEXIST;
	            }
	            int type = cJSON_GetObjectItem(temp, "devicetype")->valueint;
	            if(type != SEN_WATER_TEMPERATURE)
	            {
	                return MQTT_MSG_ERRORCODE_FORMATERROR;
	            }				
			}		

            change_hotwatersystem_temperaturesensor(id);			
		}
		else
		{
			cJSON* dev = get_device_status_json(GATEWAY_ID);
			cJSON* attr_json = get_attr_value_object_json(dev, attr);
            temp = cJSON_GetObjectItem(item, "value");
			
            if(temp == NULL)
            {
                return MQTT_MSG_ERRORCODE_FORMATERROR;
            }
			
            value = temp->valueint;
			log->deviceattrvalue = value;
			if(attr == ATTR_HOTWATER_TEMPERATURE_TARGET){
				set_hotwatersystem_targettemperature(value);
			}

			cJSON* replace_value_json = cJSON_CreateNumber(value);
			cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);		
		}

    }
    return MQTT_MSG_ERRORCODE_SUCCESS;
}


void change_devices_offline()
{
    int devicenum, online, onlinecheck;
    cJSON* devicestatus = NULL;

    pthread_mutex_lock(&g_devices_status_mutex);
    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        onlinecheck = cJSON_GetObjectItem(devicestatus, "online-check")->valueint;
        online = cJSON_GetObjectItem(devicestatus, "online")->valueint;
        if(online != onlinecheck)
        {
            cJSON_GetObjectItem(devicestatus, "online")->valueint = onlinecheck;
            cJSON_GetObjectItem(devicestatus, "online")->valuedouble = onlinecheck;
            report_device_status(devicestatus);
        }
        if(onlinecheck == TLV_VALUE_ONLINE)
        {
            cJSON_GetObjectItem(devicestatus, "online-check")->valueint = TLV_VALUE_OFFLINE;
            cJSON_GetObjectItem(devicestatus, "online-check")->valuedouble = TLV_VALUE_OFFLINE;
        }
    } 
    pthread_mutex_unlock(&g_devices_status_mutex);
}

void change_device_online(char* deviceid, char status)
{
    int devicenum,length;
    cJSON* devicestatus = NULL;
    char* array_deviceid;
    pthread_mutex_lock(&g_devices_status_mutex);
    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        length = (strlen(deviceid)>strlen(array_deviceid)?strlen(deviceid):strlen(array_deviceid));
        if(strncmp(deviceid, array_deviceid, length) == 0)
        {
            cJSON_GetObjectItem(devicestatus, "online-check")->valueint = status;
            cJSON_GetObjectItem(devicestatus, "online-check")->valuedouble = status;
            pthread_mutex_unlock(&g_devices_status_mutex);
            return;
        }
    } 
    pthread_mutex_unlock(&g_devices_status_mutex);
}

int check_device_online(char* deviceid)
{
    int devicenum,length;
    cJSON* devicestatus,* tmp;
    char* array_deviceid;

    devicenum = cJSON_GetArraySize(g_devices_status_json);

    for (int i=0; i < devicenum; i++)
    {
        devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
        array_deviceid = cJSON_GetObjectItem(devicestatus, "deviceid")->valuestring;
        length = (strlen(array_deviceid) > strlen(deviceid))?strlen(array_deviceid):strlen(deviceid);
        if(strncmp(deviceid, array_deviceid, length) == 0)
        {   
            tmp = cJSON_GetObjectItem(devicestatus, "online");
            if(tmp != NULL){
                return tmp->valueint;
            }
            return -1;
        }
    }
    
    return -1;
}

/*上报设备状态*/
void report_device_status(cJSON* stat)
{   
    char topic[TOPIC_LENGTH] = {0};
    
    sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_STATUS);
    cJSON* dev = cJSON_Duplicate(stat, 1);
    cJSON_DeleteItemFromObject(dev, "online-check");
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(dev), 0, 0);
    cJSON_Delete(dev);
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
		sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return TLV_VALUE_COND_COLD;
    }

    int mode = atoi(dbresult[1]);
	sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
    return mode;
}

void set_gateway_mode(int mode)
{
    char sql[100];
    g_system_mode = mode;
    memset(sql, 0, 100);
    sprintf(sql, "update gatewaycfg set mode = %d where rowid =1;", mode);
    exec_sql_create(sql);
}

void change_system_mode(int mode)
{
	MYLOG_INFO("Change systemmode: %d", mode);
    char topic[TOPIC_LENGTH] = {0};
    sprintf(topic, "%sdevices/status", g_topicroot);

    set_gateway_mode(mode);
    change_panels_mode(mode);
    change_device_attr_value(GATEWAY_ID, ATTR_SYSMODE, mode);

    if(mode == TLV_VALUE_BOILER_HEAT)
    {
        change_boiler_mode(TLV_VALUE_POWER_ON);
    }else{
        change_boiler_mode(TLV_VALUE_POWER_OFF);
    }
    
    cJSON* device = get_device_status_json(GATEWAY_ID);
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(device), 0, 0);
    device_closeallfan();
}

/*获取系统配置的锅炉插座id*/
int get_system_boiler(char* id)
{
    size_t nrow = 0, ncolumn = 0;
    char **dbresult;
    char sql[]= {"select boilerid from gatewaycfg;"};
    char* zErrMsg = NULL;

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("Can not get system boiler!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
		sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return -1;
    }

    memcpy(id, dbresult[1], strlen(dbresult[1]));
	sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
    return 0;
}

/*设置数据库中系统配置的锅炉插座id*/
void set_system_boiler(char* id)
{
    char sql[100] = {0};
    sprintf(sql, "update gatewaycfg set boilerid='%s' where rowid =1;", id);
	memset(g_boilerid, 0, sizeof(g_boilerid));
	memcpy(g_boilerid, id, strlen(id));
    exec_sql_create(sql);
}

/*改变系统配置的锅炉插座id*/
void change_system_boiler(char* id)
{
    cJSON* dev = get_device_status_json(GATEWAY_ID);
    cJSON* devid = get_attr_value_object_json(dev, ATTR_SYSTEM_BOILER);
    cJSON* boiler = cJSON_CreateString(id);
    cJSON_ReplaceItemInObject(devid, "value", boiler);
    set_system_boiler(id);
}

int get_hotwatersystem_socket(char* id)
{
    size_t nrow = 0, ncolumn = 0;
    char **dbresult;
    char sql[]= {"select hotwatersocketid from gatewaycfg;"};
    char* zErrMsg = NULL;

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("Can not get hotwatersystem socket!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
		sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return -1;
    }

    memcpy(id, dbresult[1], strlen(dbresult[1]));
	sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
    return 0;
}

void set_hotwatersystem_targettemperature(int temperature)
{
    char sql[100] = {0};
    sprintf(sql, "update gatewaycfg set hotwatertargettemperature='%d' where rowid =1;", temperature);
    exec_sql_create(sql);
	g_hotwatersystem_settingtemperature = temperature;
}

int get_hotwatersystem_targettemperature()
{
    size_t nrow = 0, ncolumn = 0;
    char **dbresult;
    char sql[]= {"select hotwatertargettemperature from gatewaycfg;"};
    char* zErrMsg = NULL;

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("Can not get hotwatersensorid!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
		sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return -1;
    }

	if(dbresult[1][0]>='0' && dbresult[1][0] <='9')
	{
		g_hotwatersystem_settingtemperature = atoi(dbresult[1]);		
	}
	else
	{
		g_hotwatersystem_settingtemperature = HOTWATER_DEFAULTTEMPERATURE;
	}

	sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
    return 0;
}


void set_hotwatersystem_socket(char* id)
{
    char sql[100] = {0};
    sprintf(sql, "update gatewaycfg set hotwatersocketid='%s' where rowid =1;", id);
	memset(g_hotwatersystem_socket, 0, sizeof(g_hotwatersystem_socket));
	memcpy(g_hotwatersystem_socket, id, strlen(id));
    exec_sql_create(sql);
}

void change_hotwatersystem_socket(char* id)
{
    cJSON* dev = get_device_status_json(GATEWAY_ID);
    cJSON* devid = get_attr_value_object_json(dev, ATTR_HOTWATER_CONNECTED_SOCKET);
    cJSON* socket = cJSON_CreateString(id);
    cJSON_ReplaceItemInObject(devid, "value", socket);
    set_hotwatersystem_socket(id);

}

int get_hotwatersystem_temperaturesensor(char* id)
{
    size_t nrow = 0, ncolumn = 0;
    char **dbresult;
    char sql[]= {"select hotwatersensorid from gatewaycfg;"};
    char* zErrMsg = NULL;

    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        MYLOG_DEBUG("Can not get hotwatersensorid!");
        MYLOG_DEBUG("The zErrMsg is %s", zErrMsg);
		sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        return -1;
    }

    memcpy(id, dbresult[1], strlen(dbresult[1]));
	sqlite3_free_table(dbresult);
	sqlite3_free(zErrMsg);
    return 0;
}


void set_hotwatersystem_temperaturesensor(char* id)
{
    char sql[100] = {0};
    sprintf(sql, "update gatewaycfg set hotwatersensorid='%s' where rowid =1;", id);
	memset(g_hotwatersystem_temperaturesensor, 0, sizeof(g_hotwatersystem_temperaturesensor));
	memcpy(g_hotwatersystem_temperaturesensor, id, strlen(id));
    exec_sql_create(sql);
}

void change_hotwatersystem_temperaturesensor(char* id)
{
    cJSON* dev = get_device_status_json(GATEWAY_ID);
    cJSON* devid = get_attr_value_object_json(dev, ATTR_HOTWATER_CONNECTED_TEMPERATURE_SEN);
    cJSON* sensor = cJSON_CreateString(id);
    cJSON_ReplaceItemInObject(devid, "value", sensor);
    set_hotwatersystem_temperaturesensor(id);

}


/*电量查询*/
int electricity_query(cJSON* root,char* topic)
{
    MYLOG_INFO("An electric qury!");
    int devicetype;
    cJSON* t = cJSON_GetObjectItem(root, "operation");        
    if(t == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;    	        
    }
    int type = t->valueint;
    cJSON* devices = cJSON_GetObjectItem(root, "devices");
    if(devices == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }
    int num = cJSON_GetArraySize(devices);
    if(num == 0)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }	    
    for(int i=0;i<num;i++){
        cJSON* device = cJSON_GetArrayItem(devices, i);
        cJSON* deviceidjson = cJSON_GetObjectItem(device, "deviceid");
        cJSON_AddItemToObject(device, "result", cJSON_CreateNumber(0));
        cJSON* records = cJSON_CreateArray();
	    if(deviceidjson == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            return -1;	        
	    }	        
	    char* deviceid = deviceidjson->valuestring;
	    char sql[250]={0};   
        int nrow = 0, ncolumn = 0;
        char **dbresult;
        char *zErrMsg = NULL;

        sprintf(sql, "select devicetype from devices where deviceid='%s';", deviceid);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        if(nrow == 0)
        {
            MYLOG_DEBUG("Can not find the device in devices");
            cJSON_AddItemToObject(device, "records", records);
            sqlite3_free_table(dbresult);
			sqlite3_free(zErrMsg);
            continue;
        }

        devicetype = atoi(dbresult[1]);         
        cJSON_AddItemToObject(device, "devicetype", cJSON_CreateNumber(devicetype));
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        
        switch(type)
        {
            case OP_TYPE_HOUR:
                sprintf(sql, "select electricity,hour from electricity_hour where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_DAY:
                sprintf(sql, "select electricity,day from electricity_day where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_MONTH:
                sprintf(sql, "select electricity,month from electricity_month where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_YEAR:
                sprintf(sql, "select electricity,year from electricity_year where deviceid='%s';", deviceid);
                break;
            default:
                break;
        }    
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        
        cJSON* record;
        int num;
        int data;
        time_t time_now;
        struct tm* t;
        time(&time_now);
        t = localtime(&time_now);
        int day   = t->tm_mday;
    	int month = t->tm_mon + 1; //localtime获取的month范围 0-11
    	int year = t->tm_year;
    	
        for(int i=1;i<=nrow;i++)
        {
            int recordflag = 1;//判断该条记录是否需要上报
            record = cJSON_CreateObject();
            num = atoi((const char*)dbresult[i*2]);
            data = atoi((const char*)dbresult[i*2+1]);
            
            switch (type)
            {
                case OP_TYPE_HOUR:
                    cJSON_AddNumberToObject(record, "hour", data);
                    break;
                case OP_TYPE_DAY:
                    if(day < data && data >= 29)
                    {
                        if(data == 29 && (month-1) == 2)
                        {
                            if(!isLeapYear(year)) 
                                recordflag = 0;
                        }
                        else if((data == 30 || data == 31) && (month-1) == 2)
                        {
                            recordflag == 0;
                        }                            
                        else if(((month -1) == 4 || (month -1) == 6 || (month -1) == 9 || (month -1) == 11) && data == 31)
                        {
                            recordflag == 0;
                        }                        
                    }

                    if(recordflag)
                    {
                        cJSON_AddNumberToObject(record, "day", data);                            
                    }
                    break;                
                case OP_TYPE_MONTH:
                    cJSON_AddNumberToObject(record, "month", data);
                    break;                
                case OP_TYPE_YEAR:
                    cJSON_AddNumberToObject(record, "year", data);
                    break;
                default:					
                    break;                
             
            }
            if(recordflag)
            {
                cJSON_AddNumberToObject(record, "electricity", num); 
                cJSON_AddItemToArray(records, record); 
            }
                     
        }
        cJSON_AddItemToObject(device, "records", records);       
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
    }
    cJSON_AddItemToObject(root, "resultcode", cJSON_CreateNumber(0));
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
	return 0;    
}


/*水量查询*/
int wateryield_query(cJSON* root,char* topic)
{
    MYLOG_INFO("An wateryield qury!");
    int devicetype;
    cJSON* t = cJSON_GetObjectItem(root, "operation");        
    if(t == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;    	        
    }
    int type = t->valueint;
    cJSON* devices = cJSON_GetObjectItem(root, "devices");
    if(devices == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }
    int num = cJSON_GetArraySize(devices);
    if(num == 0)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }	    
    for(int i=0;i<num;i++){
        cJSON* device = cJSON_GetArrayItem(devices, i);
        cJSON* deviceidjson = cJSON_GetObjectItem(device, "deviceid");
        cJSON* records = cJSON_CreateArray();
        cJSON* record;    
	    char* deviceid = deviceidjson->valuestring;
	    char sql[250]={0};   
        int nrow = 0, ncolumn = 0;
        char **dbresult;
        char *zErrMsg = NULL;
        int num;
        int data;
        time_t time_now;
        struct tm* t;        
        
        cJSON_AddItemToObject(device, "result", cJSON_CreateNumber(0));
	    if(deviceidjson == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            return -1;	        
	    }	        
       
        sprintf(sql, "select devicetype from devices where deviceid='%s';", deviceid);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        if(nrow == 0)
        {
            MYLOG_DEBUG("Can not find the device in devices");
	        sqlite3_free_table(dbresult);
			sqlite3_free(zErrMsg);			
            continue;
        }

        devicetype = atoi(dbresult[1]);        
        cJSON_AddItemToObject(device, "devicetype", cJSON_CreateNumber(devicetype));
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        
        switch(type)
        {
            case OP_TYPE_HOUR:
                sprintf(sql, "select wateryield,hour from wateryield_hour where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_DAY:
                sprintf(sql, "select wateryield,day from wateryield_day where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_MONTH:
                sprintf(sql, "select wateryield,month from wateryield_month where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_YEAR:
                sprintf(sql, "select wateryield,year from wateryield_year where deviceid='%s';", deviceid);
                break;
            default:
                break;
        }    
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

        time(&time_now);
        t = localtime(&time_now);
        int day   = t->tm_mday;
    	int month = t->tm_mon + 1; //localtime获取的month范围0-11
    	int year  = t->tm_year;
    	
        for(int i=1;i<=nrow;i++)
        {
            int recordflag = 1;            
            record = cJSON_CreateObject();
            num = atoi((const char*)dbresult[i*2]);
            data = atoi((const char*)dbresult[i*2+1]);

            switch (type)
            {
                case OP_TYPE_HOUR:
                    cJSON_AddNumberToObject(record, "hour", data);
                    break;
                case OP_TYPE_DAY:
                    if(day < data && data >= 29)
                    {
                        if(data == 29 && (month-1) == 2)
                        {
                            if(!isLeapYear(year)) 
                                recordflag = 0;
                        }
                        else if((data == 30 || data == 31) && (month-1) == 2)
                        {
                            recordflag == 0;
                        }                            
                        else if(((month -1) == 4 || (month -1) == 6 || (month -1) == 9 || (month -1) == 11) && data == 31)
                        {
                            recordflag == 0;
                        }                        
                    }

                    if(recordflag)
                    {
                        cJSON_AddNumberToObject(record, "day", data);                            
                    }

                    break;                
                case OP_TYPE_MONTH:
                    cJSON_AddNumberToObject(record, "month", data);
                    break;                
                case OP_TYPE_YEAR:
                    cJSON_AddNumberToObject(record, "year", data);
                    break;
                default:
                    break;                
             
            }
            if(recordflag)
            {
                cJSON_AddNumberToObject(record, "wateryield", num);                
                cJSON_AddItemToArray(records, record);                     
            }     
        }
        cJSON_AddItemToObject(device, "records", records);

        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
    }
    cJSON_AddItemToObject(root, "resultcode", cJSON_CreateNumber(0));
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
	return 0;      
}

/*温度历史数据查询*/
int temperaturedata_query(cJSON* root,char* topic)
{
    MYLOG_INFO("An temperature qury!");
    int devicetype;
    cJSON* t = cJSON_GetObjectItem(root, "operation");        
    if(t == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;    	        
    }
    int type = t->valueint;
    cJSON* devices = cJSON_GetObjectItem(root, "devices");
    if(devices == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }
    int num = cJSON_GetArraySize(devices);
    if(num == 0)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }	    
    for(int i=0;i<num;i++){
        cJSON* device = cJSON_GetArrayItem(devices, i);
        cJSON* deviceidjson = cJSON_GetObjectItem(device, "deviceid");
        cJSON* records = cJSON_CreateArray();
        cJSON* record;    
	    char* deviceid = deviceidjson->valuestring;
	    char sql[250]={0};   
        int nrow = 0, ncolumn = 0;
        char **dbresult;
        char *zErrMsg = NULL;
        int temperature,temperature_high,temperature_low;
        int date;
        time_t time_now;
        struct tm* t;        
        
        cJSON_AddItemToObject(device, "result", cJSON_CreateNumber(0));
	    if(deviceidjson == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            return -1;	        
	    }	        
       
        sprintf(sql, "select devicetype from devices where deviceid='%s';", deviceid);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        if(nrow == 0)
        {
            MYLOG_DEBUG("Can not find the device in devices");
	        sqlite3_free_table(dbresult);
			sqlite3_free(zErrMsg);			
            continue;
        }

        devicetype = atoi(dbresult[1]);        
        cJSON_AddItemToObject(device, "devicetype", cJSON_CreateNumber(devicetype));
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        
        switch(type)
        {
            case OP_TYPE_HOUR:
                sprintf(sql, "select temperature,hour from temperature_hour where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_DAY:
                sprintf(sql, "select temperature_high,temperature_low,day from temperature_day where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_MONTH:
                sprintf(sql, "select temperature_high,temperature_low,month from temperature_month where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_YEAR:
                sprintf(sql, "select temperature_high,temperature_low,year from temperature_year where deviceid='%s';", deviceid);
                break;
            default:
                break;
        }    
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

        time(&time_now);
        t = localtime(&time_now);
        int day   = t->tm_mday;
    	int month = t->tm_mon + 1; //localtime获取的month范围0-11
    	int year  = t->tm_year;
    	
        for(int i=1;i<=nrow;i++)
        {  
            int recordflag = 1;        
            record = cJSON_CreateObject();
            switch (type)
            {
                case OP_TYPE_HOUR:
                {
		            temperature = atoi((const char*)dbresult[i*2])/10;
		            date = atoi((const char*)dbresult[i*2+1]);					
                    cJSON_AddNumberToObject(record, "hour", date);
					cJSON_AddNumberToObject(record, "temperature", temperature); 
                    break;                	
                }
                case OP_TYPE_DAY:
                { 
		            temperature_high = atoi((const char*)dbresult[i*3])/10;
					temperature_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);						
                    if(day < date && date >= 29)
                    {
                        if(date == 29 && (month-1) == 2)
                        {
                            if(!isLeapYear(year)) 
                                recordflag = 0;
                        }
                        else if((date == 30 || date == 31) && (month-1) == 2)
                        {
                            recordflag == 0;
                        }                            
                        else if(((month -1) == 4 || (month -1) == 6 || (month -1) == 9 || (month -1) == 11) && date == 31)
                        {
                            recordflag == 0;
                        }                        
                    }

                    if(recordflag)
                    {
                        cJSON_AddNumberToObject(record, "day", date);
						cJSON_AddNumberToObject(record, "temperature_high", temperature_high);
						cJSON_AddNumberToObject(record, "temperature_low", temperature_low);
                    }
                    break; 
                }
                case OP_TYPE_MONTH:
                {
		            temperature_high = atoi((const char*)dbresult[i*3])/10;
					temperature_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);	                
                    cJSON_AddNumberToObject(record, "month", date);
                    break;
                }
                case OP_TYPE_YEAR:
                {
		            temperature_high = atoi((const char*)dbresult[i*3])/10;
					temperature_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);	                
                    cJSON_AddNumberToObject(record, "year", date);
                    break;
                }
                default:
                    break;                
             
            }
            if(recordflag)
            {                
                cJSON_AddItemToArray(records, record);                     
            }   
        }
        cJSON_AddItemToObject(device, "records", records);

        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
    }
    cJSON_AddItemToObject(root, "resultcode", cJSON_CreateNumber(0));
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
	return 0; 
}

/*温度湿度数据查询*/
int humidity_query(cJSON* root,char* topic)
{
    MYLOG_INFO("An humidity qury!");
    int devicetype;
    cJSON* t = cJSON_GetObjectItem(root, "operation");        
    if(t == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;    	        
    }
    int type = t->valueint;
    cJSON* devices = cJSON_GetObjectItem(root, "devices");
    if(devices == NULL)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }
    int num = cJSON_GetArraySize(devices);
    if(num == 0)
    {
        MYLOG_ERROR("Wrong format MQTT message!");
        return -1;	        
    }	    
    for(int i=0;i<num;i++){
        cJSON* device = cJSON_GetArrayItem(devices, i);
        cJSON* deviceidjson = cJSON_GetObjectItem(device, "deviceid");
        cJSON* records = cJSON_CreateArray();
        cJSON* record;    
	    char* deviceid = deviceidjson->valuestring;
	    char sql[250]={0};   
        int nrow = 0, ncolumn = 0;
        char **dbresult;
        char *zErrMsg = NULL;
        int humidity,humidity_high,humidity_low;
        int date;
        time_t time_now;
        struct tm* t;        
        
        cJSON_AddItemToObject(device, "result", cJSON_CreateNumber(0));
	    if(deviceidjson == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            return -1;	        
	    }	        
       
        sprintf(sql, "select devicetype from devices where deviceid='%s';", deviceid);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        if(nrow == 0)
        {
            MYLOG_DEBUG("Can not find the device in devices");
	        sqlite3_free_table(dbresult);
			sqlite3_free(zErrMsg);			
            continue;
        }

        devicetype = atoi(dbresult[1]);        
        cJSON_AddItemToObject(device, "devicetype", cJSON_CreateNumber(devicetype));
        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
        
        switch(type)
        {
            case OP_TYPE_HOUR:
                sprintf(sql, "select humidity,hour from humidity_hour where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_DAY:
                sprintf(sql, "select humidity_high,humidity_low,day from humidity_day where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_MONTH:
                sprintf(sql, "select humidity_high,humidity_low,month from humidity_month where deviceid='%s';", deviceid);
                break;
            case OP_TYPE_YEAR:
                sprintf(sql, "select humidity_high,humidity_low,year from humidity_year where deviceid='%s';", deviceid);
                break;
            default:
                break;
        }    
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

        time(&time_now);
        t = localtime(&time_now);
        int day   = t->tm_mday;
    	int month = t->tm_mon + 1; //localtime获取的month范围0-11
    	int year  = t->tm_year;
    	
        for(int i=1;i<=nrow;i++)
        {  
            int recordflag = 1;        
            record = cJSON_CreateObject();
            switch (type)
            {
                case OP_TYPE_HOUR:
                {
		            humidity = atoi((const char*)dbresult[i*2])/10;
		            date = atoi((const char*)dbresult[i*2+1]);					
                    cJSON_AddNumberToObject(record, "hour", date);
					cJSON_AddNumberToObject(record, "humidity", humidity); 
                    break;                	
                }
                case OP_TYPE_DAY:
                { 
		            humidity_high = atoi((const char*)dbresult[i*3])/10;
					humidity_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);						
                    if(day < date && date >= 29)
                    {
                        if(date == 29 && (month-1) == 2)
                        {
                            if(!isLeapYear(year)) 
                                recordflag = 0;
                        }
                        else if((date == 30 || date == 31) && (month-1) == 2)
                        {
                            recordflag == 0;
                        }                            
                        else if(((month -1) == 4 || (month -1) == 6 || (month -1) == 9 || (month -1) == 11) && date == 31)
                        {
                            recordflag == 0;
                        }                        
                    }

                    if(recordflag)
                    {
                        cJSON_AddNumberToObject(record, "day", date);
						cJSON_AddNumberToObject(record, "humidity_high", humidity_high);
						cJSON_AddNumberToObject(record, "humidity_low", humidity_low);
                    }
                    break; 
                }
                case OP_TYPE_MONTH:
                {
		            humidity_high = atoi((const char*)dbresult[i*3])/10;
					humidity_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);	                
                    cJSON_AddNumberToObject(record, "month", date);
					cJSON_AddNumberToObject(record, "humidity_high", humidity_high);
					cJSON_AddNumberToObject(record, "humidity_low", humidity_low);					
                    break;
                }
                case OP_TYPE_YEAR:
                {
		            humidity_high = atoi((const char*)dbresult[i*3])/10;
					humidity_low = atoi((const char*)dbresult[i*3+1])/10;
		            date = atoi((const char*)dbresult[i*3+2]);	                
                    cJSON_AddNumberToObject(record, "year", date);
					cJSON_AddNumberToObject(record, "humidity_high", humidity_high);
					cJSON_AddNumberToObject(record, "humidity_low", humidity_low);					
                    break;
                }
                default:
                    break;                
             
            }
            if(recordflag)
            {                
                cJSON_AddItemToArray(records, record);                     
            }   
        }
        cJSON_AddItemToObject(device, "records", records);

        sqlite3_free_table(dbresult);
		sqlite3_free(zErrMsg);
    }
    cJSON_AddItemToObject(root, "resultcode", cJSON_CreateNumber(0));
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
	return 0; 

}


