#ifndef _MQTT_H_
#define _MQTT_H_

/*mqtt订阅、发布、去订阅接口*/
void sendmqttmsg(long messagetype, char* topic, char* message, int qos, int retained);

/*MQTT客户端进程*/
void *mqttclient(void *argc);

/*局域网MQTT客户端进程*/
void *lanmqttlient(void *argc);

/*MQTT消息队列的处理进程*/
void* mqttqueueprocess(void *argc);

/*局域网MQTT消息队列的处理进程*/
void* lanmqttqueueprocess(void *argc);

#endif
