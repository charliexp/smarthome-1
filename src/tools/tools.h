#ifndef _TOOLS_H_
#define _TOOLS_H_

extern int g_uartfd;

typedef enum bool
{
	false = 0,
	true
}bool;

/*获取zgbmsg的packetid*/
char getpacketid(void);


/*获取设备mac地址
* 得到的值如�?
* 00:0c:43:e1:76:28
*/
int getmac(char *address);


/*睡眠msec毫秒*/
void milliseconds_sleep(unsigned long msec);

/*���ڳ�ʼ��*/
int init_uart(char* port);

#endif
