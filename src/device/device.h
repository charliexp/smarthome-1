#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "../zigbee/zigbee.h"
#define USERNAME_MAXLENGTH 50
#define TOPIC_LENGTH 50
#define MSG_LENGTH 200

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
	int type;
	char condition[50];

}devicetask;

typedef struct
{
	char username[USERNAME_MAXLENGTH];
	char mqttid[16];
    zigbeeaddress address;
	char devicename[125];
	DEVICETYPE devicetype;
	char operation;
	devicetask task;
} deviceoperation;

typedef struct  
{
	long msgtype;
	deviceoperation operation;
}deviceoperationmsg;

typedef struct
{
	int qos;
	char topic[TOPIC_LENGTH];
	char msg[MSG_LENGTH];
}mqttqueuemsg;

typedef struct
{
	long msgtype;
	mqttqueuemsg msg;
}mqttmsg;
#endif
