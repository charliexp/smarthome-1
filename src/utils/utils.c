/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <fcntl.h>      /*文件控制定义*/    
#include <termios.h>    /*PPSIX 终端控制定义*/    
#include <errno.h>      /*错误号定义*/   
#include <curl/curl.h>
#include <sqlite3.h>

#include "utils.h"
#include "const.h"
#include "type.h"
#include "error.h"
#include "../log/log.h"

extern sqlite3* g_db;
extern int g_uartfd;
extern char g_mac[20];
char g_current_packetid = 0;
const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char padding_char = '=';

char getpacketid(void)
{
	char packetid;
	packetid = (g_current_packetid++) % 256;

	return packetid;
}

int getmac(char *address)
{
	int fd;
	ssize_t ret;
	fd = open("/sys/class/net/eth0/address", O_RDONLY, S_IRUSR);
	if (fd == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -1;
	}

	ret = read(fd, (void*)address, 20);
	if (ret == -1)
	{
		printf("get macaddress error!\n");
		close(fd);
		return -2;
	}
	address[strlen(address) - 1] = 0;
	close(fd);
	return 0;	
}

/*睡眠msec毫秒*/
void milliseconds_sleep(unsigned long msec) 
{
	if (msec < 0)
		return;
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	
	select(0, NULL, NULL, NULL, &tv);
}



int addresszero(const void* src)
{
    int i = 0;
    for(; i<8; i++)
    {
        if(*((char*)src+i) == 0)
            continue;
        else
            return -1;
    }
    return 0;
}


/*******************************************************************  
* 名称：                  init_uart  
* 功能： 初始化串口（一般是初始化UART1）  
* 入口参数：     port :串口号(ttyS0,ttyS1,ttyS2)  
* 出口参数：     正确返回为1，错误返回为0  
*******************************************************************/   
int init_uart(char* port)
{
    struct termios options;
    g_uartfd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);    
    if (g_uartfd < 3)    
    {    
        MYLOG_ERROR("Can't Open Serial Port");    
        return -1;    
    }    
    //恢复串口为阻塞状态                                   
    if(fcntl(g_uartfd, F_SETFL, 0) < 0)    
    {    
        MYLOG_ERROR("fcntl failed!");    
        return -1;    
    }

    cfsetispeed(&options, B115200);     
    cfsetospeed(&options, B115200); 
    //修改控制模式，保证程序不会占用串口    
    options.c_cflag |= CLOCAL;    
    //修改控制模式，使得能够从串口中读取输入数据    
    options.c_cflag |= CREAD; 
    options.c_cflag &= ~CRTSCTS;//不使用流控制 
    options.c_cflag &= ~PARENB;     
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    //修改输出模式，原始数据输出    
    options.c_oflag &= ~OPOST;
    options.c_oflag &= ~(ONLCR | OCRNL);
    
    options.c_iflag &= ~(ICRNL | INLCR);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);  
    
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);       
       
    //设置等待时间和最小接收字符    
    options.c_cc[VTIME] = 1; /* 读取一个字符等待1*(1/10)s */      
    options.c_cc[VMIN] = 1; /* 读取字符的最少个数为1 */    
       
    //如果发生数据溢出，接收数据，但是不再读取 刷新收到的数据但是不读    
    tcflush(g_uartfd,TCIFLUSH);    
       
    //激活配置 (将修改后的termios数据设置到串口中）    
    if (tcsetattr(g_uartfd,TCSANOW,&options) != 0)      
    {    
        MYLOG_ERROR("uart set error!");      
        return -1;     
    }    
    return 0;    
}

/** 在字符串中查询特定字符位置索引
* const char *str ，字符串
* char c，要查找的字符
*/
int num_strchr(const char *str, char c) // 
{
    const char *pindex = strchr(str, c);
    if (NULL == pindex){
        return -1;
    }
    return pindex - str;
}
/* 解码
* const char * base64 码字
* unsigned char * dedata， 解码恢复的数据
*/
int base64_decode(const char * base64, unsigned char * dedata)
{
    int i = 0, j=0;
    int trans[4] = {0,0,0,0};
    for (;base64[i]!='\0';i+=4){
        // 每四个一组，译码成三个字符
        trans[0] = num_strchr(base64char, base64[i]);
        trans[1] = num_strchr(base64char, base64[i+1]);
        // 1/3
        dedata[j++] = ((trans[0] << 2) & 0xfc) | ((trans[1]>>4) & 0x03);

        if (base64[i+2] == '='){
            continue;
        }
        else{
            trans[2] = num_strchr(base64char, base64[i + 2]);
        }
        // 2/3
        dedata[j++] = ((trans[1] << 4) & 0xf0) | ((trans[2] >> 2) & 0x0f);

        if (base64[i + 3] == '='){
            continue;
        }
        else{
            trans[3] = num_strchr(base64char, base64[i + 3]);
        }

        // 3/3
        dedata[j++] = ((trans[2] << 6) & 0xc0) | (trans[3] & 0x3f);
    }

    dedata[j] = '\0';

    return 0;
}


/*
 0. 源数据都是8位位宽的数据；
 1. 相当于分组码，将源数据分为3个一组，每一组共24bits，采用每6位对应一个编码码字，那么3*8bits = 4*6its, 
将3个数据映射成4个数据，由于编码的码字都是6位长度，换位10进制就是0-63，总共有64中可能性，这也是base64名字的来历；
 2. 6bits对应10进制数对应的码字如最后的表；
 */
/*编码代码
* const unsigned char * sourcedata， 源数组
* char * base64 ，码字保存
*/
int base64_encode(const unsigned char * sourcedata, char * base64)
{
    int i=0, j=0;
    unsigned char trans_index=0;    // 索引是8位，但是高两位都为0
    const int datalength = strlen((const char*)sourcedata);
    for (; i < datalength; i += 3){
        // 每三个一组，进行编码
        // 要编码的数字的第一个
        trans_index = ((sourcedata[i] >> 2) & 0x3f);
        base64[j++] = base64char[(int)trans_index];
        // 第二个
        trans_index = ((sourcedata[i] << 4) & 0x30);
        if (i + 1 < datalength){
            trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
            base64[j++] = base64char[(int)trans_index];
        }else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            base64[j++] = padding_char;

            break;   // 超出总长度，可以直接break
        }
        // 第三个
        trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
        if (i + 2 < datalength){ // 有的话需要编码2个
            trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
            base64[j++] = base64char[(int)trans_index];

            trans_index = sourcedata[i + 2] & 0x3f;
            base64[j++] = base64char[(int)trans_index];
        }
        else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            break;
        }
    }

    base64[j] = '\0'; 

    return 0;
}


/* 
*网关信息注册
*/
int gatewayregister()
{
    CURL *curl_handle;
    CURLcode res;
    char info[100];
    struct curl_slist *list = NULL;
    
    sprintf(info, "{\"mac\":\"%s\",\n\"version\":\"0.1\"}", g_mac);
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_POST,1);
    
	//curl_easy_setopt(curl_handle,CURLOPT_VERBOSE,1); //打印调试信息

    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://123.206.15.63:8443/manager/gatewayregister");
    
    list = curl_slist_append(list, "accept: */*");
    list = curl_slist_append(list, "Content-Type: application/json");

    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, info);

    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK)
    {
        MYLOG_ERROR("curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
        curl_easy_cleanup(curl_handle);
        curl_slist_free_all(list);
        curl_global_cleanup();        
        return -1;
    }
    else
    {
        MYLOG_INFO("Gateway register success!\n");     
        curl_easy_cleanup(curl_handle);
        curl_slist_free_all(list);
        curl_global_cleanup();   
        return 0;
    }
}

/*文件上传
* const char * filepath 上传文件的绝对路径
*/
int updatefile(const char* filepath)
{
    MYLOG_INFO("begin to upload file:%s", filepath);
    if (!filepath)
    {
        MYLOG_ERROR("The filepath is null");
        return -1;
    }
    

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *hCurl = curl_easy_init();

    if(hCurl != NULL)
    {
        struct curl_slist *pOptionList = NULL;
        pOptionList = curl_slist_append(pOptionList, "Expect:");
        curl_easy_setopt(hCurl, CURLOPT_HTTPHEADER, pOptionList);

        struct curl_httppost* pFormPost = NULL;
        struct curl_httppost* pLastElem = NULL;

        curl_formadd(&pFormPost, &pLastElem, CURLFORM_COPYNAME, "file", CURLFORM_FILE, 
            filepath, CURLFORM_CONTENTTYPE, "text/plain", CURLFORM_END);

		curl_easy_setopt(hCurl,CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(hCurl,CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(hCurl, CURLOPT_HTTPPOST, pFormPost);
        curl_easy_setopt(hCurl, CURLOPT_URL, HTTP_UPLOAD_URL);

        CURLcode res = curl_easy_perform(hCurl);
        if(res != CURLE_OK)
        {
            MYLOG_ERROR("upload file error");
        }
        curl_formfree(pFormPost);
        curl_easy_cleanup(hCurl);
    }

    curl_global_cleanup();
    return 0;
}

/*电量数据处理*/
void electricity_stat(char* deviceid, int num)
{
    time_t time_now;
    int min,hour,day,month,year;
    int day_sum=0,month_sum=0,year_sum=0;
    char sql[250]={0};   
    int nrow = 0, ncolumn = 0;
    char **dbresult;
    char *zErrMsg = NULL;
    struct tm* t;
    time(&time_now);
    t = localtime(&time_now);

    min   = t->tm_min;
	hour  = t->tm_hour;
	day   = t->tm_mday;
	month = t->tm_mon;
	year  = t->tm_year + 1900;

	MYLOG_ERROR("The time is %4d-%2d-%2d %2d:%2d", year, month, day, hour, min);

	sprintf(sql, "replace into electricity_hour(deviceid, electricity, hour) values('%s', %2d, %2d);", deviceid, num, hour);
    exec_sql_create(sql);

    /*天电量处理*/
    if(hour == 0)
    {
        day_sum = num;
    }
    else
    {
    	sprintf(sql, "select electricity from electricity_day where deviceid = '%s' and day = %d", deviceid, day);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        MYLOG_ERROR("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
        if(ncolumn==1&&nrow==1)
        {
            day_sum = atoi(dbresult[1]);
            MYLOG_INFO("The day_sum is %d", day_sum);
            day_sum += num;
        }
        else
        {
            day_sum = num;
            MYLOG_ERROR("can not find right electricity from electricity_day!");
        }
        sqlite3_free_table(dbresult);
    }
	sprintf(sql, "replace into electricity_day(deviceid, electricity, day) values('%s', %2d, %2d);", deviceid, day_sum, day);
    exec_sql_create(sql); 
	
	/*月电量处理*/
	if(day == 1 && hour == 0)
	{
	    month_sum = num;
	}
	else
	{
    	sprintf(sql, "select electricity from electricity_month where deviceid = '%s' and month = %d", deviceid, month);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        MYLOG_ERROR("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
        if(ncolumn==1&&nrow==1)
        {
            month_sum = atoi(dbresult[1]);
            MYLOG_INFO("The month_sum is %d", month_sum);
            month_sum += num;
        }
        else
        {
            month_sum = num;
            MYLOG_ERROR("can not find right electricity from electricity_month!");
        }
        sqlite3_free_table(dbresult);
	}
	sprintf(sql, "replace into electricity_month(deviceid, electricity, month) values('%s', %2d, %2d);", deviceid, month_sum, month);
    exec_sql_create(sql);   

	/*年电量处理*/
	if(month == 1 && day == 1 && hour == 0)
	{
	    year_sum = num;
	}
	else
	{
    	sprintf(sql, "select electricity from electricity_year where deviceid = '%s' and year = %d", deviceid, year);
        sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
        MYLOG_ERROR("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
        if(ncolumn==1&&nrow==1)
        {
            year_sum = atoi(dbresult[1]);
            MYLOG_INFO("The year_sum is %d", year_sum);
            year_sum += num;
        }
        else
        {
            year_sum = num;
            MYLOG_ERROR("can not find right electricity from electricity_year!");
        }
        sqlite3_free_table(dbresult);
	}
	sprintf(sql, "replace into electricity_year(deviceid, electricity, year) values('%s', %2d, %4d);", deviceid, year_sum, year);
    exec_sql_create(sql);    	
}

int exec_sql_create(char* sql)
{
    char *zErrMsg = 0; 
    int rc;  
    
    MYLOG_DEBUG("The sql is %s", sql);
    rc = sqlite3_exec(g_db,sql,0,0,&zErrMsg);
    if(rc != SQLITE_OK && rc != SQLITE_ERROR) //表重复会返回SQLITE_ERROR，该错误属于正常
    {
        MYLOG_ERROR("zErrMsg = %s rc =%d\n",zErrMsg, rc);  
        return -1;          
    }    
}


int lockfile(int fd)
{
    struct flock fl;
 
    fl.l_type = F_WRLCK;  /* write lock */
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;  //lock the whole file
 
    return(fcntl(fd, F_SETLK, &fl));
}

/*单例检查*/
int already_running(const char *filename)
{
    int fd;
    char buf[16];

    fd = open(filename, O_RDWR | O_CREAT, LOCKMODE);
    if (fd < 0) 
    {
        printf("can't open %s: %m\n", filename);
        exit(1);
    }
 
    /* 先获取文件锁 */
    if (lockfile(fd) == -1) {
        if (errno == EACCES || errno == EAGAIN) {
            printf("file: %s already locked\n", filename);
            close(fd);
            return 1;
        }
        printf("can't lock %s: %m\n", filename);
        exit(1);
    }
    /* 写入运行实例的pid */
    sprintf(buf, "%ld\n", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    return 0;
}

/*判断文件是否存在*/
int is_file_exist(const char* file)
{
    if(file == NULL)
    {
        return -1;
    }

    if(access(file, F_OK) == 0)
        return 0;

    return -1;
}

int ledcontrol(int num, int action, int time)
{
    char* dir;
    switch(num)
    {
        case ZGB_LED:
            dir = ZGB_LED_DIR;
            break;
        case NET_LED:
            dir = NET_LED_DIR;
            break;
        case SYS_LED:
            dir = SYS_LED_DIR;
            break;
        default:
            ;
    }
    
    switch(action)
    {
        case LED_ACTION_ON:
        case LED_ACTION_OFF:
        {
            char file[50];
            char cmdline[150]={0};
            sprintf(file, "%s/brightness", dir);
            sprintf(cmdline, "echo 0 > %s && echo %d > %s", file, action, file);
            system(cmdline);
            return 0;
        }
        case LED_ACTION_TRIGGER:
        {
            char trfile[50],onfile[50], offfile[50];
            char cmdline[200];
            sprintf(trfile, "%s/trigger", dir);
            sprintf(onfile, "%s/delay_on", dir);
            sprintf(offfile, "%s/delay_off", dir);

            sprintf(cmdline, "echo timer > %s", trfile);
            system(cmdline);
            milliseconds_sleep(200);
            memset(cmdline, 0, 200);
            sprintf(cmdline, "echo %d > %s && echo %d > %s", time, onfile, time, offfile);
            system(cmdline);        
            return 0;
        }
        default:
        ;       
    }
    return -1;
}
