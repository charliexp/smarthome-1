#include "app.h"
#include "paho/MQTTAsync.h"

extern int g_queueid;
extern int g_operationflag;
extern char* g_topics[TOPICSNUM];
extern char g_clientid[30], g_clientid_pub[30];

static void connectfailure(void* context, MQTTAsync_failureData* response);
static void connectsuccess(void* context, MQTTAsync_successData* response);
static void connectlost(void *context, char *cause);


/*messagetype: 1、发布 2、订阅 3、去订阅*/
void sendmqttmsg(long messagetype, char* topic, char* message, int qos, int retained)
{
	int ret;
	size_t msglen;
	mqttqueuemsg msg = { 0 };
	msg.msgtype = QUEUE_MSG_MQTT;
	msg.msg.qos = qos;
	msg.msg.retained = retained;
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


void mqtt_reconnect(MQTTAsync handle)
{
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = handle;
	
	MQTTAsync_connect(handle, &conn_opts);
}


/*MQTT订阅消息到达的处理函数*/
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	char* mqttid;
	cJSON *root, *tmp;
	devicequeuemsg msg;
	int sendret;
	size_t msglen = sizeof(devicequeuemsg);
	char topic[TOPIC_LENGTH] = { 0 };

    MYLOG_INFO("Message arrived");
    MYLOG_INFO("Topic: %s", topicName);
    
	root = cJSON_Parse((char*)message->payload);
    MYLOG_INFO("Message: %s", cJSON_Print(root));

	if (root == NULL)
	{
		MYLOG_ERROR("Wrong msg!");
		return 0;
	}

    tmp = cJSON_GetObjectItem(root, "mqttid");
    if(tmp == NULL)
    {   
        MYLOG_ERROR("Wrong format MQTT message!");
        MQTTAsync_freeMessage(&message);
        MQTTAsync_free(topicName);
        return 1;        
    }
    mqttid = tmp->valuestring;
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


static void connectfailure(void* context, MQTTAsync_failureData* response)
{
    MQTTAsync client = (MQTTAsync)context;
	MYLOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
	//MQTTAsync_reconnect(client);
	mqtt_reconnect(client);

	return;
}


static void connectlost(void *context, char *cause)
{
    MQTTAsync client = (MQTTAsync)context;
	MYLOG_ERROR("Connection lost,the cause is %s", cause);
	//MQTTAsync_reconnect(client);
	mqtt_reconnect(client);

	return;
}

static void connectsuccess(void* context, MQTTAsync_successData* response)
{
    int qoss[TOPICSNUM] = {2, 1};
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;
	MYLOG_DEBUG("Successful connection");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		MYLOG_ERROR("sub error!");
	}

	return;
}

/*MQTT客户端进程*/
void *mqttlient(void *argc)
{  
    MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	MQTTAsync_create(&client, ADDRESS, g_clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = client;

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
	int rc;

	MQTTAsync_create(&client, LAN_MQTT_SERVER, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	//conn_opts.username = USERNAME;
	//conn_opts.password = PASSWORD;
	conn_opts.onSuccess = connectsuccess;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = client;

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
	int rc;

	MQTTAsync_create(&client, ADDRESS, g_clientid_pub, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = client;

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
	int result;

	MYLOG_DEBUG("Enter lanmqttqueueprocess pthread!");

    MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	MQTTAsync_create(&client, ADDRESS, g_clientid_pub, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connectlost, msgarrvd, NULL);  

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 0;
	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.onFailure = connectfailure;
	conn_opts.context = client;	

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
