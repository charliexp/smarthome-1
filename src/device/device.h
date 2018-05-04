#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../cjson/cJSON.h"

#define USERNAME_MAXLENGTH 50
#define TOPIC_LENGTH 100
#define MSG_LENGTH 200
#define MQTT_MSG_TYPE_PUB  1
#define MQTT_MSG_TYPE_SUB  2
#define MQTT_MSG_TYPE_UNSUB 3

/*�豸����*/
#define DEV_BOOT			0x00	// ���豸����ָBoot Loader
#define DEV_SOCKET		    0x01	// ����
#define DEV_AIR_CON		    0x02	// �յ����
#define DEV_BOILER			0x03	// ��¯
#define DEV_FAN_COIL		0x04	// ����̹�
#define DEV_FLOOR_HEAT	    0x05	// ��ů
#define DEV_FRESH			0x06	// �·�
#define DEV_WATER_PURIF	    0x07	// ��ˮ��
#define DEV_WATER_HEAT	    0x08	// ��ˮ��
#define DEV_WATER_PUMP	    0x09	// ѭ����
#define DEV_DUST_CLEAN	    0x0A // ������
#define DEV_MULTI_PANEL     0x0B // ���һ�������

/*��������*/
#define ZGB_RESPONSE 0x01 //��Ӧ����
#define ZGB_DEVICETYPE 0x11 //�豸����
#define ZGB_HUMIDITY 0x21 //ʪ�ȵ���
#define ZGB_COLD_WARM_REGULATION 0x22 //��ů����
#define ZGB_AIR_CONDITIONING_THREE_WIND_LEVEL 0x23 //�յ���������
#define ZGB_SOCKET_CONTROL 0x30 //�������ص���

#define SOCKET_OPEN		0x01 // �����ϵ�
#define SOCKET_CLOSE   	0x02// �����ϵ�

//#define ZGB_HUMIDITY 0x24 //�յ��޼�����
//#define ZGB_HUMIDITY 0x25 //�·���������
//#define ZGB_HUMIDITY 0x26 //�·��޼�����
//#define ZGB_HUMIDITY 0x27 //�յ��¶ȵ���
//#define ZGB_HUMIDITY 0x28 //��ů�¶ȵ���
//#define ZGB_HUMIDITY 0x40 //���������¶�����
//#define ZGB_HUMIDITY 0x41 //��������ʪ������
//#define ZGB_HUMIDITY 0x42 //��������PM2.5����
//#define ZGB_HUMIDITY 0x43 //��������CO2����
//#define ZGB_HUMIDITY 0x44 //�������Ӽ�ȩ����
//#define ZGB_HUMIDITY 0x45 //��������TVOC����

typedef char zgbaddress[8];
typedef char BYTES2[2];

typedef struct 
{
	char version;
	char packetid;
	char devicecmdid;
	char data[69];
}devicemsg;

typedef struct
{
	BYTES2 index;
	char sub;
	char opt;
	char length;
	devicemsg devmsg;
}ADF;


typedef struct  
{
	BYTES2 framecontrol;
	char reserved0[6];
	zgbaddress src;
	zgbaddress dest;
	char reserved1[2];
	BYTES2 cmdid;
	BYTES2 groupid;
	ADF adf;
}Payload;

typedef struct
{
	char header;
	char msglength;
	Payload payload;
	char check;
	char footer;
} zigbeemsg;

typedef enum
{
	ON,
    CLOSE,
    STANDBY,
} DEVICESTATUS;

typedef struct  
{
	long msgtype;
	cJSON* p_operation_json;
}deviceoperationmsg;

typedef struct
{
	int qos;
	int retained;
	char* topic; //ָ��ָ��ѣ���Ϣ�������Ҫfree
	char* msgcontent; //ָ��ָ��ѣ���Ϣ�������Ҫfree
}mqttqueuemsg;

typedef struct
{
	long msgtype;
	mqttqueuemsg msg;
}mqttmsg;

typedef struct
{
	char packetid;
	char result;
	char over;//�жϸ���Ϣ�Ƿ����յ��ظ�
}ZGB_MSG_STATUS;

/*zgb��Ϣ��ʼ��*/
void zgbmsginit(zigbeemsg *msg);

/*�յ����ƽӿ�*/
int airconcontrol(cJSON *device, char packetid);

/*�·���ƽӿ�*/
int freshaircontrol(cJSON *device, char packetid);

/*��������*/
int socketcontrol(cJSON *device, char packetid);
/*zigbee��Ϣ���ͽӿ�*/
int sendzgbmsg(zgbaddress address, char *data, char length);


#endif
