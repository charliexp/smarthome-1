/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tools.h"

int getmac(char *address)
{
	int fd;
	ssize_t ret;
	fd = open("/sys/class/net/eth0/address", O_RDONLY, S_IRUSR);
	if (fd == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -1;
	}

	ret = read(fd, (void*)address, 20);
	if (ret == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -2;
	}
	close(fd);
	return 0;	
}

