#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include "cJSON.h"
#include <unistd.h>
#include <sys/queue.h>
#include <uuid.h>

#define NUM_THREADS 2
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee_pub" //订阅客户端ID
#define SUB_CLIENTID    "todlee_sub" //发布客户端ID
#define TOPIC       "topic01"  //话题
#define PAYLOAD     "Hello Man, Can you see me ?!" //消息内容
#define QOS         1
#define TIMEOUT     10000L
#define USERNAME    "test_user"
#define PASSWORD    "jim777"
#define DISCONNECT  "out"

int CONNECT = 1;
volatile MQTTClient_deliveryToken deliveredtoken;
bool usedflag;

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
	char ch;
    char* payloadptr;
	cJSON *jsons, *userjson, *useridjson, *typejson;
	const char* user = "name";
	const char* userid = "userid";
	const char* type = "type";
	const char* notificationid = "notificationid"
	
    printf("Message arrived\n");

    payloadptr = message->payload;
	
    jsons = cJSON_Parse(payloadptr);
	if(!jsons)
	{
		userjson = cJSON_GetObjectItem(jsons, user);
		useridjson = cJSON_GetObjectItem(jsons, userid);
        if(usedflag)
        {
        	pubClient()
        }

	}
	cJSON_Delete(jsons);
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

void *subClient(void *threadid){
   long tid;
   tid = (long)threadid;
   
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    int ch;

    MQTTClient_create(&client, ADDRESS, SUB_CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    //conn_opts.username = USERNAME;
    //conn_opts.password = PASSWORD;
    
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
    MQTTClient_subscribe(client, TOPIC, QOS);

    do 
    {
        ch = getchar();
    } while(ch!='Q' && ch != 'q');

    MQTTClient_unsubscribe(client, TOPIC);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
   
   pthread_exit(NULL);
}
void *pubClient(pubmsgTypes type, char* payload, MQTTClient_message *message)
{
    long tid;
    tid = (long)threadid;
    int count = 0;

    //声明一个MQTTClient
    MQTTClient client;
    //初始化MQTT Client选项
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    //#define MQTTClient_message_initializer { {'M', 'Q', 'T', 'M'}, 0, 0, NULL, 0, 0, 0, 0 }
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    //声明消息token
    MQTTClient_deliveryToken token;
    int rc;
    //使用参数创建一个client，并将其赋值给之前声明的client
    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    //conn_opts.username = USERNAME;
    //conn_opts.password = PASSWORD;
     //使用MQTTClient_connect将client连接到服务器，使用指定的连接选项。成功则返回MQTTCLIENT_SUCCESS
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    while(CONNECT){
    MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/1000), PAYLOAD, TOPIC, CLIENTID);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);
    usleep(3000000L);
    }
    
    
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}
int main(int argc, char* argv[])
{
    pthread_t threads[NUM_THREADS];
    long t;
    pthread_create(&threads[0], NULL, subClient, (void *)0);
	
    pthread_exit(NULL);
}