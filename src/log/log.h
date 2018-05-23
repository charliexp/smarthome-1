#ifndef _LOG_H_
#define _LOG_H_

int g_debug_level = 0;

#define LOG_DEBUG(format...) if(g_debug_level == 0) my_log(##format);\
    else do{}while(0);

#define LOG_INFO(format...)  if(g_debug_level == 0 || g_debug_level == 1) my_log(##format);\
    else do{}while(0);

#define LOG_ERROR(format...)  if(g_debug_level == 0 || g_debug_level == 1 || g_debug_level == 2) my_log(##format);\
    else do{}while(0);

#endif
