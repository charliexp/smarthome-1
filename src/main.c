#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <sqlite3.h> 

#include "paho/MQTTAsync.h"
#include "utils/utils.h"
#include "device/device.h"
#include "cjson/cJSON.h"


char g_topicroot[20] = {0};
char g_mac[20] = {0};
ZGB_MSG_STATUS g_zgbmsg[ZGBMSG_MAX_NUM];
int g_zgbmsgnum;
cJSON* g_device;
int g_uartfd;


int g_operationflag = 0;
int g_queueid;
sem_t g_mqttconnetionsem;
//订阅g_topics对应的qos
int g_qoss[TOPICSNUM] = {2, 1, 2, 1, 1};
//程序启动后申请堆存放需要订阅的topic
char* g_topics[TOPICSNUM] ={0x0, 0x0, 0x0, 0x0, 0x0};
char g_topicthemes[TOPICSNUM][10] = {{"operation"}, {"update"}, {"device"}, {"config"}, {"warning"}};

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

void constructsubtopics()
{
	int i = 0;

    //获取网关mac地址
	if(getmac(g_mac) == 0)
	{
        sprintf(g_topicroot, "/%s/", g_mac);    
	}


	for(; i < TOPICSNUM; i++)
	{
		g_topics[i] = (char*)malloc(sizeof(g_topicroot) + sizeof(g_topicthemes[i]) + strlen("/+"));
		sprintf(g_topics[i], "%s%s/+", g_topicroot, g_topicthemes[i]);
	}
}

/*messagetype: 1、发布 2、订阅 3、去订阅*/
void mqttmsgqueue(long messagetype, char* topic, char* message, int qos, int retained)
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
		printf("send mqttqueuemsg fail!\n");
		return;
	}

	return;
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	printf("Successful disconnection\n");
}


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

    printf("Message arrived\n");
    printf("topic: %s\n", topicName);

    payloadptr = (char*)message->payload;
	root = cJSON_Parse(payloadptr);

	if (root == NULL)
	{
		perror("Wrong msg!\n");
		return 0;
	}

	if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag)
		{
			/*如果有消息在处理，则返回忙碌错误*/
			mqttid = cJSON_GetObjectItem(root, "mqttid")->valuestring;
			sprintf(topic, "%s%s/result/%s", g_topicroot, g_topicthemes[0], mqttid);
			mqttmsgqueue(MQTT_MSG_TYPE_PUB, topic, MQTT_MSG_SYSTEM_BUSY, 2, 0);
		}
		else
		{
			/*将操作消息json链表的root封装消息丢入消息队列*/
			g_operationflag = 1;
			printf("get an operation msg.\n");

			msg.msgtype = QUEUE_MSG_DEVIC;
			msg.p_operation_json = cJSON_Duplicate(root, 1);

			if (sendret = msgsnd(g_queueid, (void*)&msg, msglen, 0) != 0)
			{
				LOG_ERROR("sendret = %d", sendret);
				LOG_ERROR("send devicequeuemsg fail!");
				return 0;
			}
			cJSON_Delete(root);
		}
	}
	else if(strstr(topicName, "update") != 0)
	{
	
	}
	else if(strstr(topicName, "device") != 0)
	{
	}
	
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void onSend(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
	int rc;

	printf("Message with token value %d delivery confirmed\n", response->token);

	opts.onSuccess = onDisconnect;
	opts.context = client;

	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
	{
		LOG_ERROR("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	LOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
    sem_post(&g_mqttconnetionsem);
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	LOG_ERROR("Connection lost");
	LOG_ERROR("cause: %s", cause);
	LOG_ERROR("Reconnecting");

	sem_post(&g_mqttconnetionsem);
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	pthread_t pthread;
	int rc;
	LOG_DEBUG("Successful connection");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, g_qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		LOG_ERROR("sub error!\n");
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

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = "root";
	conn_opts.password = "root";
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

    while(1)
	{
        sem_wait(&g_mqttconnetionsem);
        rc = MQTTAsync_connect(client, &conn_opts);
        if (rc != MQTTASYNC_SUCCESS)
        {
            LOG_ERROR("MyMQTTClient: MQTT connect fail!");
        }
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

/*设备消息的处理进程*/
void* devicemsgprocess(void *argc)
{
	devicequeuemsg msg;
	char topic[TOPIC_LENGTH] = { 0 };
	char mqttid[16] = {0};
	cJSON *devices, *device;
	int devicenum = 0;
	int i = 0;
	int rcvret;
	char packetid;
	char devicetype;
	size_t msglen = sizeof(devicequeuemsg);
    

	while(1)
	{
		if (rcvret = msgrcv(g_queueid, (void*)&msg, msglen, QUEUE_MSG_DEVIC, 0) <= 0)
		{
			printf("rcvret = %d", rcvret);
			perror("");
			printf("recive devicequeuemsg fail!\n");
			pthread_exit(NULL);
		}
		LOG_INFO("devicemsgprocess recive a msg");
		LOG_INFO("the msg is %s", cJSON_PrintUnformatted(msg.p_operation_json));
		g_device = msg.p_operation_json;
		devices = cJSON_GetObjectItem(g_device, "device");
		devicenum = cJSON_GetArraySize(devices);

		for (i=0; i< devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);
			if (device == NULL)
			{
				LOG_ERROR("error");
			}
			packetid = getpacketid();
			g_zgbmsg[i].packetid = packetid;
			g_zgbmsg[i].over = 0;
			g_zgbmsgnum++;

			devicetype = cJSON_GetObjectItem(device, "type")->valueint;
			if (device == NULL)
			{
				LOG_ERROR("error");
			}

			switch (devicetype)
			{
			case DEV_SOCKET:
				socketcontrol(device, packetid);
				break;
			case DEV_AIR_CON:
				airconcontrol(device, packetid);
				break;
			case DEV_FRESH:
				freshaircontrol(device, packetid);
				break;
			default:
				break;
			}
		}

		for (i=0; i<RESPONSE_WAIT/50000; i++)
		{
			int j = 0;
			for (; j < g_zgbmsgnum; j++)
			{
				if (g_zgbmsg[j].over == 0)
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
			if (device == NULL)
			{
				LOG_ERROR("error");
				continue;
			}
			cJSON_AddNumberToObject(device, "result", g_zgbmsg[i].result);
		}
		strncpy(mqttid, cJSON_GetObjectItem(msg.p_operation_json, "mqttid")->valuestring, 16);
		sprintf(topic, "%s%s/result/%s", g_topicroot, g_topicthemes[0], mqttid);
		mqttmsgqueue(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(msg.p_operation_json), 2, 0);
		cJSON_Delete(msg.p_operation_json);
		g_operationflag = 0;
	}
	pthread_exit(NULL);
}

void* uartsend(void *argc)
{    
    uartsendqueuemsg qmsg;    
    int rcvret;
    
    LOG_DEBUG("Enter pthread uartsend");
    while(true)
    {
		if ( rcvret = msgrcv(g_queueid, (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_UART, 0) <= 0 )
		{
			LOG_ERROR("rcvret = %d", rcvret);
			LOG_ERROR("recive uartsendmsg fail!");
		}
        printf("uart begin to send msg!");
        if ( write(g_uartfd, (char *)&qmsg.msg, (int)(qmsg.msg.msglength + 2)) != (qmsg.msg.msglength + 2))
	    {
		    LOG_ERROR("com write error!");
	    }
	    if ( write(g_uartfd, (char *)&qmsg.msg+sizeof(qmsg.msg)-2, 2) != 2)//发送check和footer
	    {
		    LOG_ERROR("com write check and footer error!");
	    }
    }
	pthread_exit(NULL);    
}

/*zgb消息处理*/
void* zgbmsgprocess(void* argc)
{
	int rcvret;
    zgbqueuemsg qmsg;
    char packetid;

    LOG_DEBUG("Enter pthread zgbmsgprocess");
	while(1)
	{
		if (rcvret = msgrcv(g_queueid, (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_ZGB, 0) <= 0)
		{
                LOG_ERROR("recive zgbqueuemsg fail!");
                LOG_ERROR("rcvret = %d", rcvret);
		}
		LOG_INFO("zgbmsgprocess recive a msg");

        if (qmsg.msg.payload.adf.data.msgtype == ZGB_MSGTYPE_DEVICE_OPERATION_RESULT)
        {
            packetid = qmsg.msg.payload.adf.data.packetid;
            for (int i = 0; i < ZGBMSG_MAX_NUM; ++i)
            {
                if (g_zgbmsg[i].packetid == packetid)
                {
                    g_zgbmsg[i].over = 1;
                    g_zgbmsg[i].result = *((char*)qmsg.msg.payload.adf.data.pdu + 1);
                    break;
                }
            }
        }
        else if(qmsg.msg.payload.adf.data.msgtype == ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE)
        {
            
        }
	}
    
	pthread_exit(NULL);    
    
}


/*监听串口的进程，提取单片机上传的zgb消息*/
void* uartlisten(void *argc)
{
	char msgbuf[1024]; //暂时使用1024字节存储串口数据，后续测试读取时串口最大数据量
	pthread_t threads[2];
	int nByte;
	int bitflag;
	zgbmsg zmsg;
    zgbqueuemsg zgbqmsg;
	int i, j, sum;
    int ret;

    pthread_create(&threads[0], NULL, uartsend, NULL);
    pthread_create(&threads[1], NULL, zgbmsgprocess, NULL);

    LOG_DEBUG("Enter pthread uartlisten");
	while (true)
	{
		nByte = read(g_uartfd, msgbuf, 1024);
        LOG_INFO("Uart recv %d byte", nByte);
        
		for (i = 0; i < nByte; )
		{
			if (msgbuf[i] != 0x2A)
			{
				i++;
				continue;
			}
			if (msgbuf[i + 1] > nByte - 4)
			{
				LOG_ERROR("[uartlisten]Wrong format!");
				break;
			}
			//zgb消息提取
			zgbmsginit(&zmsg);
			zmsg.msglength = msgbuf[i + 1];
			zmsg.check = msgbuf[i + zmsg.msglength + 2];
			sum = 0;

			//zgb消息的check校验
			for (j = 0; j < zmsg.msglength; j++)
			{
				sum += msgbuf[i + j + 2];
			}
			if (sum%256 != zmsg.check)
			{
				LOG_ERROR("[uartlisten]Wrong format!");
				i = i + zmsg.msglength + 4;
				continue;
			}

			strncpy((char *)zmsg.payload.src, msgbuf + 10, ZGB_ADDRESS_LENGTH);
            zmsg.payload.adf.data.devicetype = msgbuf[i + (int)&zmsg.payload.adf.data.devicetype - (int)&zmsg];
            zmsg.payload.adf.data.deviceindex = msgbuf[i + (int)&zmsg.payload.adf.data.deviceindex - (int)&zmsg];
			zmsg.payload.adf.data.packetid = msgbuf[i + (int)&zmsg.payload.adf.data.packetid - (int)&zmsg];
			zmsg.payload.adf.data.msgtype = msgbuf[i + (int)&zmsg.payload.adf.data.msgtype - (int)&zmsg];
            zmsg.payload.adf.data.length = msgbuf[i + (int)&zmsg.payload.adf.data.length - (int)&zmsg];
			strncpy(zmsg.payload.adf.data.pdu, msgbuf + i + 40, zmsg.payload.adf.data.length);

            zgbqmsg.msgtype = QUEUE_MSG_ZGB;
            zgbqmsg.msg = zmsg;
          	if (ret = msgsnd(g_queueid, &zgbqmsg, sizeof(zgbqueuemsg), 0) != 0)
        	{
		        LOG_ERROR("send zgbqueuemsg fail!");
	        }
            i = i + zmsg.msglength + 4;
		}
	}
	pthread_exit(NULL);
}

void mqttpub_onFailure(void* context, MQTTAsync_failureData* response)
{
	printf("pub msg fail!\n");
	printf("The token:%d\n", response->token);
	printf("The code:%d\n", response->code);
	printf("The token:%s\n", response->message);
}

void mqtt_onSuccess(void* context, MQTTAsync_successData* response)
{
	printf("pub msg success!\n");
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
		printf("MyMQTTClient: MQTT connect fail!\n");
	}
	printf("enter mqttqueueprocess pthread!\n");

	/*处理mqtt消息队列*/
	while (1)
	{
		ret = msgrcv(g_queueid, (void*)&msg, sizeof(mqttmsg), QUEUE_MSG_MQTT, 0);
		if (ret == -1)
		{
			perror("read mqttmsg fail!\n");
		}
loop:
		switch (msg.msgtype)
		{
		case MQTT_MSG_TYPE_PUB:
			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, &opts);
			
			if (result != MQTTASYNC_SUCCESS)
			{
                if (result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    printf("MQTTAsync_send fail! %d\n", result);
                }
			}
			break;
		case MQTT_MSG_TYPE_SUB:
			result = MQTTAsync_subscribe(client, (const char*)msg.msg.topic, msg.msg.qos, &opts);
			if (result != MQTTASYNC_SUCCESS)
                if (result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    printf("MQTTAsync_subscribe fail! %d\n", result);
                }

			break;
		case MQTT_MSG_TYPE_UNSUB:
			result = MQTTAsync_unsubscribe(client, (const char*)msg.msg.topic, &opts);
			if (result != MQTTASYNC_SUCCESS)
                if (result == MQTTASYNC_DISCONNECTED) //如果连接断掉，重连并重发消息
                {
                    while(MQTTAsync_connect(client, &conn_opts) != MQTTASYNC_SUCCESS)
                    {
                        milliseconds_sleep(1000);
                    }
                    goto loop;
                }
                else
                {
				    printf("MQTTAsync_unsubscribe fail! %d\n", result);
                }

			break;
		default:
			printf("unknow msg!\n");
		}
		if (msg.msgtype == MQTT_MSG_TYPE_PUB)
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
		printf("get ipc_id error!\n");
		return -1;
	}
    return 0;  
}

/*创建需要的数据库表*/
int sqlitedb_init()
{
    sqlite3 *db;
    char *zErrMsg = 0;  
    char sql[128];  
    int rc;  
    int len = 0; 

    rc = sqlite3_open("mydb", &db);
    if(rc != SQLITE_OK)  
    {  
        printf("zErrMsg = %s\n",zErrMsg);  
        return -1;  
    }
    sprintf(sql,"create table device(address varchar(8),type INTEGER,status INTEGER,online INTEGER);");
    rc = sqlite3_exec(db,sql,0,0,&zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ERROR) //表重复会返回SQLITE_ERROR，该错误属于正常
    {
        printf("zErrMsg = %s rc =%d\n",zErrMsg, rc);  
        return -1;          
    }
    sprintf(sql,"create table airconditioning(address varchar(8),status INTEGER,mode INTEGER,temperature float, windspeed INTEGER);");
    sqlite3_exec(db,sql,0,0,&zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ERROR)
    {
        printf("zErrMsg = %s\n",zErrMsg);
        return -1;          
    }
    return 0;
}

int main(int argc, char* argv[])
{
    pthread_t threads[NUM_THREADS];
    
    log_init();
    if(init_uart("/dev/ttyS1") == -1)
    {
        LOG_ERROR("init_uart fail!");
    }
    sem_init(&g_mqttconnetionsem, 0, 1); 

    if(createmessagequeue() == -1)
    {
        LOG_ERROR("CreateMessageQueue failed!");
        return -1;
    }
    if(sqlitedb_init() == -1)
    {
        LOG_ERROR("Create db failed!");
        return -1;        
    }
    
	constructsubtopics();
    
	pthread_create(&threads[0], NULL, mqttlient, NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten, NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);

    pthread_exit(NULL);
}


