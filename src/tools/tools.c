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
	{
		address[17] = '0'; //shell命令执行完输出最后带有\n,此处把\n去掉
		return 0;
	}
	return -1;	
}

