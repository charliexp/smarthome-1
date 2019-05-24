#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <fcntl.h>      /*文件控制定义*/    
#include <termios.h>    /*PPSIX 终端控制定义*/    
#include <errno.h>      /*错误号定义*/   
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
extern int g_hotwatersystem_settingtemperature;
extern char g_hotwatersystem_socket[20];
extern char g_hotwatersystem_temperaturesensor[20];
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
        return -1;//不存在该timer
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

/*定时回调任务*/
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
    timer* timer = createtimer(10, statustimerfun);
    addtimer(timer);

    timer = createtimer(10, airconditiontimerfun);
    addtimer(timer);    

    timer = createtimer(10, hotwatertimerfun);
    addtimer(timer);  
	
    int sec,min;
    time_t time_now;
    struct tm* tm;
    time(&time_now);
    tm = localtime(&time_now);
    sec = tm->tm_sec;
    min = tm->tm_min;
    int time_sec;//需要定时的秒数	

	if(min == 59)
	{
		time_sec = 60 - sec + 59*60;
	}
	else
	{
		time_sec = 59*60 - min*60 -sec;//当前一小时剩余的秒数加上下个59分钟的秒数,alarm函数不精确 		
	}

    timer = createtimer(time_sec, electtimerfun);
    addtimer(timer);	
    signal(SIGALRM, sigalrm_fn); 
    alarm(1);    
}


void electtimerfun(timer* t)
{
    time_t time_now;
    struct tm* tm;
    int sec,min,hour,day,month,year;
    int time_sec;//需要定时的秒数

    time(&time_now);
    tm = localtime(&time_now);
    sec   = tm->tm_sec;
    min   = tm->tm_min;
	hour  = tm->tm_hour;
	day   = tm->tm_mday;
	month = tm->tm_mon + 1;
	year  = tm->tm_year + 1900;

	//置零当前小时的数据
	char sql[100] = {0};

	sprintf(sql, "update electricity_hour set electricity = 0 where hour = %d;", hour);
	exec_sql_create(sql);
	memset(sql, 0, 100);
	sprintf(sql, "update wateryield_hour set wateryield = 0 where hour = %d;", hour);
	exec_sql_create(sql);
	memset(sql, 0, 100);

	if(hour == 0)
	{
		//置零当前天的数据
		sprintf(sql, "update electricity_day set electricity = 0 where day = %d;", day);
		exec_sql_create(sql);
		memset(sql, 0, 100);
		sprintf(sql, "update wateryield_day set wateryield = 0 where day = %d;", day);
		exec_sql_create(sql);
		memset(sql, 0, 100);
		if(day == 1)
		{
			//置零当前月的数据
			sprintf(sql, "update electricity_month set electricity = 0 where month = %d;", month);
			exec_sql_create(sql);
			memset(sql, 0, 100);
			sprintf(sql, "update wateryield_month set wateryield = 0 where month = %d;", month);
			exec_sql_create(sql);
			memset(sql, 0, 100);	
		}		
	}


    MYLOG_DEBUG("electric statistics!");
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    BYTE payload[] = {ATTR_SOCKET_E};
    milliseconds_sleep(500);
    sendzgbmsg(address, payload, 1, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, getpacketid());//查询电量
    milliseconds_sleep(500);
    payload[0] = ATTR_SEN_WATER_YIELD;
    sendzgbmsg(address, payload, 1, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, SEN_WATER_FLOW, 0, getpacketid());//查询水量    

    time_sec = (60*60 - min*60 -sec) + 59*60;//当前一小时剩余的秒数加上下个59分钟的秒数,alarm函数不精确
    t->timevalue = time_sec;
    t->lefttime = time_sec;
    return;    
}

void airconditiontimerfun(timer* t)
{
    int openflag = 0;//风机盘管是否全部关闭的标志
    int isexist = 0; //空调主机是否存在
    char* id = NULL;
    int index;
    cJSON* dev,*status;
    int aircond_status = -1;
    int aircond_online = -1;
    pthread_mutex_lock(&g_devices_status_mutex);
    int size = cJSON_GetArraySize(g_devices_status_json);
    int type,value,online;
    ZGBADDRESS airconaddr;
    BYTE close_payload[] = {ATTR_DEVICESTATUS, 0, 0, 0, 0};//关闭操作
    BYTE open_payload[] = {ATTR_DEVICESTATUS, 0, 0, 0, 1};//打开操作
    
	MYLOG_DEBUG("aircondition status check!");
	
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
    if(isexist && aircond_online) //空调主机存在且在线
    {
        if((openflag == 1) && (aircond_status == 0))
        {
            dbaddresstozgbaddress(id, airconaddr);   
            //通知空调打开
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
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    sendzgbmsg(address, 0, 0, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_ANYONE, 0, getpacketid());    
    t->timevalue = 10;
    t->lefttime = 10;
    return;      
}

void hotwatertimerfun(timer* t)
{
    MYLOG_DEBUG("hotwater system check!");
	if(strlen(g_hotwatersystem_socket) == 0 || strlen(g_hotwatersystem_temperaturesensor) == 0){
		return;
	}
	if((check_device_online(g_hotwatersystem_socket)!=1) || (check_device_online(g_hotwatersystem_temperaturesensor)!=1)){
		return;
	}
	cJSON* device_temperaturesensor = get_device_status_json(g_hotwatersystem_temperaturesensor);
	cJSON* tmp = get_attr_value_object_json(device_temperaturesensor, ATTR_ENV_TEMPERATURE);
	
	if(tmp == NULL)
		return;
	
	int temperature = cJSON_GetObjectItem(tmp, "value")->valueint;
	if(temperature > g_hotwatersystem_settingtemperature){
		ZGBADDRESS socketaddr;
		dbaddresstozgbaddress(g_hotwatersystem_socket, socketaddr);
	    BYTE data[5] = {ATTR_DEVICESTATUS, 0x0, 0x0, 0x0, TLV_VALUE_POWER_OFF};
	    sendzgbmsg(socketaddr, data, 5, ZGB_MSGTYPE_DEVICE_OPERATION, DEV_SOCKET, 0, getpacketid()); 
	}
}