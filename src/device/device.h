#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../cjson/cJSON.h"

/*设备zgb地址转成数据库zgb地址*/
void zgbaddresstodbaddress(ZGBADDRESS addr, char* db_address);

/*数据库zgb地址转成设备zgb地址*/
void dbaddresstozgbaddress(char* db_address, ZGBADDRESS addr);


/*zgb消息初始化*/
void zgbmsginit(zgbmsg *msg);


/*zigbee消息发送接口*/
int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid);

void devices_status_json_init();

cJSON* create_device_status_json(char* deviceid, char devicetype);

cJSON* dup_device_status_json(char* deviceid);

cJSON* get_attr_value_object_json(cJSON* device, char attrtype);

void devices_status_query();

void sendzgbmsgfordevices(BYTE devicetype, BYTE *data, char length, char msgtype);

/*更改内存设备属性表中的设备属性*/
void change_device_attr_value(char* deviceid, char attr, int value);

void device_closeallfan();

void change_boiler_mode(int mode);

int gatewayproc(cJSON* op);

/* MQTT的操作数据转为ZGB的DATA */
/* 返回data的数据长度 */
int mqtttozgb(cJSON* op, BYTE* zgbdata, int devicetype);

/*设置设备下线状态*/
void change_devices_offline();

void change_device_online(char* deviceid, char status);

int check_device_online(char* deviceid);

int get_gateway_mode();

void set_gateway_mode(int mode);

void change_system_mode(int mode);

/*系统锅炉插座配置接口*/
int get_system_boiler(char* id);

void set_system_boiler(char* id);

void change_system_boiler(char* id);

/*热水系统插座配置接口*/
int get_hotwatersystem_socket(char* id);

void set_hotwatersystem_socket(char* id);

void change_hotwatersystem_socket(char* id);

/*热水系统水温器配置接口*/
int get_hotwatersystem_temperaturesensor(char* id);

void set_hotwatersystem_temperaturesensor(char* id);

void change_hotwatersystem_temperaturesensor(char* id);

void report_device_status(cJSON* stat);

/*电量查询*/
int electricity_query(cJSON* root, char* topic);
/*水量查询*/
int wateryield_query(cJSON* root,char* topic);

#endif
