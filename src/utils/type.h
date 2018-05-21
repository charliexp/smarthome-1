#ifndef _TYPE_H_
#define _TYPE_H_

#include "../cjson/cJSON.h"

typedef unsigned char BYTE;
typedef BYTE ZGBADDRESS[8];

typedef struct 
{
    BYTE magicnum;
    BYTE length;
	BYTE version;
    BYTE devicetype;
    BYTE deviceindex;
    BYTE packetid;
	BYTE msgtype;
	BYTE pdu[69];
}DATA;

typedef struct
{
	BYTE index[2];
	BYTE sub;
	BYTE opt;
	BYTE length;
	DATA data;
}ADF;


typedef struct  
{
	BYTE framecontrol[2];
	BYTE reserved0[6];
	ZGBADDRESS src;
	ZGBADDRESS dest;
	BYTE reserved1[2];
	BYTE cmdid[2];
	BYTE groupid[2];
	ADF adf;
}Payload;

typedef struct
{
	BYTE header;
	BYTE msglength;
	Payload payload;
	BYTE check;
	BYTE footer;
} zgbmsg;

typedef struct
{
    long msgtype;
    zgbmsg msg;
}zgbqueuemsg;

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
}devicequeuemsg;

typedef struct
{
	int qos;
	int retained;
	char* topic; //ָ��ָ��ѣ���Ϣ�������Ҫfree
	char* msgcontent; //ָ��ָ��ѣ���Ϣ�������Ҫfree
}mqttmsg;

typedef struct
{
	long msgtype;
	mqttmsg msg;
}mqttqueuemsg;

typedef struct
{
    long msgtype;
    zgbmsg msg;
}uartsendqueuemsg;

typedef struct
{
	char packetid;
	char result;
	char over;//�жϸ���Ϣ�Ƿ����յ��ظ�
}ZGB_MSG_STATUS;

#endif