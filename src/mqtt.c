#include "app.h"
#include "paho/MQTTAsync.h"

extern int g_queueid;
extern int g_operationflag;
extern char* g_topics[TOPICSNUM];
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

    sleep(10);
	
	MQTTAsync_connect(context->handle, &conn_opts);
}


/*MQTT订阅消息到达的处理函数*/
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	int mqttid;
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
		return 1;
	}

    tmp = cJSON_GetObjectItem(root, "mqttid");
    if(tmp == NULL)
    {   
        MYLOG_ERROR("Wrong format MQTT message!");
        MQTTAsync_freeMessage(&message);
        MQTTAsync_free(topicName);
        return 1;        
    }
    mqttid = tmp->valueint;
    sprintf(topic, "%s/response/%d", topicName, mqttid);

    /*设备电量查询*/
    if(strstr(topicName, "electric") != 0)
    {
        MYLOG_INFO("An electric qury!");
        cJSON* t = cJSON_GetObjectItem(root, "type");        
	    if(t == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            goto end;    	        
	    }
	    int type = t->valueint;
	    cJSON* devices = cJSON_GetObjectItem(root, "devices");
	    if(devices == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            goto end;	        
	    }
	    int num = cJSON_GetArraySize(devices);
	    for(int i=0;i<num;i++){
	        cJSON* device = cJSON_GetArrayItem(devices, i);
	        cJSON* deviceidjson = cJSON_GetObjectItem(device, "deviceid");
    	    if(deviceidjson == NULL)
    	    {
                MYLOG_ERROR("Wrong format MQTT message!");
                goto end;	        
    	    }	        
    	    char* deviceid = deviceidjson->valuestring;
    	    char sql[250]={0};   
            int nrow = 0, ncolumn = 0;
            char **dbresult;
            char *zErrMsg = NULL;
            switch(type)
            {
                case 1:
                    sprintf(sql, "select electricity,hour from electricity_hour where deviceid='%s';", deviceid);
                    break;
                case 2:
                    sprintf(sql, "select electricity,day from electricity_day where deviceid='%s';", deviceid);
                    break;
                case 3:
                    sprintf(sql, "select electricity,month from electricity_month where deviceid='%s';", deviceid);
                    break;
                case 4:
                    sprintf(sql, "select electricity,year from electricity_year where deviceid='%s';", deviceid);
                    break;
                default:
                    break;
            }    
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            cJSON* records = cJSON_CreateArray();
            cJSON* record;
            const char* num;
            const char* time;
            for(int i=1;i<nrow;i++)
            {
                record = cJSON_CreateObject();
                num = (const char*)dbresult[i*2];
                time = (const char*)dbresult[i*2+1];
                cJSON_AddNumberToObject(record, "electricity", atoi(num));
                switch (type)
                {
                    case 1:
                        cJSON_AddNumberToObject(record, "hour", atoi(time));
                        break;
                    case 2:
                        cJSON_AddNumberToObject(record, "day", atoi(time));
                        break;                
                    case 3:
                        cJSON_AddNumberToObject(record, "month", atoi(time));
                        break;                
                    case 4:
                        cJSON_AddNumberToObject(record, "year", atoi(time));
                        break;
                    default:
                        break;                
                 
                }
                cJSON_AddItemToArray(records, record);            
            }
            cJSON_AddItemToObject(device, "records", records);	
            sqlite3_free_table(dbresult);
	    }
        sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
		goto end;        	   
    }
    /*设备操作*/
	else if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag) //检查当前是否有msg在处理
		{
			/*如果有消息在处理，则返回忙碌错误*/
			cJSON_AddStringToObject(root, "result", MQTT_MSG_SYSTEM_BUSY);
			cJSON_AddNumberToObject(root, "resultcode", MQTT_MSG_ERRORCODE_BUSY);
			sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
			goto end;
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
                goto end;
			}			
		}
	}
	/*网关操作*/
	else if(strstr(topicName, "gateway") != 0)
	{
	    cJSON* op = cJSON_GetObjectItem(root, "operation");
	    if(op == NULL)
	    {
            MYLOG_ERROR("Wrong format MQTT message!");
            goto end;       
	    }
	    
	    int operationtype = op->valueint;
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
        else if(operationtype == 4)//清空日志
        {
            fclose(g_fp);
            int fd = open(LOG_FILE, O_RDWR | O_TRUNC);
            log_init();
        }        
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
    //MQTTAsync client = clicontext->handle;
	MYLOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
	//MQTTAsync_reconnect(client);
	mqtt_reconnect(clicontext);

	return;
}


static void connectlost(void *context, char *cause)
{
    Clientcontext* clicontext = (Clientcontext*)context;
    MQTTAsync client = clicontext->handle;
	MYLOG_ERROR("Connection lost,the cause is %s", cause);
	//MQTTAsync_reconnect(client);
	mqtt_reconnect(clicontext);

	return;
}

static void connectsuccess(void* context, MQTTAsync_successData* response)
{
    Clientcontext* clicontext = (Clientcontext*)context;
    MQTTAsync client = clicontext->handle;
    int clientid = clicontext->clientid;

    if((clientid == WAN_CLIENT_PUB_ID) || (clientid == LAN_CLIENT_PUB_ID)) //发布进程不需要订阅topic
        return;

    int qoss[TOPICSNUM] = {2, 1};
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
void *mqttclient(void *argc)
{  
    MQTTAsync client;
    Clientcontext context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

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
	int result;

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
    int nrow = 0, ncolumn = 0;
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
    //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
    for(int i=1;i<=nrow;i++)
    {
        root = cJSON_CreateObject();
        deviceid      = azResult[i*ncolumn];
        devicetype    = atoi(azResult[i*ncolumn+2]); 
        cJSON_AddStringToObject(root, "deviceid", deviceid);
        cJSON_AddNumberToObject(root, "devicetype", devicetype);        
        sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
        cJSON_Delete(root);
    } 
    sqlite3_free_table(azResult);      
}
