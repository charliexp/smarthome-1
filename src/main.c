#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <semaphore.h>

#include "paho/MQTTAsync.h"
#include "tools/tools.h"
#include "device/device.h"
#include "cjson/cJSON.h"
#include "tools/error.h"

#define NUM_THREADS 4
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee" //客户端ID
#define CLIENTID1    "todlee_pub" //客户端ID
#define TIMEOUT     10000L
#define QOS 1
#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM   5
#define RESPONSE_WAIT 5000000 //消息响应等待时间5000000us = 5s

char g_topicroot[20] = {0};
char g_mac[20] = {0};
ZGB_MSG_STATUS g_zgbmsg[20];
int g_zgbmsgnum;
cJSON* g_device;

int g_operationflag = 0;
sem_t g_operationsem;
sem_t g_devicestatussem;
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
		zgbaddress address;
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
	key_t key;
	int id;
	int ret;
	size_t msglen;
	mqttmsg msg = { 0 };
	msg.msgtype = 1;
	msg.msg.qos = qos;
	msg.msg.retained = 0;
	if (topic != NULL)
	{
		msg.msg.topic = malloc(strlen(topic)+1);
		strcpy(msg.msg.topic, topic);
		msg.msg.topic[strlen(topic)] = 0;
	}

	if (message != NULL && messagetype == 1)
	{
		msg.msg.msgcontent = malloc(strlen(message) + 1);
		strcpy(msg.msg.msgcontent, message);
		msg.msg.msgcontent[strlen(message) + 1] = 0;
	}

	msglen = sizeof(mqttmsg);
	key = ftok("/etc/hosts", 'm');
	id = msgget(key, IPC_CREAT | 0666);
	if (id == -1)
	{
		perror("msgget fail!\n");
	}
	if (ret = msgsnd(id, &msg, msglen, 0) != 0)
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
	deviceoperationmsg msg;
	key_t key;
	int id;
	int sendret;
	size_t msglen = sizeof(deviceoperationmsg);
	char topic[TOPIC_LENGTH] = { 0 };

    printf("Message arrived\n");
    printf("topic: %s\n", topicName);

    payloadptr = (char*)message->payload;
	root = cJSON_Parse(payloadptr);

	if (root == NULL)
	{
		perror("Wrong msg!\n");
		return -1;
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

			key = ftok("/etc/hosts", 'd');
			id = msgget(key, IPC_CREAT | 0666);
			if (id == -1)
			{
				perror("msgget fail!\n");
			}
			msg.msgtype = 1;
			msg.p_operation_json = cJSON_Duplicate(root, 1);

			if (sendret = msgsnd(id, (void*)&msg, msglen, 0) != 0)
			{
				printf("sendret = %d", sendret);
				perror("");
				printf("send devicequeuemsg fail!\n");
				return -1;
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
		printf("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Connect failed, rc %d\n", response ? response->code : 0);
    sem_post(&g_mqttconnetionsem);
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	printf("\nConnection lost\n");
	printf("cause: %s\n", cause);
	printf("Reconnecting\n");

	sem_post(&g_mqttconnetionsem);
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	pthread_t pthread;
	int rc;
	printf("Successful connection\n");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, g_qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		printf("sub error!\n");
	}

	return;
}

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
            printf("MyMQTTClient: MQTT connect fail!\n");
        }
	}

    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

void* devicemsgprocess(void *argc)
{
	deviceoperationmsg msg;
	char topic[TOPIC_LENGTH] = { 0 };
	char mqttid[16] = {0};
	cJSON *devices, *device;
	int devicenum = 0;
	int i = 0;
	key_t key;
	int id;
	int rcvret;
	char packetid;
	char devicetype;
	size_t msglen = sizeof(deviceoperationmsg);

	while(1)
	{
		key = ftok("/etc/hosts", 'd');
		id = msgget(key, IPC_CREAT | 0666);
		if (id == -1)
		{
			perror("msgget fail!\n");
		}

		if (rcvret = msgrcv(id, (void*)&msg, msglen, 0, 0) <= 0)
		{
			printf("rcvret = %d", rcvret);
			perror("");
			printf("recive devicequeuemsg fail!\n");
			pthread_exit(NULL);
		}
		printf("devicemsgprocess recive a msg\n");
		printf("the msg is %s\n", cJSON_PrintUnformatted(msg.p_operation_json));
		g_device = msg.p_operation_json;
		devices = cJSON_GetObjectItem(g_device, "device");
		devicenum = cJSON_GetArraySize(devices);

		for (; i< devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);
			if (device == NULL)
			{
				printf("error");
			}
			packetid = getpacketid();
			g_zgbmsg[i].packetid = packetid;
			g_zgbmsg[i].over = 0;
			g_zgbmsgnum++;

			devicetype = cJSON_GetObjectItem(device, "type")->valueint;
			if (device == NULL)
			{
				printf("error");
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
			usleep(50000);
		}

		for (i=0; i<devicenum; i++)
		{
			device = cJSON_GetArrayItem(devices, i);
			if (device == NULL)
			{
				printf("error");
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

void* uartlisten(void *argc)
{

	pthread_exit(NULL);
}


void* mqttqueueprocess(void *argc)
{
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	mqttmsg msg;
	ssize_t ret;
	key_t key;
	int id;
	int result;

	MQTTAsync_create(&client, ADDRESS, CLIENTID1, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.username = "root";
	conn_opts.password = "root";
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;
	rc = MQTTAsync_connect(client, &conn_opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		printf("MyMQTTClient: MQTT connect fail!\n");
	}
	printf("enter mqttqueueprocess pthread!\n");
	key = ftok("/etc/hosts", 'm');
	id = msgget(key, IPC_CREAT | 0666);
	if (id == -1)
	{
		perror("msgget fail!\n");
	}

	/*处理mqtt消息队列*/
	while (1)
	{
		ret = msgrcv(id, (void*)&msg, sizeof(mqttmsg), 0, 0);
		if (ret == -1)
		{
			perror("read mqttmsg fail!\n");
		}
		printf("receive a msg:type:%d,topic:%s\n", msg.msgtype, msg.msg.topic);
		switch (msg.msgtype)
		{
		case MQTT_MSG_TYPE_PUB:
			result = MQTTAsync_send(client, (const char*)msg.msg.topic, strlen(msg.msg.msgcontent),
				(void *)msg.msg.msgcontent, msg.msg.qos, msg.msg.retained, &opts);
			if (result != MQTTASYNC_SUCCESS)
			{
				printf("MQTTAsync_send fail! %d\n", result);
			}
			break;
		case MQTT_MSG_TYPE_SUB:
			result = MQTTAsync_subscribe(client, (const char*)msg.msg.topic, msg.msg.qos, &opts);
			if (result != MQTTASYNC_SUCCESS)
				printf("MQTTAsync_subscribe fail!\n");
			break;
		case MQTT_MSG_TYPE_UNSUB:
			result = MQTTAsync_unsubscribe(client, (const char*)msg.msg.topic, &opts);
			if (result != MQTTASYNC_SUCCESS)
				printf("MQTTAsync_unsubscribe fail!\n");
			break;
		default:
			printf("unknow msg!\n");
		}
		if (msg.msgtype == MQTT_MSG_TYPE_PUB)
		{
			free(msg.msg.msgcontent);
		}
		free(msg.msg.topic);
	}
	pthread_exit(NULL);
}
/*创建两个消息队列，分别用MQTT的订阅、发布、去订阅和存取设备操作消息*/
int createmessagequeue()
{
   	int mqttmsgid, devicemsgid;     
	key_t mqttkey, devicemsgkey;
	mqttkey = ftok("/etc/hosts", 'm');
    devicemsgkey = ftok("/etc/hosts", 'd');
    
	mqttmsgid = msgget(mqttkey, IPC_CREAT | 0666);
    devicemsgid = msgget(devicemsgkey, IPC_CREAT | 0666);
	if (mqttmsgid < 0 || devicemsgid < 0)
	{
		printf("get ipc_id error!\n");
		return -1;
	}
    return 0;  
}

int main(int argc, char* argv[])
{
    pthread_t threads[NUM_THREADS];

	system("stty -F /dev/ttyS1 speed 57600 cs8 -parenb -cstopb  -echo");
	sem_init(&g_devicestatussem, 0, 0);	
    sem_init(&g_mqttconnetionsem, 0, 1); 
	sem_init(&g_operationsem, 0, 0);

    if(createmessagequeue() != 0)
    {
        printf("CreateMessageQueue failed!\n");
        return -1;
    }
	constructsubtopics();
	
	pthread_create(&threads[0], NULL, mqttlient, NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten, NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);

    pthread_exit(NULL);

	return 0;
}


