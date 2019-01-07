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
    time_t time_now;
    struct tm* t;
    int sec,min;
    int time_sec;//需要定时的秒数 
    
    time(&time_now);
    t = localtime(&time_now);
    sec = t->tm_sec;
    min = t->tm_min;

    if(min >= 58)
    {
        time_sec = 1;
    }
    else
    {
        time_sec = (58*60 - min*60 -sec);
    }

    timer* pelecttimer = createtimer(10, electtimerfun);
    addtimer(pelecttimer);
    
    timer* pstatustimer = createtimer(10, statustimerfun);
    addtimer(pstatustimer);
    signal(SIGALRM, sigalrm_fn); 
    alarm(1);    
}


void electtimerfun(timer* t)
{
    time_t time_now;
    struct tm* tm;
    int sec,min;
    int time_sec;//需要定时的秒数

    MYLOG_DEBUG("electric statistics!");
    ZGBADDRESS address = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; //广播报文
    BYTE payload[] = {ATTR_SOCKET_E};
    sendzgbmsg(address, payload, 1, ZGB_MSGTYPE_DEVICE_STATUS_QUERY, DEV_SOCKET, 0, getpacketid());        
    time(&time_now);
    tm = localtime(&time_now);
    sec = tm->tm_sec;
    min = tm->tm_min;
    time_sec = (60*60 - min*60 -sec) + 58*60;//当前一小时剩余的秒数加上下个59分钟的秒数,alarm函数不精确
    t->timevalue = time_sec;
    t->lefttime = time_sec;
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