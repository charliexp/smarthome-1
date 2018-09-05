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

void sendzgbmsgfordevices(char devicetype, BYTE *data, char length, char msgtype);


#endif
