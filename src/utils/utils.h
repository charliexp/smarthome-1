#ifndef _TOOLS_H_
#define _TOOLS_H_

#include "type.h"
#include "error.h"
#include "const.h"

extern int g_uartfd;

typedef enum bool
{
	false = 0,
	true
}bool;

/*获取zgbmsg消息的packetid*/
char getpacketid(void);


/*获取设备mac值
* 格式如下
* 00:0c:43:e1:76:28
*/
int getmac(char *address);

int addresszero(const void* src);
/*睡眠msec毫秒*/
void milliseconds_sleep(unsigned long msec);

/*串口初始化*/
int init_uart(char* port);

/*base64编码*/
int base64_encode(const unsigned char * sourcedata, char * base64);

/*base64解码*/
int base64_decode(const char * base64, unsigned char * dedata);

/*网关注册mac地址*/
int gatewayregister();

/*上传文件到服务器*/
int updatefile(const char* filepath);

/*电量、水量数据库删除*/
void devicedatadelete(char* id);

void devicedatainit(char* id,int type);

/*判断是否闰年*/
int isLeapYear(int year);

/*电量统计*/
void electricity_stat(char* deviceid, int num);

/*水量统计*/
void wateryield_stat(char* deviceid, int num);

/*sql语句执行*/
int exec_sql_create(char* sql);

/*判断文件是否存在*/
int is_file_exist(const char* file);

/*检查程序是否已运行*/
int already_running(const char *filename);

/*LED控制接口*/
int ledcontrol(int num, int action, int time);

int debugproc(cJSON* root, char* topic);
#endif
