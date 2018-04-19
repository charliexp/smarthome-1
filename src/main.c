#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTAsync.h"
#include "tools.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>

#define NUM_THREADS 1
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee" //客户端ID
#define TIMEOUT     10000L
#define QOS 1
#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM 5

char g_topicroot[20] = "/00:00:00:00:00:00/";
char g_mac[20] = {0};


int g_operationflag = 0;
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

void onDisconnect(void* context, MQTTAsync_successData* response)
{
	printf("Successful disconnection\n");
}



int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("topic: %s\n", topicName);

    payloadptr = message->payload;

	if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag != 1)
		{
			printf("busy,wait a mement.\n");
		}
		printf("get an operation msg.\n");
		g_operationflag = 1;
		sleep(10);
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

void *MQTTClient(void *argc)
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

/*创建四个消息队列，分布用于订阅、发布、去订阅以及设备操作消息*/
int CreateMessageQueue()
{
   	int submsgid, pubmsgid, unsubmsgid, devicemsgid;     

	key_t subkey, pubkey, unsubkey, devicemsgkey;
	subkey = ftok("/etc", 's');
    pubkey = ftok("/etc", 'p');
    unsubkey = ftok("/etc", 'u');
    devicemsgkey = ftok("/etc", 'd');
    
	submsgid = msgget(subkey, IPC_CREAT | 0666);
    pubmsgid = msgget(pubkey, IPC_CREAT | 0666);
    unsubmsgid = msgget(unsubkey, IPC_CREAT | 0666);
    devicemsgid = msgget(devicemsgkey, IPC_CREAT | 0666);
	if (submsgid < 0 || pubmsgid < 0 || unsubmsgid < 0 || devicemsgid < 0)
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

    if(CreateMessageQueue() != 0)
    {
        printf("CreateMessageQueue failed!\n");
        return -1;
    }
	constructSubTopics();
	
    pthread_create(&threads[0], NULL, MQTTClient, NULL);
    pthread_exit(NULL);

	return 0;
}


