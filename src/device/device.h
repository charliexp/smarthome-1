#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "cjson/cJSON.h"

/*zgb消息初始化*/
void zgbmsginit(zgbmsg *msg);

/*空调控制接口*/
int airconcontrol(cJSON *device, char packetid);

/*新风控制接口*/
int freshaircontrol(cJSON *device, char packetid);

/*插座控制*/
int socketcontrol(cJSON *device, char packetid);

/*zigbee消息发送接口*/
int sendzgbmsg(ZGBADDRESS address, char *data, char length);


#endif
