#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include "cJSON.h"
#include <unistd.h>
#include <sys/queue.h>

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

volatile MQTTClient_deliveryToken deliveredtoken;
int usedflag = 1;

char *RESULTPUBMSG[] =
{
	"The other user is operating,please wait a moment.",
	"Operation failed.",
	"Operation success.",
};

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
	const char* notificationid = "notificationid";
	
    printf("Message arrived\n");

    payloadptr = message->payload;
	
    jsons = cJSON_Parse(payloadptr);
	if(!jsons)
	{
		userjson = cJSON_GetObjectItem(jsons, user);
		useridjson = cJSON_GetObjectItem(jsons, userid);
        if(usedflag)
        {
        	pubresult(OPBUSY, message);
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
void pubresult(pubmsgTypes type, MQTTClient_message *message)
{
    int count = 0;
	char topicname[MAX_TOPIC_LENGTH] = {0};

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
    pubmsg.payload = RESULTPUBMSG[type];
    pubmsg.payloadlen = strlen(RESULTPUBMSG[type]);
    pubmsg.qos = message->qos;
    pubmsg.retained = 0;
	sprintf(topicname, "%s", "/" + getgatewayid() + "/" + itoa(message->msgid));

    MQTTClient_publishMessage(client, topicname, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);   
    
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

char *getgatewayid()
{
	return "123456789";
}
int main(int argc, char* argv[])
{
    pthread_t threads[NUM_THREADS];
    long t;
    pthread_create(&threads[0], NULL, subClient, (void *)0);
	
    pthread_exit(NULL);
}