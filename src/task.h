#ifndef _TASK_H_
#define _TASK_H_

/*局域网监听任务*/
void* lantask(void *argc);

/*定时任务*/
void timefun(void);

/*系统初始化任务*/
void init();

/*APP下发的设备消息的处理进程*/
void* devicemsgprocess(void *argc);

/*监听串口的进程，提取单片机上传的ZGB消息*/
void* uartlisten(void *argc);

#endif
