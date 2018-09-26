#include "../app.h"
#include "../mqtt.h"

extern char g_topicroot[20];

//测试函数
void* testfun(void *argv)
{
    cJSON* root = cJSON_CreateObject();  
    char topic[TOPIC_LENGTH] = { 0 };
    sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_ADD);   
    cJSON_AddStringToObject(root, "deviceid", "00FA2DC1DF1");
    cJSON_AddNumberToObject(root, "devicetype", 1);
    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
    MYLOG_DEBUG("The mqtt msg is %s", cJSON_PrintUnformatted(root));
    sleep(1);
    cJSON* addr = cJSON_CreateString("00FA2DC1DF2");
    cJSON* value = cJSON_CreateNumber(1);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息  
    MYLOG_DEBUG("The mqtt msg is %s", cJSON_PrintUnformatted(root));
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF3");
    value = cJSON_CreateNumber(1);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息 
    MYLOG_DEBUG("The mqtt msg is %s", cJSON_PrintUnformatted(root));
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF4");
    value = cJSON_CreateNumber(1);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息  
    MYLOG_DEBUG("The mqtt msg is %s", cJSON_PrintUnformatted(root));
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF5");
    value = cJSON_CreateNumber(2);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息 
    MYLOG_DEBUG("The mqtt msg is %s", cJSON_PrintUnformatted(root));
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF6");
    value = cJSON_CreateNumber(3);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息   
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF7");
    value = cJSON_CreateNumber(4);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息   
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF8");
    value = cJSON_CreateNumber(5);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息   
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DF9");
    value = cJSON_CreateNumber(6);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息 
    sleep(1);
    addr = cJSON_CreateString("00FA2DC1DFA");
    value = cJSON_CreateNumber(6);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息     
    sleep(1);
    addr = cJSON_CreateString("20FA2DC1DFA0");
    value = cJSON_CreateNumber(12);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
    sleep(1);
    addr = cJSON_CreateString("30FA2DC1DFA0");
    value = cJSON_CreateNumber(12);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
    sleep(1);
    addr = cJSON_CreateString("10FA2DC1DFA0");
    value = cJSON_CreateNumber(12);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
    sleep(1);
    addr = cJSON_CreateString("A0FA2DC1DFA0");
    value = cJSON_CreateNumber(11);
    cJSON_ReplaceItemInObject(root, "deviceid", addr);
    cJSON_ReplaceItemInObject(root, "devicetype", value);
    sendmqttmsg(MQTT_MSG_TYPE_PUB,topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息    
}

