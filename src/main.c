#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTAsync.h"
#include "tools.h"
#include <unistd.h>
#include <semaphore.h>

#define NUM_THREADS 1
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee" //发布客户端ID
#define TIMEOUT     10000L
#define QOS 1
#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM 5

char g_topicroot[20] = "/00:00:00:00:00:00/";
char g_mac[20] = {0};
const char **g_topicthemes =
	{
		"operation",
		"update",
		"device",
		"configuration",
		"warning",
	};


int g_operationflag = 0;
sem_t g_devicestatussem;
int *g_qoss = {2, 1, 2, 1};
char* g_topics[TOPICSNUM] ={0};

void ConstructTopics()
{
	int i = 0;

	for(; i < TOPICSNUM; i++)
	{
		g_topics[i] = malloc(sizeof(g_topicroot) + sizeof(g_topicthemes[i]) + strlen("/+"));
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
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;

	if (strstr(topicName, "operation") != 0)
	{
		if (g_operationflag != 1)
		{
			printf("busy,wait a mement.\n");//发送操作忙消息，提示用户等待当前操作完成。
		}
		printf("get an operation msg.\n");
		g_operationflag = 1;
		sleep(10);
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
}


void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

	printf("Successful connection\n");

	rc = MQTTAsync_subscribeMany(client, TOPICSNUM, g_topics, g_qoss, &opts)
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
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	do
	{
	    rc = MQTTAsync_connect(client, &conn_opts);
	}
	while (!rc)
	
}

void *MyMQTTClient(void *argc)
{  
	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	MQTTAsync_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
	
    MQTTAsync_destroy(&client);   
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    pthread_t threads[NUM_THREADS];

	sem_init(&g_devicestatussem, 0, 0);	
	
	//获取每个设备Topic的根节点
	if(getmac(g_mac) == 0)
	{
        sprintf(g_topicroot, "/%s/", g_mac);    
	}
    pthread_create(&threads[0], NULL, MyMQTTClient, NULL);
    pthread_exit(NULL);

	return 0;
}


