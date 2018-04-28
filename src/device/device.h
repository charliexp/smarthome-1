#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "cJSON.h"

#define USERNAME_MAXLENGTH 50
#define TOPIC_LENGTH 100
#define MSG_LENGTH 200

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
	BYTES2 index;
	char sub;
	char opt;
	char length;
	char data[72];
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
	char lenght;
	Payload payload;
	char check;
	char footer;
} zigbeemsg;

typedef enum
{
    BOX = 1,
	AIRCONDITIONING,
	NEWTREND,
	OTHERS,
} DEVICETYPE;

typedef enum
{
	ON,
    CLOSE,
    STANDBY,
} DEVICESTATUS;

typedef struct
{
	char username[USERNAME_MAXLENGTH];
	char mqttid[16];
    zgbaddress address;
	char devicename[125];
	DEVICETYPE devicetype;
	char operation;
} deviceoperation;

typedef struct  
{
	long msgtype;
	cJSON* p_operation_json;
}deviceoperationmsg;

typedef struct
{
	int qos;
	int retained;
	char topic[TOPIC_LENGTH];
	char msg[MSG_LENGTH];
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

/*����zigbee���ĵ�packetid*/
char getpacketid(void);

/*�յ����ƽӿ�*/
int airconditioningcontrol(cJSON *op);

/*�·���ƽӿ�*/
int airconcontrol(cJSON *op);

/*zigbee��Ϣ���ͽӿ�*/
int sendzgbmsg(zgbaddress address, char *data, char length);

#endif
