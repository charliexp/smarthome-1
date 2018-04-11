/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include<stdio.h>
#include<stdlib.h>

int getmac(char *address)
{
	FILE *fp;
	fp = popen("cat /sys/class/net/eth0/address", "r");
	if (fgets(address, 20, fp))
		return 0;
	return -1;	
}

