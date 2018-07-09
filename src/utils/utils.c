/*******************************************************************************
 * Copyright (c) 2018, ShangPingShangSheng Corp.
 *
 * by lipenghui 2018.04.10
 *******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <fcntl.h>      /*�ļ����ƶ���*/    
#include <termios.h>    /*PPSIX �ն˿��ƶ���*/    
#include <errno.h>      /*����Ŷ���*/   
#include <curl/curl.h>

#include "utils.h"
#include "const.h"
#include "type.h"
#include "error.h"

extern int g_uartfd;
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

/*˯��msec����*/
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
* ���ƣ�                  init_uart  
* ���ܣ� ��ʼ�����ڣ�һ���ǳ�ʼ��UART1��  
* ��ڲ�����     port :���ں�(ttyS0,ttyS1,ttyS2)  
* ���ڲ�����     ��ȷ����Ϊ1�����󷵻�Ϊ0  
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
    //�ָ�����Ϊ����״̬                                   
    if(fcntl(g_uartfd, F_SETFL, 0) < 0)    
    {    
        MYLOG_ERROR("fcntl failed!");    
        return -1;    
    }

    cfsetispeed(&options, B57600);     
    cfsetospeed(&options, B57600); 
    //�޸Ŀ���ģʽ����֤���򲻻�ռ�ô���    
    options.c_cflag |= CLOCAL;    
    //�޸Ŀ���ģʽ��ʹ���ܹ��Ӵ����ж�ȡ��������    
    options.c_cflag |= CREAD; 
    options.c_cflag &= ~CRTSCTS;//��ʹ�������� 
    options.c_cflag &= ~PARENB;     
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    //�޸����ģʽ��ԭʼ�������    
    options.c_oflag &= ~OPOST;
    options.c_oflag &= ~(ONLCR | OCRNL);
    
    options.c_iflag &= ~(ICRNL | INLCR);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);  
    
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);       
       
    //���õȴ�ʱ�����С�����ַ�    
    options.c_cc[VTIME] = 1; /* ��ȡһ���ַ��ȴ�1*(1/10)s */      
    options.c_cc[VMIN] = 1; /* ��ȡ�ַ������ٸ���Ϊ1 */    
       
    //�����������������������ݣ����ǲ��ٶ�ȡ ˢ���յ������ݵ��ǲ���    
    tcflush(g_uartfd,TCIFLUSH);    
       
    //�������� (���޸ĺ��termios�������õ������У�    
    if (tcsetattr(g_uartfd,TCSANOW,&options) != 0)      
    {    
        MYLOG_ERROR("uart set error!");      
        return -1;     
    }    
    return 0;    
}

/** ���ַ����в�ѯ�ض��ַ�λ������
* const char *str ���ַ���
* char c��Ҫ���ҵ��ַ�
*/
int num_strchr(const char *str, char c) // 
{
    const char *pindex = strchr(str, c);
    if (NULL == pindex){
        return -1;
    }
    return pindex - str;
}
/* ����
* const char * base64 ����
* unsigned char * dedata�� ����ָ�������
*/
int base64_decode(const char * base64, unsigned char * dedata)
{
    int i = 0, j=0;
    int trans[4] = {0,0,0,0};
    for (;base64[i]!='\0';i+=4){
        // ÿ�ĸ�һ�飬����������ַ�
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
 0. Դ���ݶ���8λλ������ݣ�
 1. �൱�ڷ����룬��Դ���ݷ�Ϊ3��һ�飬ÿһ�鹲24bits������ÿ6λ��Ӧһ���������֣���ô3*8bits = 4*6its, 
��3������ӳ���4�����ݣ����ڱ�������ֶ���6λ���ȣ���λ10���ƾ���0-63���ܹ���64�п����ԣ���Ҳ��base64���ֵ�������
 2. 6bits��Ӧ10��������Ӧ�����������ı�
 */
/*�������
* const unsigned char * sourcedata�� Դ����
* char * base64 �����ֱ���
*/
int base64_encode(const unsigned char * sourcedata, char * base64)
{
    int i=0, j=0;
    unsigned char trans_index=0;    // ������8λ�����Ǹ���λ��Ϊ0
    const int datalength = strlen((const char*)sourcedata);
    for (; i < datalength; i += 3){
        // ÿ����һ�飬���б���
        // Ҫ��������ֵĵ�һ��
        trans_index = ((sourcedata[i] >> 2) & 0x3f);
        base64[j++] = base64char[(int)trans_index];
        // �ڶ���
        trans_index = ((sourcedata[i] << 4) & 0x30);
        if (i + 1 < datalength){
            trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
            base64[j++] = base64char[(int)trans_index];
        }else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            base64[j++] = padding_char;

            break;   // �����ܳ��ȣ�����ֱ��break
        }
        // ������
        trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
        if (i + 2 < datalength){ // �еĻ���Ҫ����2��
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
*������Ϣע��
*/
int gatewayregister()
{
    CURL *curl_handle;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://123.206.15.63:8443/manager/gatewayregister");
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, g_mac);

    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK)
    {
        MYLOG_ERROR("curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();        
        return -1;
    }
    else
    {
        MYLOG_INFO("Gateway register success!\n");     
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();   
        return 0;
    }
}

/*�ļ��ϴ�
* const char * filepath �ϴ��ļ��ľ���·��
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
        curl_easy_setopt(hCurl, CURLOPT_URL, HTTP_UPLOAD_UAR);

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
