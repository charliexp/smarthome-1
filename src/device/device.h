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

cJSON* get_device_status_json(char* deviceid);

cJSON* get_attr_value_object_json(cJSON* device, char attrtype);

void devices_status_query();

void sendzgbmsgfordevices(BYTE devicetype, BYTE *data, char length, char msgtype);

/*更改内存设备属性表中的设备属性*/
void change_device_attr_value(char* deviceid, char attr, int value);

void device_closeallfan();

void gatewayproc(cJSON* op);

/* MQTT的操作数据转为ZGB的DATA */
/* 返回data的数据长度 */
int mqtttozgb(cJSON* op, BYTE* zgbdata, int devicetype);

/*设置设备下线状态*/
void change_devices_offline();

void change_device_online(char* deviceid, char status);

int check_device_online(char* deviceid);
#endif
