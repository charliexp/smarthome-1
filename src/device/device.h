#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "cjson/cJSON.h"

/*zgb��Ϣ��ʼ��*/
void zgbmsginit(zgbmsg *msg);

/*�յ����ƽӿ�*/
int airconcontrol(cJSON *device, char packetid);

/*�·���ƽӿ�*/
int freshaircontrol(cJSON *device, char packetid);

/*��������*/
int socketcontrol(cJSON *device, char packetid);

/*zigbee��Ϣ���ͽӿ�*/
int sendzgbmsg(ZGBADDRESS address, char *data, char length);


#endif
