#ifndef _LOG_H_
#define _LOG_H_

#include "../utils/type.h"

extern int g_log_level;

void my_log(char* file, int line, char* function, const char *format, ...);

void printBYTE(BYTE* p, int num);
    
int log_init();

#define MYLOG_BYTE(p, n)  if(g_log_level == 0) printBYTE(p, n)

#define MYLOG_DEBUG(format...) if(g_log_level== 0) my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)

#define MYLOG_INFO(format...)  if(g_log_level == 0 || g_log_level == 1) my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)

#define MYLOG_ERROR(format...)  my_log((char*)__FILE__, (int)__LINE__, (char*)__FUNCTION__, format)

#define MYLOG_ZGBMSG(msg) {  MYLOG_INFO("header:%2X", msg.header);MYLOG_INFO("msglength:%2X", msg.msglength);\
    MYLOG_INFO("framecontrol:%2X%2X", msg.payload.framecontrol[0], msg.payload.framecontrol[1]);\
    MYLOG_INFO("src:%2X%2X%2X%2X%2X%2X%2X%2X", msg.payload.src[0], msg.payload.src[1], msg.payload.src[2], msg.payload.src[3], msg.payload.src[4], msg.payload.src[5], msg.payload.src[6], \
    msg.payload.src[7]);\
    MYLOG_INFO("src:%2X%2X%2X%2X%2X%2X%2X%2X", msg.payload.dest[0], msg.payload.dest[1], msg.payload.dest[2], msg.payload.dest[3], msg.payload.dest[4], msg.payload.dest[5], msg.payload.dest[6], \
    msg.payload.dest[7]);\
    MYLOG_INFO("cmdid:%2X%2X", msg.payload.cmdid[0], msg.payload.cmdid[1]);\
    MYLOG_INFO("groupid:%2X%2X", msg.payload.groupid[0], msg.payload.groupid[1]);\
    MYLOG_INFO("index:%2X%2X", msg.payload.adf.index[0], msg.payload.adf.index[1]);\
    MYLOG_INFO("sub:%2X", msg.payload.adf.sub);\
    MYLOG_INFO("opt:%2X", msg.payload.adf.opt);\
    MYLOG_INFO("length:%2X", msg.payload.adf.length);\
    MYLOG_INFO("data:");\
    MYLOG_BYTE((BYTE*)&msg.payload.adf.data, 7+msg.payload.adf.data.length);\
}
#endif
