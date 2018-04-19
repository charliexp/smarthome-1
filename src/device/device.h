#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "zigbee.h"

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
	char *condition;

}devicetask;

typedef struct
{
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
};
#endif
