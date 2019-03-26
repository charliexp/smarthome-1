#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../cjson/cJSON.h"

/*�豸zgb��ַת�����ݿ�zgb��ַ*/
void zgbaddresstodbaddress(ZGBADDRESS addr, char* db_address);

/*���ݿ�zgb��ַת���豸zgb��ַ*/
void dbaddresstozgbaddress(char* db_address, ZGBADDRESS addr);


/*zgb��Ϣ��ʼ��*/
void zgbmsginit(zgbmsg *msg);


/*zigbee��Ϣ���ͽӿ�*/
int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid);

void devices_status_json_init();

cJSON* create_device_status_json(char* deviceid, char devicetype);

cJSON* get_device_status_json(char* deviceid);

cJSON* get_attr_value_object_json(cJSON* device, char attrtype);

void devices_status_query();

void sendzgbmsgfordevices(BYTE devicetype, BYTE *data, char length, char msgtype);

/*�����ڴ��豸���Ա��е��豸����*/
void change_device_attr_value(char* deviceid, char attr, int value);

void device_closeallfan();

void gatewayproc(cJSON* op);

/* MQTT�Ĳ�������תΪZGB��DATA */
/* ����data�����ݳ��� */
int mqtttozgb(cJSON* op, BYTE* zgbdata, int devicetype);

/*�����豸����״̬*/
void change_devices_offline();

void change_device_online(char* deviceid, char status);

int check_device_online(char* deviceid);

int get_gateway_mode();

void set_gateway_mode(int mode);

void change_system_mode(int mode);
#endif
