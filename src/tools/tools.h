#ifndef _TOOLS_H_
#define _TOOLS_H_

typedef enum bool
{
	false = 0,
	true
}bool;

/*获取设备mac地址
* 得到的值如下:
* 00:0c:43:e1:76:28
*/
int getmac(char *address);

#endif
