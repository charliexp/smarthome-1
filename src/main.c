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
#include "log/log.h"


char g_topicroot[20] = {0};
char g_mac[20] = {0};
ZGB_MSG_STATUS g_zgbmsg[ZGBMSG_MAX_NUM];
int g_zgbmsgnum;
cJSON* g_device;
int g_uartfd;
int g_log_level = 0;

int g_operationflag = 0;
int g_queueid;
sqlite3* g_db;
sem_t g_mqttconnetionsem;
//����g_topics��Ӧ��qos
int g_qoss[TOPICSNUM] = {2, 1, 2, 1, 1};
//��������������Ѵ����Ҫ���ĵ�topic
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

    //��ȡ����mac��ַ
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

/*messagetype: 1������ 2������ 3��ȥ����*/
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

	if (root == NULL)
	{
		MYLOG_ERROR("Wrong msg!");
		return 0;
	}

	if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag)
		{
			/*�������Ϣ�ڴ����򷵻�æµ����*/
			mqttid = cJSON_GetObjectItem(root, "mqttid")->valuestring;
			sprintf(topic, "%s%s/result/%s", g_topicroot, g_topicthemes[0], mqttid);
			sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, MQTT_MSG_SYSTEM_BUSY, QOS_LEVEL_2, 0);
		}
		else
		{
			/*��������Ϣjson�����root��װ��Ϣ������Ϣ����*/
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
	else if(strstr(topicName, "update") != 0)
	{
	
	}
	else if(strstr(topicName, "coo") != 0)
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

	MYLOG_INFO("Message with token value %d delivery confirmed", response->token);

	opts.onSuccess = onDisconnect;
	opts.context = client;

	if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
	{
		MYLOG_ERROR("Failed to start sendMessage, return code %d", rc);
		exit(EXIT_FAILURE);
	}
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	MYLOG_ERROR("Connect failed, rc %d", response ? response->code : 0);
    sem_post(&g_mqttconnetionsem);
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	MYLOG_ERROR("Connection lost");
	MYLOG_ERROR("cause: %s", cause);
	MYLOG_ERROR("Reconnecting");

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

/*MQTT������Ϣ������Ϣ�Ľ���*/
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
            MYLOG_ERROR("MyMQTTClient: MQTT connect fail!");
        }
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

/*�豸��Ϣ�Ĵ������*/
void* devicemsgprocess(void *argc)
{
	devicequeuemsg msg;
	char topic[TOPIC_LENGTH] = { 0 };
	int mqttid = {0};
	cJSON *devices, *device, *operations, *operation, *tmp;
	int devicenum = 0, operationnum = 0;
	int i = 0;
	int rcvret;
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
			pthread_exit(NULL);
		}
        
		MYLOG_INFO("devicemsgprocess recive a msg");
		MYLOG_INFO("the msg is %s", cJSON_PrintUnformatted(msg.p_operation_json));
		g_device = msg.p_operation_json;
		devices = cJSON_GetObjectItem(g_device, "device");
		if (devices == NULL)
		{
			MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
            cJSON_AddStringToObject(g_device, "result", MQTT_MSG_FORMAT_ERROR);
            goto response;
		}        
		devicenum = cJSON_GetArraySize(devices);

		for (i=0; i< devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);

			packetid = getpacketid();
			g_zgbmsg[i].packetid = packetid;
			g_zgbmsg[i].over = 0;
			g_zgbmsgnum++;
            
            tmp = cJSON_GetObjectItem(device, "deviceid");
            if(tmp == NULL)
		    {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
			}                
            
			deviceid = tmp->valueint;
			if (deviceid == NULL)
			{
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
			}
            
            if (deviceid == 0) //��Ҫ����COOģ��
            {
                int actiontype;
                tmp = cJSON_GetObjectItem(device, "operations");
			    if (tmp == NULL)
			    {
                    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device, "result", MQTT_MSG_FORMAT_ERROR);
                    goto response;
			    }                
                actiontype = tmp->valueint;

                switch (actiontype)
                {
                    case TYPE_CREATE_NETWORK:
                        send(g_uartfd, AT_CREATE_NETWORK, strlen(AT_CREATE_NETWORK));
                        break;
                    case TYPE_OEPN_NETWORK:
                        send(g_uartfd, AT_OEPN_NETWORK, strlen(AT_OEPN_NETWORK));
                        break;
                    case TYPE_DEVICE_LIST:
                        send(g_uartfd, AT_DEVICE_LIST, strlen(AT_DEVICE_LIST));
                        break;   
                    case TYPE_NETWORK_NOCLOSE:
                        send(g_uartfd, AT_NETWORK_NOCLOSE, strlen(AT_NETWORK_NOCLOSE));
                        break;
                    case TYPE_CLOSE_NETWORK:
                        send(g_uartfd, AT_CLOSE_NETWORK, strlen(AT_CLOSE_NETWORK));
                        break;
                    case TYPE_NETWORK_INFO:
                        send(g_uartfd, AT_NETWORK_INFO, strlen(AT_NETWORK_INFO));
                        break;   
                    default:
                        MYLOG_ERROR("Unknow actiontype!");
                }
                continue;
            }

            int nrow = 0, ncolumn = 0;
	        char **dbresult; 
            
            sprintf(sql,"SELECT * FROM devices WHERE deviceid = %d;", deviceid);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            if(nrow == 0) //���ݿ���û�и��豸
            {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device, "result", MQTT_MSG_UNKNOW_DEVICE);
                goto response;
            }

            strncpy(db_zgbaddress, dbresult[ncolumn+1], 20);
            devicetype = dbresult[ncolumn+2][0] - '0';
            deviceindex = dbresult[ncolumn+3][0] - '0';
            operations = cJSON_GetObjectItem(device, "operations");
		    if (operations == NULL)
		    {
			    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device, "result", MQTT_MSG_FORMAT_ERROR);
                goto response;
		    }        
		    operationnum = cJSON_GetArraySize(operations);
            MYLOG("the operationnum = %d", operationnum);
            
		}

		for (i=0; i<RESPONSE_WAIT/50000; i++)
		{
			int j = 0;
			for (; j < g_zgbmsgnum; j++)
			{
				if (g_zgbmsg[j].over == 0)
					break;
			}
			if (j == g_zgbmsgnum)//���е�zgb��Ϣ���Ѵ�����
			{
				break;
			}
			milliseconds_sleep(50);
		}

		for (i=0; i<devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);
			cJSON_AddNumberToObject(device, "result", g_zgbmsg[i].result);
		}
        
response:      

		mqttid = cJSON_GetObjectItem(msg.p_operation_json, "mqttid")->valueint;
		sprintf(topic, "%s%s/result/%d", g_topicroot, g_topicthemes[0], mqttid);
		sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(msg.p_operation_json), QOS_LEVEL_2, 0);
		cJSON_Delete(msg.p_operation_json);
        g_zgbmsgnum = 0;
		g_operationflag = 0;
        g_device = NULL;
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
        if ( write(g_uartfd, (char *)&qmsg.msg, (int)(qmsg.msg.msglength + 2)) != (qmsg.msg.msglength + 2))
	    {
		    MYLOG_ERROR("com write error!");
	    }
	    if ( write(g_uartfd, (char *)&qmsg.msg+sizeof(qmsg.msg)-2, 2) != 2)//����check��footer
	    {
		    MYLOG_ERROR("com write check and footer error!");
	    }
    }
	pthread_exit(NULL);    
}

/*zgb��Ϣ����*/
void* zgbmsgprocess(void* argc)
{
	int rcvret;
    ZGBADDRESS src;
    zgbqueuemsg qmsg;
    char db_zgbaddress[17] = {0};
    char msgtype,devicetype,deviceindex,packetid;
    zgbmsg responsemsg;
    char topic[TOPIC_LENGTH] = { 0 };
    char *zErrMsg = 0;  
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
        msgtype = qmsg.msg.payload.adf.data.msgtype;
        devicetype = qmsg.msg.payload.adf.data.devicetype;
        deviceindex = qmsg.msg.payload.adf.data.deviceindex;
        packetid = qmsg.msg.payload.adf.data.packetid;
        memcpy(src, qmsg.msg.payload.src, 8);
        zgbaddresstodbaddress(src, db_zgbaddress);

        if(qmsg.msg.payload.cmdid[0] == 0x20 && qmsg.msg.payload.cmdid[1] == 0x98) //�豸������Ϣ
        {
            int nrow = 0, ncolumn = 0;
	        char **dbresult; 
            
            sprintf(sql,"SELECT * FROM devices WHERE zgbaddress = %s;", db_zgbaddress);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            if(nrow != 0) //���ݿ����Ѿ��и��豸
            {
                continue;
            }
            
            sendzgbmsg(src, data, 0, ZGB_MSGTYPE_DEVICEREGISTER, 0, 0, getpacketid());//Ҫ���豸ע��
            continue;
        }

        if(msgtype == ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE) //Ҫ���豸ע�����Ӧ��Ϣ
        {
            cJSON* root = cJSON_CreateObject();
            int nrow = 0, ncolumn = 0;
            char **azResult; 

           
            sprintf(sql, "insert into devices values(null, %s, %d, %d, 1);",db_zgbaddress, devicetype, deviceindex);
            MYLOG_INFO(sql);
            rc = sqlite3_exec(g_db, sql, 0, 0, &zErrMsg);
            if(rc != SQLITE_OK)
            {
                MYLOG_ERROR(zErrMsg);
                continue;
            }

            sprintf(sql, "select deviceid from devices where zgbaddress = %s and devicetype = %d and deviceindex = %d",
                db_zgbaddress, devicetype, deviceindex);
            MYLOG_INFO(sql);
            rc = sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
            if(rc != SQLITE_OK)
            {
                MYLOG_ERROR(zErrMsg);
                continue;
            }
            
            data[0] = TLV_TYPE_RESPONSE;
            data[2] = 1;            
            data[1] = TLV_VALUE_RTN_OK;
            sendzgbmsg(src, data, 3, ZGB_MSGTYPE_GATEWAY_RESPONSE, devicetype, deviceindex, packetid);//�豸ע���������Ӧ
            sprintf(topic, "%s%s/", g_topicroot, TOPIC_NEWDEVICE);
            double deviceid = strtoul(azResult[1], NULL, 10);  
            cJSON_AddNumberToObject(root, "deviceid", deviceid);
            cJSON_AddNumberToObject(root, "devicetype", devicetype);
            sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);
            continue;      
        }

        if(msgtype == ZGB_MSGTYPE_DEVICE_OPERATION_RESULT)
        {
            for (int i = 0; i < ZGBMSG_MAX_NUM; ++i)
            {
                if (g_zgbmsg[i].packetid == packetid)
                {
                    g_zgbmsg[i].over = 1;
                    g_zgbmsg[i].result = *((char*)qmsg.msg.payload.adf.data.pdu + 1);
                    break;
                }
            }
            continue;
        }

        if(msgtype == ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE)
        {
            
        }
	}
	pthread_exit(NULL);    
    
}


/*�������ڵĽ��̣���ȡ��Ƭ���ϴ���zgb��Ϣ*/
void* uartlisten(void *argc)
{
	char msgbuf[1024]; //��ʱʹ��1024�ֽڴ洢�������ݣ��������Զ�ȡʱ�������������
	pthread_t threads[2];
	int nByte;
	int bitflag;
	zgbmsg zmsg;
    zgbqueuemsg zgbqmsg;
	int i, j, sum;
    int ret;

    pthread_create(&threads[0], NULL, uartsend, NULL);
    pthread_create(&threads[1], NULL, zgbmsgprocess, NULL);

    MYLOG_DEBUG("Enter pthread uartlisten");
	while (true)
	{
		nByte = read(g_uartfd, msgbuf, 1024);
        MYLOG_INFO("Uart recv %d byte", nByte);
        
		for (i = 0; i < nByte; )
		{
			if (msgbuf[i] != 0x2A)
			{
				i++;
				continue;
			}
			if (msgbuf[i + 1] > nByte - 4)
			{
				MYLOG_ERROR("[uartlisten]Wrong format!");
				break;
			}
			//zgb��Ϣ��ȡ
			zgbmsginit(&zmsg);
			zmsg.msglength = msgbuf[i + 1];
			zmsg.check = msgbuf[i + zmsg.msglength + 2];
			sum = 0;

			//zgb��Ϣ��checkУ��
			for (j = 0; j < zmsg.msglength; j++)
			{
				sum += msgbuf[i + j + 2];
			}
			if (sum%256 != zmsg.check)
			{
				MYLOG_ERROR("[uartlisten]Wrong format!");
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
		        MYLOG_ERROR("send zgbqueuemsg fail!");
	        }
            i = i + zmsg.msglength + 4;
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

/*MQTT��Ϣ���еĴ������*/
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
		MYLOG_ERROR("MyMQTTClient: MQTT connect fail!");
	}
	MYLOG_DEBUG("enter mqttqueueprocess pthread!");

	/*����mqtt��Ϣ����*/
	while (1)
	{
		ret = msgrcv(g_queueid, (void*)&msg, sizeof(mqttmsg), QUEUE_MSG_MQTT, 0);
		if (ret == -1)
		{
			MYLOG_ERROR("read mqttmsg fail!");
		}
loop:
		switch (msg.msgtype)
		{
		case MQTT_MSG_TYPE_PUB:
			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, &opts);
			
			if (result != MQTTASYNC_SUCCESS)
			{
                if (result == MQTTASYNC_DISCONNECTED) //������Ӷϵ����������ط���Ϣ
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
			if (result != MQTTASYNC_SUCCESS)
                if (result == MQTTASYNC_DISCONNECTED) //������Ӷϵ����������ط���Ϣ
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
			if (result != MQTTASYNC_SUCCESS)
                if (result == MQTTASYNC_DISCONNECTED) //������Ӷϵ����������ط���Ϣ
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
		if (msg.msgtype == MQTT_MSG_TYPE_PUB)
		{
			free(msg.msg.msgcontent); //�����PUB��Ϣ��Ҫ������ָ���ͷ�
		}
		free(msg.msg.topic);
	}
	pthread_exit(NULL);
}

/*����������Ҫ�õ�����Ϣ���У�����MQTT�Ķ��ġ�������ȥ���ĺʹ�ȡ�豸������Ϣ�Լ�zgb��Ϣ�Ĵ洢*/
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

/*������Ҫ�����ݿ��*/
int sqlitedb_init()
{
    char *zErrMsg = 0;  
    char sql[512];  
    int rc;  
    int len = 0; 

    rc = sqlite3_open("mydb", &g_db);
    if(rc != SQLITE_OK)  
    {  
        MYLOG_ERROR("open mydb error!");  
        return -1;  
    }
    sprintf(sql,"CREATE TABLE devices (deviceid INTEGER PRIMARY KEY autoincrement, zgbaddress CHAR(20) DEFAULT 0xFFFFFFFFFFFFFFFF, devicetype CHAR(1), deviceindex CHAR(1), online CHAR(1));");
    rc = sqlite3_exec(g_db,sql,0,0,&zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ERROR) //���ظ��᷵��SQLITE_ERROR���ô�����������
    {
        MYLOG_ERROR("zErrMsg = %s rc =%d\n",zErrMsg, rc);  
        return -1;          
    }
    sprintf(sql,"create table airconditioning(deviceid INTEGER PRIMARY KEY, status INTEGER,mode INTEGER,temperature float, windspeed INTEGER);");
    sqlite3_exec(g_db,sql,0,0,&zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ERROR)
    {
        MYLOG_ERROR("zErrMsg = %s\n",zErrMsg);
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
        MYLOG_ERROR("init_uart fail!");
    }
    sem_init(&g_mqttconnetionsem, 0, 1); 

    if(createmessagequeue() == -1)
    {
        MYLOG_ERROR("CreateMessageQueue failed!");
        return -1;
    }
    if(sqlitedb_init() == -1)
    {
        MYLOG_ERROR("Create db failed!");
        return -1;        
    }
    
	constructsubtopics();
    
	pthread_create(&threads[0], NULL, mqttlient, NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten, NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);

    pthread_exit(NULL);
}


