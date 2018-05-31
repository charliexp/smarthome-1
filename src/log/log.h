#ifndef _LOG_H_
#define _LOG_H_

#include "../utils/type.h"

extern int g_log_level;

void my_log(char* file, int line, char* function, const char *format, ...);

void printBYTE(BYTE* p, int num);
    
int log_init();

#define MYLOG_BYTE(p, n)  if(g_log_level == 0) printBYTE(p, n)

#define MYLOG_DEBUG(format...) if(g_log_level > 0) my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)

#define MYLOG_INFO(format...)  if(g_log_level > 1) my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)

#define MYLOG_ERROR(format...)  if(g_log_level > 2) my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)
#endif
