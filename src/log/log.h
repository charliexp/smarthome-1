#ifndef _LOG_H_
#define _LOG_H_

extern int g_log_level;

void my_log(const char *format, ...);

int log_init();


#define MYLOG_DEBUG(format...) if(g_log_level == 0) my_log(format);\
    else do{}while(0);

#define MYLOG_INFO(format...)  if(g_log_level == 0 || g_log_level == 1) my_log(format);\
    else do{}while(0);

#define MYLOG_ERROR(format...)  if(g_log_level == 0 || g_log_level == 1 || g_log_level == 2) my_log(format);\
    else do{}while(0);

#endif
