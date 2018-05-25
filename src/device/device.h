#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../cjson/cJSON.h"

/*设备zgb地址转成数据库zgb地址*/
unsigned __int64 zgbaddresstodbaddress(ZGBADDRESS addr);

/*zgb消息初始化*/
void zgbmsginit(zgbmsg *msg);

/*空调控制接口*/
int airconcontrol(cJSON *device, char packetid);

/*新风控制接口*/
int freshaircontrol(cJSON *device, char packetid);

/*插座控制*/
int socketcontrol(cJSON *device, char packetid);

/*zigbee消息发送接口*/
int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid);



#endif
