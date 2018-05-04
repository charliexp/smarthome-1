/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tools.h"

int getmac(char *address)
{
	int fd;
	int ret;
	fd = open("/sys/class/net/eth0/address", O_RDONLY);
	if (fd == -1)
	{
		printf("get macaddress error!\n");
		return -1;
	}

	ret = read(fd, address, 20);
	if (ret == -1)
	{
		printf("get macaddress error!\n");
		return -2;
	}
	return 0;	
}

