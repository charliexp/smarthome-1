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

/*��ȡzgbmsg��Ϣ��packetid*/
char getpacketid(void);


/*��ȡ�豸macֵ
* ��ʽ����
* 00:0c:43:e1:76:28
*/
int getmac(char *address);

int addresszero(const void* src);
/*˯��msec����*/
void milliseconds_sleep(unsigned long msec);

/*���ڳ�ʼ��*/
int init_uart(char* port);

/*base64����*/
int base64_encode(const unsigned char * sourcedata, char * base64);

/*base64����*/
int base64_decode(const char * base64, unsigned char * dedata);

/*����ע��mac��ַ*/
int gatewayregister();

/*�ϴ��ļ���������*/
int updatefile(const char* filepath);

/*������ˮ�����ݿ�ɾ��*/
void devicedatadelete(char* id);

void devicedatainit(char* id,int type);

/*�ж��Ƿ�����*/
int isLeapYear(int year);

/*����ͳ��*/
void electricity_stat(char* deviceid, int num);

/*ˮ��ͳ��*/
void wateryield_stat(char* deviceid, int num);

/*sql���ִ��*/
int exec_sql_create(char* sql);

/*�ж��ļ��Ƿ����*/
int is_file_exist(const char* file);

/*�������Ƿ�������*/
int already_running(const char *filename);

/*LED���ƽӿ�*/
int ledcontrol(int num, int action, int time);

int debugproc(cJSON* root, char* topic);
#endif
