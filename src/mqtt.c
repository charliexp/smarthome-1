#include "app.h"
#include "paho/MQTTAsync.h"

extern int g_queueid,g_zgbqueueid;
extern char g_topics[TOPICSNUM][50];
extern char g_clientid[30], g_clientid_pub[30];
extern char g_topicroot[20];
extern sqlite3* g_db;

extern FILE* g_fp;

static void connectfailure(void* context, MQTTAsync_failureData* response);
static void connectsuccess(void* context, MQTTAsync_successData* response);
static void connectlost(void *context, char *cause);

typedef struct
{
    /*用于识别哪个mqtt client*/
    int clientid; 
    MQTTAsync handle;
}Clientcontext;

/*messagetype: 1、发布 2、订阅 3、去订阅*/
void sendmqttmsg(long messagetype, char* topic, char* message, int qos, int retained)
{
	int ret;
	size_t msglen;
	mqttqueuemsg msg = { 0 };
	msg.msgtype = QUEUE_MSG_MQTT;
	msg.msg.qos = qos;
	msg.msg.retained = retained;

    if(topic == NULL || message == NULL){
        return;
    }
	
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
		free(msg.msg.topic);
		free(msg.msg.msgcontent);
		return;
	}

	return;
}


void mqtt_reconnect(Clientcontext* context)
{
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = context;

    sleep(5);
	
	MQTTAsync_connect(context->handle, &conn_opts);
}


/*MQTT订阅消息到达的处理函数*/
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	int mqttid;
	cJSON *root, *tmp;
	devicequeuemsg msg;
	int sendret, ret;
	size_t msglen = sizeof(devicequeuemsg);
	char topic[TOPIC_LENGTH] = { 0 };
	int devicetype;

    MYLOG_INFO("MQTT Message arrived");
    MYLOG_INFO("Topic: %s", topicName);
    
	root = cJSON_Parse((char*)message->payload);

	if(root == NULL)
	{
		MYLOG_ERROR("Wrong msg format!");
        goto end;
	}
    MYLOG_INFO("Message: %s", cJSON_Print(root));

    tmp = cJSON_GetObjectItem(root, "mqttid");
    if(tmp == NULL)
    {   
        MYLOG_ERROR("Wrong format MQTT message, need mqttid!");
        goto end;        
    }
    mqttid = tmp->valueint;
    sprintf(topic, "%s/response/%d", topicName, mqttid);

    /*设备电量查询*/
    if(strstr(topicName, "electric") != 0)
    {
        ret = electricity_query(root, topic);
    }
    /*设备电量查询*/
    else if(strstr(topicName, "wateryield") != 0)
    {
        ret = wateryield_query(root, topic);      	   
    }
    /*设备操作或查询*/
	else if (strstr(topicName, "operation") != 0)
	{
        MYLOG_INFO("get an operation msg.");
        
        msg.msgtype = QUEUE_MSG_DEVIC;
        msg.p_operation_json = cJSON_Duplicate(root, 1);
        
        if (sendret = msgsnd(g_queueid, (void*)&msg, msglen, 0) != 0)
        {
            MYLOG_ERROR("sendret = %d", sendret);
            MYLOG_ERROR("send devicequeuemsg fail!");
        }
	}
	/*调试操作*/
	else if(strstr(topicName, "gateway") != 0)
	{
       ret = debugproc(root, topic);
	}
end:
    cJSON_Delete(root);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    
    return 1;
}


static void connectfailure(void* context, MQTTAsync_failureData* response)
{
    Clientcontext* clicontext = (Clientcontext*)context;
    int clientid = clicontext->clientid;   
	MYLOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
	if(clientid == WAN_CLIENT_ID)	
    	ledcontrol(NET_LED, LED_ACTION_OFF, 0);//熄灭NET LED灯

	mqtt_reconnect(clicontext);

	return;
}


static void connectlost(void *context, char *cause)
{
    Clientcontext* clicontext = (Clientcontext*)context;
    MQTTAsync client = clicontext->handle;
    int clientid = clicontext->clientid;    
	MYLOG_ERROR("Connection lost,the cause is %s", cause);

	if(clientid == WAN_CLIENT_ID)
    	ledcontrol(NET_LED, LED_ACTION_OFF, 0);//熄灭NET LED灯

	mqtt_reconnect(clicontext);

	return;
}

static void connectsuccess(void* context, MQTTAsync_successData* response)
{
    Clientcontext* clicontext = (Clientcontext*)context;
    MQTTAsync client = clicontext->handle;
    int clientid = clicontext->clientid;
	int rc;

    if((clientid == WAN_CLIENT_PUB_ID) || (clientid == LAN_CLIENT_PUB_ID)) //发布进程不需要订阅topic
        return;

    int qoss[TOPICSNUM] = {2, 1};
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MYLOG_DEBUG("Successful connection");
	if(clientid == WAN_CLIENT_ID)
    	ledcontrol(NET_LED, LED_ACTION_ON, 0);//点亮NET LED灯

    char* topic[TOPICSNUM];

    for(int i=0;i<TOPICSNUM;i++)
    {
        topic[i] = (char*)&g_topics[i];
    }
    
	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, (char * const*)topic, qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MYLOG_ERROR("sub error!");
	}

	return;
}

/*MQTT客户端进程*/
void *mqttclient(void *argc)
{  
    MQTTAsync client;
    Clientcontext context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

    MYLOG_INFO("Thread mqttclient begin!");
	MQTTAsync_create(&client, ADDRESS, g_clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	context.clientid = WAN_CLIENT_ID;
    context.handle = client;
    
	MQTTAsync_setCallbacks(client, (void*)&context, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = (void*)&context;

    while(1)
	{
        rc = MQTTAsync_connect(client, &conn_opts);
        pause();
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}


/*局域网MQTT客户端进程*/
void *lanmqttlient(void *argc)
{
    MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	Clientcontext context;
	int rc;

	MQTTAsync_create(&client, LAN_MQTT_SERVER, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	context.clientid = LAN_CLIENT_ID;
	context.handle = client;

	MQTTAsync_setCallbacks(client, (void*)&context, connectlost, msgarrvd, NULL);  
	
	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = (void*)&context;

    while(1)
	{
        MQTTAsync_connect(client, &conn_opts);
        pause();
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

/*MQTT消息队列的处理进程*/
void* mqttqueueprocess(void *argc)
{   
	mqttqueuemsg msg;
	ssize_t ret;
	int result;

	MYLOG_DEBUG("mqttqueueprocess pthread begin!");
	milliseconds_sleep(2000);

    MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	Clientcontext context;
	int rc;

	MQTTAsync_create(&client, ADDRESS, g_clientid_pub, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	context.clientid = WAN_CLIENT_PUB_ID;
	context.handle = client;

	MQTTAsync_setCallbacks(client, (void*)&context, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = (void*)&context;

    MQTTAsync_connect(client, &conn_opts);
	/*处理mqtt消息队列*/
	while(1)
	{
		ret = msgrcv(g_queueid, (void*)&msg, sizeof(mqttmsg), QUEUE_MSG_MQTT, 0);
		if(ret == -1)
		{
			MYLOG_ERROR("read mqttmsg fail!");
		}

		switch(msg.msgtype)
		{
    		case MQTT_MSG_TYPE_PUB:
    		    MYLOG_INFO("A msg need to pub.");
    			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
    				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, NULL);
    			
    			if(result != MQTTASYNC_SUCCESS)
    			{		
                    MYLOG_ERROR("MQTTAsync_send fail! %d", result);
    			}
    			break;
    		case MQTT_MSG_TYPE_SUB:
    		    MYLOG_INFO("A msg need to sub.");
    			result = MQTTAsync_subscribe(client, (const char*)msg.msg.topic, msg.msg.qos, NULL);
    			if(result != MQTTASYNC_SUCCESS)
    			{		
                    MYLOG_ERROR("MQTTAsync_subscribe fail! %d", result);
                }
    			break;
    		case MQTT_MSG_TYPE_UNSUB:
    		    MYLOG_INFO("A msg need to unsub.");
    			result = MQTTAsync_unsubscribe(client, (const char*)msg.msg.topic, NULL);
    			if(result != MQTTASYNC_SUCCESS)
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
	MQTTAsync_destroy(&client);
	pthread_exit(NULL);
}


/*局域网MQTT消息队列的处理进程*/
void* lanmqttqueueprocess(void *argc)
{
	mqttqueuemsg msg;
	ssize_t ret;
	ssize_t result;

	MYLOG_DEBUG("Enter lanmqttqueueprocess pthread!");

    MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;
    Clientcontext context;
    
	context.clientid = LAN_CLIENT_PUB_ID;
	context.handle = client;

	MQTTAsync_create(&client, ADDRESS, g_clientid_pub, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, (void*)&context, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = (void*)&context;	

	/*处理mqtt消息队列*/
	while(1)
	{
		ret = msgrcv(g_queueid, (void*)&msg, sizeof(mqttmsg), QUEUE_MSG_MQTT, 0);
		if(ret == -1)
		{
			MYLOG_ERROR("read mqttmsg fail!");
		}
		
		switch(msg.msgtype)
		{
    		case MQTT_MSG_TYPE_PUB:
    		    MYLOG_INFO("A msg need to pub.");
    			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
    				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, NULL);
    			
    			if(result != MQTTASYNC_SUCCESS)
    			{		
                    MYLOG_ERROR("MQTTAsync_send fail! %d", result);
    			}
    			break;
    		case MQTT_MSG_TYPE_SUB:
    		    MYLOG_INFO("A msg need to sub.");
    			result = MQTTAsync_subscribe(client, (const char*)msg.msg.topic, msg.msg.qos, NULL);
    			if(result != MQTTASYNC_SUCCESS)
    			{		
                    MYLOG_ERROR("MQTTAsync_subscribe fail! %d", result);
                }
    			break;
    		case MQTT_MSG_TYPE_UNSUB:
    		    MYLOG_INFO("A msg need to unsub.");
    			result = MQTTAsync_unsubscribe(client, (const char*)msg.msg.topic, NULL);
    			if(result != MQTTASYNC_SUCCESS)
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

//上报所有已注册设备
void reportdevices()
{
    size_t nrow = 0,ncolumn = 0;
    char **azResult; 
    char *zErrMsg = NULL; 
    char sql[256] = {0};
    char* deviceid = {0}; 
    char devicetype;
    char topic[TOPIC_LENGTH] = {0};
    cJSON* root;
    
    sprintf(sql,"SELECT * FROM devices;");
    MYLOG_INFO(sql);
            
    sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_ADD);
    sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
    
    for(int i=1;i<=nrow;i++)
    {
        root = cJSON_CreateObject();
        deviceid      = azResult[i*ncolumn];
        devicetype    = atoi(azResult[i*ncolumn+2]); 
        cJSON_AddStringToObject(root, "deviceid", deviceid);
        cJSON_AddNumberToObject(root, "devicetype", devicetype);        
        sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
        cJSON_Delete(root);
    }
    sqlite3_free_table(azResult);
	sqlite3_free(zErrMsg);
}
