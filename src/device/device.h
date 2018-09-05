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

void sendzgbmsgfordevices(char devicetype, BYTE *data, char length, char msgtype);


#endif
