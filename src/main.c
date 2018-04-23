#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTAsync.h"
#include "tools/tools.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include "device/device.h"
#include "cjson/cJSON.h"

#define NUM_THREADS 2
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee" //客户端ID
#define TIMEOUT     10000L
#define QOS 1
#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM 5

typedef struct 
{
	long msgtype;
	char data[500];
}testmsg;


char g_topicroot[20] = "/00:00:00:00:00:00/";
char g_mac[20] = {0};

int g_operationflag = 0;
sem_t g_operationsem;
sem_t g_devicestatussem;
sem_t g_mqttconnetionsem;
//订阅g_topics对应的qos
int g_qoss[TOPICSNUM] = {2, 1, 2, 1, 1};
//程序启动后申请堆存放需要订阅的topic
char* g_topics[TOPICSNUM] ={0x0, 0x0, 0x0, 0x0, 0x0};
char g_topicthemes[TOPICSNUM][10] = {{"operation"}, {"update"}, {"device"}, {"config"}, {"warning"}};


void constructSubTopics()
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

void pubmsg(char* topic, char* message, int qos)
{
	key_t key;
	int id;
	size_t msglen;
	mqttmsg msg = { 0 };
	msg.msgtype = 0;
	msg.msg.qos = qos;
	strcpy(msg.msg.topic, topic);
	strcpy(msg.msg.msg, message);
	msglen = sizeof(mqttmsg);

	printf("begin to open mqttqueue\n");
	key = ftok("/etc", 'm');
	id = msgget(key, IPC_CREAT | 0666);
	if (id == -1)
	{
		perror("msgget fail!\n");
	}
	if (msgsnd(id, &msg, msglen, 0) != 0)
	{
		printf("send mqttqueuemsg fail!\n");
		return;
	}

	return;
}

void processoperationmsg(cJSON *root)
{
	printf("root = %p", root);
	cJSON *device;
	cJSON *devicechild;
	deviceoperationmsg msg;
	int devicenum = 0;
	int i = 0, j =0;
	key_t key;
	int id;
	int sendret;
	size_t msglen = sizeof(deviceoperationmsg);
	
	for (;j<msglen;j++)
	{
		((char*)&msg)[j] = 0;
	}

	key = ftok("/etc", 'd');
	id = msgget(key, IPC_CREAT | 0666);
	if (id == -1)
	{
		perror("msgget fail!\n");
	}

	device = cJSON_GetObjectItem(root, "device");
	devicenum = cJSON_GetArraySize(device);
	for (; i < devicenum; i++)
	{
		devicechild = cJSON_GetArrayItem(device, i);
		msg.msgtype = 1;
		strcpy(msg.operation.address, cJSON_GetArrayItem(devicechild, 2)->valuestring);
		strcpy(msg.operation.devicename, cJSON_GetArrayItem(devicechild, 0)->valuestring);
		msg.operation.devicetype = cJSON_GetArrayItem(devicechild, 1)->valueint;

		if (sendret = msgsnd(id, (void*)&msg, msglen, 0) != 0)
		{
			printf("sendret = %d", sendret);
			perror("");
			printf("send devicequeuemsg fail!\n");
			return;
		}
	}
	sem_post(&g_operationsem);
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	printf("Successful disconnection\n");
}


int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
	char* payloadptr;
	cJSON *jsonroot;
	char mqttid[16] = {0};
	char topic[TOPIC_LENGTH] = { 0 };

    printf("Message arrived\n");
    printf("topic: %s\n", topicName);

    payloadptr = (char*)message->payload;
	printf("msg is %s\n", payloadptr);

	if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag)
		{
			/*如果有消息在处理，则返回忙碌错误*/
			strncpy(mqttid, payloadptr, 16);
			sprintf(topic, "%s%s/result/%s", g_topicroot, g_topicthemes[0], mqttid);
			char *msg = "The other user is using,please wait!";
			pubmsg(topic, msg, 1);
		}
		else
		{
			/*将操作消息丢入消息队列*/
			g_operationflag = 1;
			printf("get an operation msg.\n");
			jsonroot = cJSON_Parse(payloadptr);
			processoperationmsg(jsonroot);
		}
		sem_wait(&g_operationsem);
		g_operationflag = 0;

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


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	printf("Successful connection\n");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, g_qoss, &opts);
	if (rc != MQTTASYNC_SUCCESS)
	{
		printf("sub error!\n");
	}
}

void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	printf("\nConnection lost\n");
	printf("     cause: %s\n", cause);
	printf("Reconnecting\n");

    sem_post(&g_mqttconnetionsem);
}

void *mqttlient(void *argc)
{  
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
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

void devicemsgprocess(void *argc)
{

}

/*创建两个消息队列，分别用MQTT的订阅、发布、去订阅和存取设备操作消息*/
int CreateMessageQueue()
{
   	int mqttmsgid, devicemsgid;     

	key_t mqttkey, devicemsgkey;
	mqttkey = ftok("/etc", 'm');
    devicemsgkey = ftok("/etc", 'd');
    
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

	sem_init(&g_devicestatussem, 0, 0);	
    sem_init(&g_mqttconnetionsem, 0, 1); 
	sem_init(&g_operationsem, 0, 0);

    if(CreateMessageQueue() != 0)
    {
        printf("CreateMessageQueue failed!\n");
        return -1;
    }
	constructSubTopics();
	
    pthread_create(&threads[0], NULL, mqttlient, NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
    pthread_exit(NULL);

	return 0;
}


