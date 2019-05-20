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

cJSON* dup_device_status_json(char* deviceid);

cJSON* get_attr_value_object_json(cJSON* device, char attrtype);

void devices_status_query();

void sendzgbmsgfordevices(BYTE devicetype, BYTE *data, char length, char msgtype);

/*�����ڴ��豸���Ա��е��豸����*/
void change_device_attr_value(char* deviceid, char attr, int value);

void device_closeallfan();

void change_boiler_mode(int mode);

int gatewayproc(cJSON* op);

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

/*ϵͳ��¯�������ýӿ�*/
int get_system_boiler(char* id);

void set_system_boiler(char* id);

void change_system_boiler(char* id);

/*��ˮϵͳ�������ýӿ�*/
int get_hotwatersystem_socket(char* id);

void set_hotwatersystem_socket(char* id);

void change_hotwatersystem_socket(char* id);

/*��ˮϵͳˮ�������ýӿ�*/
int get_hotwatersystem_temperaturesensor(char* id);

void set_hotwatersystem_temperaturesensor(char* id);

void change_hotwatersystem_temperaturesensor(char* id);

void report_device_status(cJSON* stat);

/*������ѯ*/
int electricity_query(cJSON* root, char* topic);
/*ˮ����ѯ*/
int wateryield_query(cJSON* root,char* topic);

#endif
