#ifndef _MQTT_H_
#define _MQTT_H_

/*mqtt���ġ�������ȥ���Ľӿ�*/
void sendmqttmsg(long messagetype, char* topic, char* message, int qos, int retained);

/*MQTT�ͻ��˽���*/
void *mqttlient(void *argc);

/*������MQTT�ͻ��˽���*/
void *lanmqttlient(void *argc);

/*MQTT��Ϣ���еĴ������*/
void* mqttqueueprocess(void *argc);

/*������MQTT��Ϣ���еĴ������*/
void* lanmqttqueueprocess(void *argc);

#endif
