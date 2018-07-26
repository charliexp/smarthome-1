#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "paho/MQTTAsync.h"
#include "utils/utils.h"
#include "device/device.h"
#include "cjson/cJSON.h"
#include "log/log.h"


char g_topicroot[20] = {0};
char g_mac[20] = {0};
ZGB_MSG_STATUS g_devicemsgstatus[ZGBMSG_MAX_NUM];
int g_zgbmsgnum;
cJSON* g_device_mqtt_json, *g_devices_status_json;
int g_uartfd;
int g_log_level = 0;

int g_operationflag = 0;
int g_queueid;
sqlite3* g_db;
sem_t g_mqttconnetionsem;
//订阅g_topics对应的qos
int g_qoss[TOPICSNUM] = {2, 1};
//程序启动后申请堆存放需要订阅的topic
char* g_topics[TOPICSNUM] ={0x0, 0x0};
char g_topicthemes[TOPICSNUM][10] = {{"devices"}, {"gateway"}};

struct operation_results
{
	int msgid;
	struct operation
	{ 
		ZGBADDRESS address;
		char op;
		char result;
	} operation;
} g_operationstatus;

void init()
{
	int i = 0;

    //获取网关mac地址
	if(getmac(g_mac) == 0)
	{
        sprintf(g_topicroot, "/%s/", g_mac);    
	}
    
    if(init_uart("/dev/ttyS1") == -1)
    {
        MYLOG_ERROR("init_uart fail!");
    }

	for(; i < TOPICSNUM; i++)
	{
		g_topics[i] = (char*)malloc(sizeof(g_topicroot) + sizeof(g_topicthemes[i]) + strlen("/+"));
		sprintf(g_topics[i], "%s%s/+", g_topicroot, g_topicthemes[i]);
	}
    devices_status_json_init();
    gatewayregister();
}

/*messagetype: 1、发布 2、订阅 3、去订阅*/
void sendmqttmsg(long messagetype, char* topic, char* message, int qos, int retained)
{
	int ret;
	size_t msglen;
	mqttqueuemsg msg = { 0 };
	msg.msgtype = QUEUE_MSG_MQTT;
	msg.msg.qos = qos;
	msg.msg.retained = 0;
	if (topic != NULL)
	{
		msg.msg.topic = malloc(strlen(topic)+1);
		strncpy(msg.msg.topic, topic, strlen(topic));
		msg.msg.topic[strlen(topic)] = 0;
	}

	if (message != NULL && messagetype == 1)
	{
		msg.msg.msgcontent = malloc(strlen(message) + 1);
		strncpy(msg.msg.msgcontent, message, strlen(message));
		msg.msg.msgcontent[strlen(message)] = 0;
	}

	msglen = sizeof(mqttmsg);
	if (ret = msgsnd(g_queueid, &msg, msglen, 0) != 0)
	{
		MYLOG_ERROR("send mqttqueuemsg fail!");
		return;
	}

	return;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	MYLOG_ERROR("Successful disconnection");
}

/*MQTT订阅消息到达的处理函数*/
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
	char* payloadptr;
	char* mqttid;
	cJSON *root;
	devicequeuemsg msg;
	int sendret;
	size_t msglen = sizeof(devicequeuemsg);
	char topic[TOPIC_LENGTH] = { 0 };

    MYLOG_INFO("Message arrived");
    MYLOG_INFO("topic: %s", topicName);

    payloadptr = (char*)message->payload;
	root = cJSON_Parse(payloadptr);
    MYLOG_INFO("message: %s", cJSON_Print(root));

	if (root == NULL)
	{
		MYLOG_ERROR("Wrong msg!");
		return 0;
	}

    mqttid = cJSON_GetObjectItem(root, "mqttid")->valuestring;
    sprintf(topic, "%sresponse/%s/", topicName, mqttid);
    
	if (strstr(topicName, "devices") != 0)
	{
		if (g_operationflag) //检查当前是否有msg在处理
		{
			/*如果有消息在处理，则返回忙碌错误*/		
			sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, MQTT_MSG_SYSTEM_BUSY, QOS_LEVEL_2, 0);
		}
		else
		{
			/*将操作消息json链表的root封装消息丢入消息队列*/
			g_operationflag = 1;
			MYLOG_INFO("get an operation msg.\n");

			msg.msgtype = QUEUE_MSG_DEVIC;
			msg.p_operation_json = cJSON_Duplicate(root, 1);

			if (sendret = msgsnd(g_queueid, (void*)&msg, msglen, 0) != 0)
			{
				MYLOG_ERROR("sendret = %d", sendret);
				MYLOG_ERROR("send devicequeuemsg fail!");
				return 0;
			}
			cJSON_Delete(root);
		}
	}
	else if(strstr(topicName, "gateway") != 0)
	{
	    int operationtype = cJSON_GetObjectItem(root, "operation")->valueint;
        if(operationtype == 1)
        {
            int ret = gatewayregister();
            if (ret == 0)
            {
                MYLOG_INFO("Register gateway success!");
                cJSON_AddStringToObject(root, "result", "ok");           
            }
            else
            {
                MYLOG_INFO("Register gateway failed!");
                cJSON_AddStringToObject(root, "result", "fail");                
            }
            sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
        }
        else if(operationtype == 2)
        {
            if(updatefile(LOG_FILE))
            {
                cJSON_AddStringToObject(root, "result", "ok");
            }
            else
            {
                cJSON_AddStringToObject(root, "result", "fail");
            }
            sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
            
        }
        else if(operationtype == 3)
        {
            int loglevel = cJSON_GetObjectItem(root, "value")->valueint;
            g_log_level = loglevel;
        }
	}

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}


void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	MYLOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
    sem_post(&g_mqttconnetionsem);
}


void connlost(void *context, char *cause)
{
	MYLOG_ERROR("Connection lost,the cause is %s", cause);
	sem_post(&g_mqttconnetionsem);
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	pthread_t pthread;
	int rc;
	MYLOG_DEBUG("Successful connection");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, g_qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MYLOG_ERROR("sub error!");
	}

	return;
}

/*MQTT订阅消息处理消息的进程*/
void *mqttlient(void *argc)
{  
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

    while(1)
	{
        sem_wait(&g_mqttconnetionsem);
        rc = MQTTAsync_connect(client, &conn_opts);
        if (rc != MQTTASYNC_SUCCESS)
        {
            MYLOG_ERROR("MQTTClient: MQTT connect fail!");
        }
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

/*设备消息的处理进程*/
void* devicemsgprocess(void *argc)
{
	devicequeuemsg msg; //消息队列中取出的设备操作消息
	char topic[TOPIC_LENGTH] = { 0 };//对操作的响应topic
	int mqttid = 0;
	cJSON *devices, *device, *operations, *operation, *tmp;
	int devicenum = 0, operationnum = 0;
	int i = 0;
	int rcvret;
    int operationtype;
	char packetid;
	int deviceid;
	size_t msglen = sizeof(devicequeuemsg);

    ZGBADDRESS src;
    zgbqueuemsg qmsg;
    char db_zgbaddress[20];
    char devicetype,deviceindex;
    char *zErrMsg = 0;  
    char sql[512]; 
    BYTE data[72];
    int rc;  
    int len = 0;         

	while(1)
	{
		if (rcvret = msgrcv(g_queueid, (void*)&msg, msglen, QUEUE_MSG_DEVIC, 0) <= 0)
		{
            MYLOG_ERROR("recive devicequeuemsg fail!rcvret = %d", rcvret);
            continue;
		}
        
		MYLOG_INFO("devicemsgprocess recive a msg");
		MYLOG_INFO("the msg is %s", cJSON_PrintUnformatted(msg.p_operation_json));
        
		g_device_mqtt_json = msg.p_operation_json;
        tmp = cJSON_GetObjectItem(g_device_mqtt_json, "operation");
        if (tmp == NULL)
        {
            MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
            cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
            goto response;
        }          
        operationtype = tmp->valueint;//操作类型

        if(operationtype == 0)//操作的coo模块
        {
            int actiontype;
            tmp = cJSON_GetObjectItem(g_device_mqtt_json, "cmdid");
            if (tmp == NULL)
            {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
            }             
            actiontype = tmp->valueint;//coo操作值
                     
            switch (actiontype)
            {
                case TYPE_CREATE_NETWORK:
                    write(g_uartfd, AT_CREATE_NETWORK, strlen(AT_CREATE_NETWORK));
                    MYLOG_INFO("COO operation AT+FORM=02");
                    break;
                case TYPE_OPEN_NETWORK:
                    write(g_uartfd, AT_CREATE_NETWORK, strlen(AT_CREATE_NETWORK));
                    MYLOG_INFO("COO operation AT+FORM=02");
                    milliseconds_sleep(2000);
                    write(g_uartfd, AT_OPEN_NETWORK, strlen(AT_OPEN_NETWORK));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=60");
                    break;
                case TYPE_DEVICE_LIST:
                    write(g_uartfd, AT_DEVICE_LIST, strlen(AT_DEVICE_LIST));
                    MYLOG_INFO("COO operation AT+LIST");                    
                    break;   
                case TYPE_NETWORK_NOCLOSE:
                    write(g_uartfd, AT_NETWORK_NOCLOSE, strlen(AT_NETWORK_NOCLOSE));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=FF");
                    break;
                case TYPE_CLOSE_NETWORK:
                    write(g_uartfd, AT_CLOSE_NETWORK, strlen(AT_CLOSE_NETWORK));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=00");
                    break;
                case TYPE_NETWORK_INFO:
                    write(g_uartfd, AT_NETWORK_INFO, strlen(AT_NETWORK_INFO));
                    MYLOG_INFO("COO operation AT+GETINFO");
                    break;   
                default:
                    MYLOG_ERROR("Unknow actiontype!");
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);                    
            }
            goto response;
        }
        
		devices = cJSON_GetObjectItem(g_device_mqtt_json, "devices");
		if (devices == NULL)
		{
			MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
            cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
            goto response;
		} 
        
		devicenum = cJSON_GetArraySize(devices);//一个消息中操作的设备数量

		for (i=0; i< devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);

            /***
            **全局的设备操作数组中添加一个新的未处理的
            **设备操作动作
            ***/
			packetid = getpacketid(); //packetid是用来跟zgb设备通信使用的
			g_devicemsgstatus[i].packetid = packetid;
			g_devicemsgstatus[i].finish = 0;
			g_zgbmsgnum++;
            
            tmp = cJSON_GetObjectItem(device, "deviceid");
            if(tmp == NULL)
		    {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
			}                
            
			deviceid = tmp->valueint;

            int nrow = 0, ncolumn = 0;
	        char **dbresult; 
            
            sprintf(sql,"SELECT * FROM devices WHERE deviceid = %d;", deviceid);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            if(nrow == 0) //数据库中没有该设备
            {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_UNKNOW_DEVICE);
                goto response;
            }

            strncpy(db_zgbaddress, dbresult[ncolumn+1], 20);
            devicetype  = dbresult[ncolumn+2][0] - '0';
            deviceindex = dbresult[ncolumn+3][0] - '0';
            operations  = cJSON_GetObjectItem(device, "operations");
		    if (operations == NULL)
		    {
			    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
		    }        
		    operationnum = cJSON_GetArraySize(operations);
            MYLOG_INFO("the operationnum = %d", operationnum);
            sqlite3_free_table(dbresult);
            
		}

		for (i=0; i<RESPONSE_WAIT/50000; i++)
		{
			int j = 0;
			for (; j < g_zgbmsgnum; j++)
			{
				if (g_devicemsgstatus[j].finish == 0)
					break;
			}
			if (j == g_zgbmsgnum)//所有的zgb消息都已处理完
			{
				break;
			}
			milliseconds_sleep(50);
		}

		for (i=0; i<devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);
			cJSON_AddNumberToObject(device, "result", g_devicemsgstatus[i].result);
		}
        
response:      
		mqttid = cJSON_GetObjectItem(msg.p_operation_json, "mqttid")->valueint;
		sprintf(topic, "%s%s/response/%d", g_topicroot, g_topicthemes[0], mqttid);
		sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(msg.p_operation_json), QOS_LEVEL_2, 0);
		cJSON_Delete(msg.p_operation_json);
        g_zgbmsgnum = 0;
		g_operationflag = 0;
        g_device_mqtt_json = NULL;
	}
	pthread_exit(NULL);
}

void* uartsend(void *argc)
{    
    uartsendqueuemsg qmsg;    
    int rcvret;
    
    MYLOG_DEBUG("Enter pthread uartsend");
    while(true)
    {
		if ( rcvret = msgrcv(g_queueid, (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_UART, 0) <= 0 )
		{
			MYLOG_ERROR("rcvret = %d", rcvret);
			MYLOG_ERROR("recive uartsendmsg fail!");
		}
        MYLOG_INFO("uart begin to send msg!");
        MYLOG_BYTE((BYTE*)&qmsg.msg, sizeof(qmsg.msg));
        if ( write(g_uartfd, (char *)&qmsg.msg, (int)(qmsg.msg.msglength - 2)) != (qmsg.msg.msglength - 2))
	    {
		    MYLOG_ERROR("com write error!");
	    }
	    if ( write(g_uartfd, (char *)&qmsg.msg+sizeof(qmsg.msg)-2, 2) != 2)//发送check和footer
	    {
		    MYLOG_ERROR("com write check and footer error!");
	    }
    }
	pthread_exit(NULL);    
}

/*zgb消息处理*/
void* zgbmsgprocess(void* argc)
{
	int rcvret;
    ZGBADDRESS src;
    DATA* zgbdata;
    bool needmqtt;
    zgbqueuemsg qmsg;
    char db_zgbaddress[17] = {0};
    char db_deviceid[20] = {0}; 
    char msgtype,devicetype,deviceindex,packetid;
    zgbmsg responsemsg;
    char topic[TOPIC_LENGTH] = { 0 };
    char *zErrMsg = NULL;  
    char sql[512]; 
    BYTE data[72];
    int rc;  
    int len = 0; 

    MYLOG_DEBUG("Enter pthread zgbmsgprocess");

	while(1)
	{
		if (rcvret = msgrcv(g_queueid , (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_ZGB, 0) <= 0)
		{
            MYLOG_ERROR("recive zgbqueuemsg fail!");
            MYLOG_ERROR("rcvret = %d", rcvret);
		}
		MYLOG_INFO("zgbmsgprocess recive a msg");
        memcpy(src, qmsg.msg.payload.src, 8);
        zgbaddresstodbaddress(src, db_zgbaddress);
        MYLOG_ZGBMSG(qmsg.msg);
        MYLOG_INFO("The index is %2x, %2x", qmsg.msg.payload.adf.index[0], qmsg.msg.payload.adf.index[1]);
        if(qmsg.msg.payload.adf.index[0] == 0x00 && qmsg.msg.payload.adf.index[1] == 0x00) //设备入网消息
        {
            int nrow = 0, ncolumn = 0;
	        char **dbresult;
        
            MYLOG_INFO("[ZGB DEVICE]Get a device network joining message.");
            sprintf(sql,"SELECT * FROM devices WHERE zgbaddress = %s;", db_zgbaddress);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
            if(nrow == 0) //数据库中没有该设备
            {
                sendzgbmsg(src, NULL, 0, ZGB_MSGTYPE_DEVICEREGISTER, 0, 0, getpacketid());//要求设备注册
            }
            sqlite3_free_table(dbresult);
            continue;
        }
        
        zgbdata      = &qmsg.msg.payload.adf.data;
        msgtype      = zgbdata->msgtype;
        devicetype   = zgbdata->devicetype;
        deviceindex  = zgbdata->deviceindex;
        packetid     = zgbdata->packetid;
        
        sprintf(db_deviceid, "%s%d", db_zgbaddress, deviceindex);
        
        switch (msgtype)
        {
            case ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE:
            {
                 cJSON* root = cJSON_CreateObject();
                 int nrow = 0, ncolumn = 0;
                 char **azResult; 
                 
                 MYLOG_INFO("[ZGB DEVICE]Get a device network joining response message.");
       
                 sprintf(sql, "insert into devices values(%s, %s, %d, %d, 1);", db_deviceid, db_zgbaddress, devicetype, deviceindex);
                 MYLOG_INFO("The sql is %s", sql);
                 rc = sqlite3_exec(g_db, sql, 0, 0, &zErrMsg);
                 if(rc != SQLITE_OK)
                 {
                     MYLOG_ERROR(zErrMsg);
                     continue;
                 }
                 cJSON* devicestatus = create_device_status_json(db_deviceid, devicetype);
                 cJSON_AddItemToArray(g_devices_status_json, devicestatus);
                 
                 sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_ADD);
                 cJSON_AddStringToObject(root, "deviceid", db_deviceid);
                 cJSON_AddNumberToObject(root, "devicetype", devicetype);
                 sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
                 break;
            }
            case ZGB_MSGTYPE_DEVICE_OPERATION_RESULT:
            {                
                for (int i = 0; i < ZGBMSG_MAX_NUM; ++i)
                {
                    if (g_devicemsgstatus[i].packetid == packetid)
                    {
                        g_devicemsgstatus[i].finish = 1;
                        g_devicemsgstatus[i].result = zgbdata->pdu[1];
                        break;
                    }
                }                              
                break;
            } 
            case ZGB_MSGTYPE_DEVICE_STATUS_REPORT:
            {
                cJSON *device_json = get_device_status_json(db_deviceid, devicetype);
                cJSON *attr_json;
                cJSON *replace_value_json;
                int value;
                int i = 0;
                BYTE attr; 

                if(device_json == NULL)
                {
                    break;
                }
                
                while(i < zgbdata->length)
                {
                    attr = zgbdata->pdu[i++];
                    
                    attr_json = get_attr_value_object_json(device_json, attr);
                    
                    if(device_json == NULL)
                    {
                        break;
                    }
                    
                    switch(attr)
                    {
                        case ATTR_WORKING_STATUS:
                        {
                            value = zgbdata->pdu[i++];
                            replace_value_json = cJSON_CreateNumber(value);
                            cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);
                            needmqtt = true;
                            break;
                        }
                        case ATTR_SOCKET_E:
                        {
                            value = zgbdata->pdu[i++] + 0xFF*(zgbdata->pdu[i++]) + 0xFFFF*(zgbdata->pdu[i++]) + 0xFFFFFF*(zgbdata->pdu[i++]);
                            needmqtt = false;
                            MYLOG_INFO("Get a socket electricity report msg!the value is %lu", value);
                            electricity_stat(db_deviceid, value);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            default: 
                ;
        }
	}
	pthread_exit(NULL);        
}


/*监听串口的进程，提取单片机上传的ZGB消息*/
void* uartlisten(void *argc)
{
	BYTE msgbuf[1024]; 
	pthread_t threads[2];
	int nbyte=0, bitnum=0;
	zgbmsg zmsg;
    zgbqueuemsg zgbqmsg;
	int i, j, sum;
    int ret;

    pthread_create(&threads[0], NULL, uartsend, NULL);
    pthread_create(&threads[1], NULL, zgbmsgprocess, NULL);

    MYLOG_DEBUG("pthread uartlisten begin");
    
	while(1)
	{
		nbyte = read(g_uartfd, msgbuf, 110);
        MYLOG_INFO("Uart recv %d byte:", nbyte);
        MYLOG_BYTE(msgbuf, nbyte);
        bitnum = nbyte;
		for(i = 0; i < bitnum; )
		{
			if(msgbuf[i] != 0x2A)//查找ZGB自定义报文开始字节
			{
				i++;
				continue;
			}

			/*开始提取ZGB消息*/
			zgbmsginit(&zmsg);
			zmsg.msglength = msgbuf[i + 1];
            if(zmsg.msglength+4 > bitnum-i) //如果ZGB消息长度大于接受到字节剩余长度说明还要接受后续字节
            {
                int needbyte = (zmsg.msglength + 4) -  (bitnum - i);//构造当前报文还需接收的字节数
                
                MYLOG_DEBUG("Need %d byte to construct a message!", needbyte);
                
                nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                    
                MYLOG_INFO("Uart extern recv %d byte:", nbyte);
                MYLOG_BYTE(msgbuf+bitnum, nbyte);
                bitnum = bitnum + nbyte;
                
                while(needbyte > nbyte)//循环读取直到全部读取
                {
                    needbyte = needbyte - nbyte;
                    MYLOG_DEBUG("Need %d byte!", needbyte);
                    nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                    MYLOG_INFO("Uart extern recv %d byte:", nbyte);
                    MYLOG_BYTE(msgbuf+bitnum, nbyte); 
                    bitnum = bitnum + nbyte;
                }
                MYLOG_DEBUG("The complete msg is :");
                MYLOG_BYTE(msgbuf+i, zmsg.msglength + 4);
            }
			zmsg.check = msgbuf[i + zmsg.msglength + 2];
			sum = 0;

			//ZGB消息的check校验
			for (j = 0; j < zmsg.msglength; j++)
			{
				sum += msgbuf[i + 2 + j];//check取值为Payload相加值的最低字节
			}
			if (sum%256 != zmsg.check)
			{
                MYLOG_DEBUG("The zmsg.check is %d", zmsg.check);
                MYLOG_DEBUG("The sum is %d,the sum/256 = %d", sum, sum%256);
				MYLOG_ERROR("Wrong format!");
				//i = i + zmsg.msglength + 4;
				i++;
				continue;
			}

			memcpy((void*)zmsg.payload.src, (void*)msgbuf + i + 10, ZGB_ADDRESS_LENGTH); //ZGB消息中src位于报文第10字节
    		if(addresszero(zmsg.payload.src) == 0)//不是ROT发来的报文
    		{
                //移除前一个报文
                if((i + zmsg.msglength + 4)!= (bitnum -1))
                {
                    MYLOG_DEBUG("move a message!");
                    memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                    bitnum = bitnum - (i + zmsg.msglength + 4);
                    i = 0;
                    continue;                    
                }
            }

            zmsg.payload.cmdid[0] = msgbuf[i + (int)&zmsg.payload.cmdid - (int)&zmsg];
            zmsg.payload.cmdid[1] = msgbuf[i + 1 + (int)&zmsg.payload.cmdid - (int)&zmsg];            

            zmsg.payload.adf.index[0] = msgbuf[i + (int)&zmsg.payload.adf.index - (int)&zmsg];
            zmsg.payload.adf.index[1] = msgbuf[i + 1 + (int)&zmsg.payload.adf.index - (int)&zmsg]; 

            if(zmsg.payload.cmdid[0] != 0x20 && zmsg.payload.cmdid[1] == 0x98)//不是ROT发来的透传报文
            {
                //移除前一个报文
                if((i + zmsg.msglength + 4)!= (bitnum -1))
                {
                    MYLOG_DEBUG("move a message!");
                    memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                    bitnum = bitnum - (i + zmsg.msglength + 4);
                    i = 0;
                    continue;                    
                }            
            }

            if(zmsg.payload.adf.index[0] == 0xA0 && zmsg.payload.adf.index[1] == 0x0F)
            {
                zmsg.payload.adf.data.devicetype  = msgbuf[i + (int)&zmsg.payload.adf.data.devicetype - (int)&zmsg];
                zmsg.payload.adf.data.deviceindex = msgbuf[i + (int)&zmsg.payload.adf.data.deviceindex - (int)&zmsg];
    			zmsg.payload.adf.data.packetid    = msgbuf[i + (int)&zmsg.payload.adf.data.packetid - (int)&zmsg];
    			zmsg.payload.adf.data.msgtype     = msgbuf[i + (int)&zmsg.payload.adf.data.msgtype - (int)&zmsg];
                zmsg.payload.adf.data.length      = msgbuf[i + (int)&zmsg.payload.adf.data.length - (int)&zmsg];
    			memcpy((void*)zmsg.payload.adf.data.pdu, (void*)(msgbuf + i + 44), zmsg.payload.adf.data.length);                
            }

            zgbqmsg.msgtype = QUEUE_MSG_ZGB;
            zgbqmsg.msg = zmsg;
            MYLOG_ZGBMSG(zmsg);
          	if (ret = msgsnd(g_queueid, &zgbqmsg, sizeof(zgbqueuemsg), 0) != 0)
        	{
		        MYLOG_ERROR("send zgbqueuemsg fail!");
	        }
            //移除前一个报文
            if((i + zmsg.msglength + 4)!= bitnum)
            {
                MYLOG_DEBUG("move a message!");
                memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                bitnum = bitnum - (i + zmsg.msglength + 4);
                i = 0;                   
            } 
            else
            {
                i = bitnum;
            }
		}
	}
	pthread_exit(NULL);
}

void mqttpub_onFailure(void* context, MQTTAsync_failureData* response)
{
	MYLOG_ERROR("pub msg fail!");
	MYLOG_ERROR("The token:%d", response->token);
	MYLOG_ERROR("The code:%d", response->code);
	MYLOG_ERROR("The token:%s", response->message);
}

void mqtt_onSuccess(void* context, MQTTAsync_successData* response)
{
	MYLOG_INFO("pub msg success!");
}

/*MQTT消息队列的处理进程*/
void* mqttqueueprocess(void *argc)
{
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	mqttqueuemsg msg;
	ssize_t ret;
	int result;

	MQTTAsync_create(&client, ADDRESS, CLIENTID1, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = "root";
	conn_opts.password = "root";
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

	opts.onFailure = mqttpub_onFailure;
	opts.onSuccess = mqtt_onSuccess;
	rc = MQTTAsync_connect(client, &conn_opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MYLOG_ERROR("MQTTClient: MQTT connect fail!");
	}
	MYLOG_DEBUG("enter mqttqueueprocess pthread!");

	/*处理mqtt消息队列*/
	while(1)
	{
		ret = msgrcv(g_queueid, (void*)&msg, sizeof(mqttmsg), QUEUE_MSG_MQTT, 0);
		if(ret == -1)
		{
			MYLOG_ERROR("read mqttmsg fail!");
		}
loop:
		switch(msg.msgtype)
		{
		case MQTT_MSG_TYPE_PUB:
			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, &opts);
			
			if(result != MQTTASYNC_SUCCESS)
			{
                if(result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    MYLOG_ERROR("MQTTAsync_send fail! %d", result);
                }
			}
			break;
		case MQTT_MSG_TYPE_SUB:
			result = MQTTAsync_subscribe(client, (const char*)msg.msg.topic, msg.msg.qos, &opts);
			if(result != MQTTASYNC_SUCCESS)
                if(result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    MYLOG_ERROR("MQTTAsync_subscribe fail! %d", result);
                }
			break;
		case MQTT_MSG_TYPE_UNSUB:
			result = MQTTAsync_unsubscribe(client, (const char*)msg.msg.topic, &opts);
			if(result != MQTTASYNC_SUCCESS)
                if(result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    MYLOG_ERROR("MQTTAsync_unsubscribe fail! %d\n", result);
                }
			break;
		default:
			MYLOG_INFO("unknow msg!\n");
		}
		if(msg.msgtype == MQTT_MSG_TYPE_PUB)
		{
			free(msg.msg.msgcontent); //如果是PUB消息需要把内容指针释放
		}
		free(msg.msg.topic);
	}
	pthread_exit(NULL);
}

/*创建程序需要用到的消息队列，用于MQTT的订阅、发布、去订阅和存取设备操作消息以及zgb消息的存储*/
int createmessagequeue()
{   
	key_t key;
	key = ftok("/", 0);
    
	g_queueid = msgget(key, IPC_CREAT | 0644);
	if (g_queueid < 0)
	{
		MYLOG_ERROR("get ipc_id error!");
		return -1;
	}
    return 0;  
}

/*创建需要的数据库表*/
int sqlitedb_init()
{
    int rc; 
    char sql[512];  

    rc = sqlite3_open("smarthome.db", &g_db);
    if(rc != SQLITE_OK)  
    {  
        MYLOG_ERROR("open smarthome.db error!");  
        return -1;  
    }
    sprintf(sql,"CREATE TABLE devices (deviceid TEXT, zgbaddress TEXT, devicetype CHAR(1), deviceindex CHAR(1), online CHAR(1));");
    exec_sql_create(sql);
    
    sprintf(sql,"create table airconditioning(deviceid INTEGER PRIMARY KEY, status INTEGER,mode INTEGER,temperature float, windspeed INTEGER);");
    exec_sql_create(sql);
    
    sprintf(sql,"CREATE TABLE [electricity_day]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[day] INT NOT NULL, primary key(deviceid, day));");
    exec_sql_create(sql);


    sprintf(sql,"CREATE TABLE [electricity_month]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[month] INT NOT NULL, primary key(deviceid, month));");
    exec_sql_create(sql);

    sprintf(sql,"CREATE TABLE [electricity_year]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[year] INT NOT NULL, primary key(deviceid, year));");
    exec_sql_create(sql);

    sprintf(sql,"CREATE TABLE [electricity_hour]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[hour] INT NOT NULL, primary key(deviceid, hour));");
    exec_sql_create(sql);
    
    return 0;
}


/*局域网监听任务*/
void* lantask(void *argc)
{
    struct sockaddr_in servaddr,appaddr;
    char text[] = "smarthome app";
    int listenfd,sendfd;
    char buf[128];               
                         
    listenfd = socket(AF_INET,SOCK_DGRAM,0);
    sendfd = socket(AF_INET,SOCK_DGRAM,0);
    int set = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int));
    setsockopt(sendfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int));

    bzero(&servaddr,sizeof(servaddr));
    bzero(&appaddr,sizeof(appaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(8787);

    bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));  //端口绑定
    int recvbytes;
    int addrlen = sizeof(struct sockaddr_in);

    while(1)
    {
        if((recvbytes = recvfrom(listenfd, buf, 128, 0, (struct sockaddr *)&appaddr, &addrlen)) != -1)
        {
            buf[recvbytes] = '\0';
            MYLOG_DEBUG("Lantask revc a udp msg!, the msg is %s", buf);
            if(memcmp(buf, text, strlen(text))==0)
            {
                int sendBytes;
                appaddr.sin_family = AF_INET;
                appaddr.sin_addr.s_addr = inet_addr(inet_ntoa(appaddr.sin_addr));
                appaddr.sin_port = htons(8787);  

                MYLOG_DEBUG("Send a msg to app!");
                if((sendBytes = sendto(sendfd, text, strlen(text), 0, (struct sockaddr *)&appaddr, sizeof(struct sockaddr))) == -1)
                {
                    MYLOG_ERROR("sendto fail!");
                }
            }
        }
        else
        {
            MYLOG_ERROR("recvfrom fail\n");
        }
    }
	pthread_exit(NULL);    
}


int main(int argc, char* argv[])
{
    pthread_t threads[THREAD_NUMS];
    /*只运行单例*/
    if (already_running(LOCKFILE))
    {
        return 0;    
    }
    log_init();	

    if(sqlitedb_init() == -1)
    {
        MYLOG_ERROR("Create db failed!");
        return -1;        
    }
    
    init(); //程序启动初始任务
    sem_init(&g_mqttconnetionsem, 0, 1); 

    if(createmessagequeue() == -1)
    {
        MYLOG_ERROR("CreateMessageQueue failed!");//这里后续要添加重启的功能
        return -1;
    }
   
	pthread_create(&threads[0], NULL, mqttlient,        NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten,       NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);
    pthread_create(&threads[4], NULL, lantask,          NULL);

    pthread_exit(NULL);
}


