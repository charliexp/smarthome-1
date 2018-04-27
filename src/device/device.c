#include <stdio.h>
#include "device.h"
#include "cjson/cJSON.h"

char g_current_packetid = 0;

int sendzgbmsg(zigbeemsg *pmsg)
{

}

char getpacketid(void)
{
	return (g_current_packetid++) % 256;
}

int airconditioningcontrol(cJSON *op)
{

}


