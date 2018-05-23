#include <stdio.h>  
#include <stdarg.h>  
#include <time.h>
#include <pthread.h>

#include "../utils/const.h"
#include "log.h"

int g_debug_level = 0;
FILE* g_fp;

static pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;


int log_init()
{
    if(!(g_fp = fopen(LOG_FILE, "a+"))) 
    {
        return -1;
    }
    return 0;
}

void my_log(const char *format, ...)
{
    va_list arg;  
    int done;  

    pthread_mutex_lock(&fileMutex);
    va_start(arg, format);  
  
    time_t time_log = time(NULL);  
    struct tm* tm_log = localtime(&time_log);  
    fprintf(g_fp, "[%04d-%02d-%02d %02d:%02d:%02d]%s(%d): ", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, 
        tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec,__FILE__, __LINE__);  
  
    vfprintf(g_fp, format, arg);  
    va_end(arg);  

    fflush(g_fp);
    pthread_mutex_unlock(&fileMutex);  
    return;      
}

