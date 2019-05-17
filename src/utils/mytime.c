#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>      /*�ļ����ƶ���*/    
#include <termios.h>    /*PPSIX �ն˿��ƶ���*/    
#include <errno.h>      /*����Ŷ���*/   
#include <curl/curl.h>
#include <sqlite3.h>

#include "mytime.h"
#include "utils.h"
#include "const.h"
#include "type.h"
#include "error.h"
#include "../device/device.h"
#include "../log/log.h"

extern pthread_mutex_t g_devices_status_mutex;
extern cJSON* g_devices_status_json;

timer* g_timer_manager;

timer* createtimer(int timevlaue,void(*fun)())
{
    MYLOG_INFO("create a timer!, timevalue is %d", timevlaue);
    timer* t = (timer*)malloc(sizeof(timer));
    if(t == NULL)
    {
        MYLOG_ERROR("Timer create failed,malloc error!");
        return NULL;
    }

    t->handler = fun;
    t->timevalue = timevlaue;
    t->lefttime = timevlaue;
    t->next = NULL;

    return t;
}


void addtimer(timer* t)
{
    if (g_timer_manager == NULL)
    {
        g_timer_manager = t;
    }
    else
    {
        timer* temp = g_timer_manager;
        while(temp->next)
        {
            temp = temp->next;
        }

        temp->next = t;
    }
}

int deltimer(timer* t)
{
    timer* temp = g_timer_manager;
    timer* pre,*next;

    if(g_timer_manager == t)
    {
        next = t->next;
        t->next = NULL;
        g_timer_manager = next;
        free(t);
        return 0;
    }

    while(temp != NULL && temp != t)
    {
        pre = temp;
        temp = temp->next;
    }

    if(temp == NULL)
    {
        return -1;//�����ڸ�timer
    }

    pre->next = temp->next;
    temp->next = NULL;
    free(t);
    return 0;
}

int rebuildtimer(timer* t)
{
    if(t == NULL)
    {
        return 0;
    }
    
    t->lefttime = t->timevalue;
    return -1;
}

/*��ʱ�ص�����*/
void sigalrm_fn(int sig)
{
    timer* t = g_timer_manager;
    
    alarm(1); 
    
    if(g_timer_manager == NULL)
        return;  
    
    while(t)
    {
        if(--t->lefttime == 0)
            t->handler(t);
        t = t->next;        
    }
    return;
}

void timerinit()
{    
    timer* pstatustimer = createtimer(10, statustimerfun);
    addtimer(pstatustimer);

    timer* pairconditiontimer = createtimer(10, airconditiontimerfun);
    addtimer(pairconditiontimer);    

    int sec,min;
    time_t time_now;
    struct tm* tm;
    time(&time_now);
    tm = localtime(&time_now);
    sec = tm->tm_sec;
    min = tm->tm_min;
    int time_sec;//��Ҫ��ʱ������	

	if(min == 59)
	{
		time_sec = 60 - sec + 59*60;
	}
	else
	{
		time_sec = 59*60 - min*60 -sec;//��ǰһСʱʣ������������¸�59���ӵ�����,alarm��������ȷ 		
	}

    timer* pelecttimer = createtimer(time_sec, electtimerfun);
    addtimer(pelecttimer);	
    signal(SIGALRM, sigalrm_fn); 
    alarm(1);    
}


void electtimerfun(timer* t)
{
    time_t time_now;
    struct tm* tm;
    int sec,min,hour,day,month,year;
    int time_sec;//��Ҫ��ʱ������

    time(&time_now);
    tm = localtime(&time_now);
    sec   = tm->tm_sec;
    min   = tm->tm_min;
	hour  = tm->tm_hour;
	day   = tm->tm_mday;
	month = tm->tm_mon + 1;
	year  = tm->tm_year + 1900;

	//���㵱ǰСʱ������
	char sql[100] = {0};

	sprintf(sql, "update electricity_hour set electricity = 0 where hour = %d;", hour);
	exec_sql_create(sql);
	memset(sql, 0, 100);
	sprintf(sql, "update wateryield_hour set wateryield = 0 where hour = %d;", hour);
	exec_sql_create(sql);
	memset(sql, 0, 100);

	if(hour == 0)
	{
		//���㵱ǰ�������
		sprintf(sql, "update electricity_day set electricity = 0 where day = %d;", day);
		exec_sql_create(sql);
		memset(sql, 0, 100);
		sprintf(sql, "update wateryield_day set wateryield = 0 where day = %d;", day);
		exec_sql_create(sql);
		memset(sql, 0, 100);
		if(day == 1)
		{
			//���㵱ǰ�µ�����
			sprintf(sql, "update electricity_month set electricity = 0 where month = %d;", month);
			exec_sql_create(sql);
			memset(sql, 0, 100);
			sprintf(sql, "update wateryield_month set wateryield = 0 where month = %d;", month);
			exec_sql_create(sql);
			memset(sql, 0, 100);	
		}		
	}


    MYLOG_DEBUG("electric statistics!");
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //�㲥����
    BYTE payload[] = {ATTR_SOCKET_E};
    milliseconds_sleep(500);
    sendzgbmsg(address, payload, 1, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, getpacketid());//��ѯ����
    milliseconds_sleep(500);
    payload[0] = ATTR_SEN_WATER_YIELD;
    sendzgbmsg(address, payload, 1, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, SEN_WATER_FLOW, 0, getpacketid());//��ѯˮ��    

    time_sec = (60*60 - min*60 -sec) + 59*60;//��ǰһСʱʣ������������¸�59���ӵ�����,alarm��������ȷ
    t->timevalue = time_sec;
    t->lefttime = time_sec;
    return;    
}

void airconditiontimerfun(timer* t)
{
    int openflag = 0;//����̹��Ƿ�ȫ���رյı�־
    int isexist = 0; //�յ������Ƿ����
    char* id = NULL;
    int index;
    cJSON* dev,*status;
    int aircond_status = -1;
    int aircond_online = -1;
    pthread_mutex_lock(&g_devices_status_mutex);
    int size = cJSON_GetArraySize(g_devices_status_json);
    int type,value,online;
    ZGBADDRESS airconaddr;
    BYTE close_payload[] = {ATTR_DEVICESTATUS, 0, 0, 0, 0};//�رղ���
    BYTE open_payload[] = {ATTR_DEVICESTATUS, 0, 0, 0, 1};//�򿪲���
    for(int i=0; i<size; i++)
    {
        dev = cJSON_GetArrayItem(g_devices_status_json, i);
        type = cJSON_GetObjectItem(dev, "devicetype")->valueint;
        if(type == DEV_FAN_COIL)
        {
            status = cJSON_GetObjectItem(dev, "status");  
            online = cJSON_GetObjectItem(dev, "online")->valueint;
            value = cJSON_GetObjectItem(cJSON_GetArrayItem(status, 0), "value")->valueint;
            if(value == 1 && online == 1)
            {
                openflag = 1;
            }
        }
        else if(type == DEV_AIR_CON)
        {
            isexist = 1;
            status = cJSON_GetObjectItem(dev, "status");
            id = cJSON_GetObjectItem(dev, "deviceid")->valuestring;
            index = atoi(id+16);
            aircond_online = cJSON_GetObjectItem(dev, "online")->valueint;
            aircond_status = cJSON_GetObjectItem(cJSON_GetArrayItem(status, 0), "value")->valueint;
        }
    }
    if(isexist && aircond_online) //�յ���������������
    {
        if((openflag == 1) && (aircond_status == 0))
        {
            dbaddresstozgbaddress(id, airconaddr);   
            //֪ͨ�յ���
            sendzgbmsg(airconaddr, open_payload, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_AIR_CON, index, getpacketid());
        }
        else if((openflag == 0) && (aircond_status == 1))
        {      
            dbaddresstozgbaddress(id, airconaddr);
            sendzgbmsg(airconaddr, close_payload, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_AIR_CON, index, getpacketid());
        }        
    }

    pthread_mutex_unlock(&g_devices_status_mutex); 
    t->timevalue = 10;
    t->lefttime = 10;
    return;     
}

void statustimerfun(timer* t)
{
    MYLOG_DEBUG("device status query!");
    change_devices_offline();
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //�㲥����
    sendzgbmsg(address, 0, 0, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, getpacketid());    
    t->timevalue = 10;
    t->lefttime = 10;
    return;      
}