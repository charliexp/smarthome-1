#ifndef _ERROR_H_
#define _ERROR_H_

/*mqtt´íÎóÂë*/
#define MQTT_MSG_SUCCESS        u8"Operation Success."
#define MQTT_MSG_FORMAT_ERROR   u8"The MQTT message format error,please check it."
#define MQTT_MSG_SYSTEM_BUSY    u8"The system is busy please try again later."
#define MQTT_MSG_UNKNOW_DEVICE  u8"The device do not exist."

#define MQTT_MSG_ERRORCODE_SUCCESS 0
#define MQTT_MSG_ERRORCODE_FORMATERROR 1
#define MQTT_MSG_ERRORCODE_BUSY    2
#define MQTT_MSG_ERRORCODE_DEVICENOEXIST 3


#endif
