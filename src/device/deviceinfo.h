#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "zigbee.h"

typedef enum
{
    BOX,
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
    zigbeeaddress address;
	char *devicename;
	DEVICETYPE devicetype;
	DEVICESTATUS devicestatus;	
} deviceinfo;
#endif
