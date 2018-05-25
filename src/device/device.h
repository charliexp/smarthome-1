#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../cjson/cJSON.h"

/*�豸zgb��ַת�����ݿ�zgb��ַ*/
unsigned long long zgbaddresstodbaddress(ZGBADDRESS addr);

/*���ݿ�zgb��ַת���豸zgb��ַ*/
void dbaddresstozgbaddress(unsigned long long addr, ZGBADDRESS zgbaddress);


/*zgb��Ϣ��ʼ��*/
void zgbmsginit(zgbmsg *msg);

/*�յ����ƽӿ�*/
int airconcontrol(cJSON *device, char packetid);

/*�·���ƽӿ�*/
int freshaircontrol(cJSON *device, char packetid);

/*��������*/
int socketcontrol(cJSON *device, char packetid);

/*zigbee��Ϣ���ͽӿ�*/
int sendzgbmsg(ZGBADDRESS address, BYTE *data, char length, char msgtype, char devicetype, char deviceindex, char packetid);



#endif
