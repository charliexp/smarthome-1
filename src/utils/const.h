#ifndef _CONST_H_
#define _CONST_H_

/*COO AT命令*/
#define AT_CREATE_NETWORK       "AT+FORM=02"
#define AT_OPEN_NETWORK         "AT+PERMITJOIN=30"
#define AT_DEVICE_LIST          "AT+LIST"
#define AT_NETWORK_NOCLOSE      "AT+PERMITJOIN=FF"
#define AT_CLOSE_NETWORK        "AT+PERMITJOIN=00"
#define AT_NETWORK_INFO         "AT+GETINFO"

#define TYPE_CREATE_NETWORK       0x01
#define TYPE_OPEN_NETWORK         0x02
#define TYPE_DEVICE_LIST          0x03
#define TYPE_NETWORK_NOCLOSE      0x04
#define TYPE_CLOSE_NETWORK        0x05
#define TYPE_NETWORK_INFO         0x06



#define LOG_FILE "smarthomelog.log"
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_ERROR 2

#define THREAD_NUMS 5
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define CLIENTID    "todlee"                   //客户端ID
#define CLIENTID1   "todlee_pub"              //客户端ID

#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM          2
#define RESPONSE_WAIT      3000000 //消息响应等待时间5000000us = 5s
#define ZGB_ADDRESS_LENGTH 8
#define ZGBMSG_MAX_NUM     20

#define TOPIC_DEVICE_ADD "devices/add/"

#define QOS_LEVEL_0 0
#define QOS_LEVEL_1 1
#define QOS_LEVEL_2 2


#define QUEUE_MSG_MQTT  0x01
#define QUEUE_MSG_ZGB   0x02
#define QUEUE_MSG_DEVIC 0x03
#define QUEUE_MSG_UART  0x04


#define TOPIC_LENGTH        100
#define MQTT_MSG_TYPE_PUB   1
#define MQTT_MSG_TYPE_SUB   2
#define MQTT_MSG_TYPE_UNSUB 3

#define ZGB_MAGIC_NUM  0xAA
#define ZGB_VERSION_10 0x10

/*设备类型*/
#define DEV_BOOT			0x00	// 空设备，特指Boot Loader
#define DEV_SOCKET		    0x01	// 插座
#define DEV_AIR_CON		    0x02	// 空调
#define DEV_BOILER			0x03	// 锅炉
#define DEV_FAN_COIL		0x04	// 风机盘管
#define DEV_FLOOR_HEAT	    0x05	// 地暖
#define DEV_FRESH			0x06	// 新风
#define DEV_WATER_PURIF	    0x07	// 净水器
#define DEV_WATER_HEAT	    0x08	// 热水器
#define DEV_WATER_PUMP	    0x09	// 循环泵
#define DEV_DUST_CLEAN	    0x0A    // 除尘器
#define DEV_MULTI_PANEL     0x0B    // 多合一控制面板
#define DEV_ANYONE          0xFF    // 任何设备


/*消息类型     对应msgtype*/
#define ZGB_MSGTYPE_DEVICEREGISTER          0x00 //要求设备注册
#define ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE 0x01 //设备注册响应
#define ZGB_MSGTYPE_DEVICE_OFFNET           0x02 //要求设备离网
#define ZGB_MSGTYPE_DEVICE_OPERATION        0x03 //设备操作
#define ZGB_MSGTYPE_DEVICE_OPERATION_RESULT 0x04 //设备操作结果
#define ZGB_MSGTYPE_DEVICE_STATUS_QUERY     0x05 //设备状态查询
#define ZGB_MSGTYPE_DEVICE_STATUS_REPORT    0x06 //设备状态上报
#define ZGB_MSGTYPE_UPDATE                  0x10 //软件升级提示
#define ZGB_MSGTYPE_UPDATE_RESPONSE         0x11 //软件升级响应
#define ZGB_MSGTYPE_UPDATE_PAYLOAD          0x12 //升级报文
#define ZGB_MSGTYPE_UPDATE_RESULT           0x13 //升级结果


/*升级状态 msgtype对应ZGB_MSGTYPE_UPDATE_RESPONSE*/
#define OTA_BEGIN          0x00 // 升级准备
#define OTA_READY		   0x01	// 准备好接收
#define OTA_ONE_OK         0x02 // 单个报文接收OK
#define OTA_ONE_FAIL       0x03 // 单个报文接收失败，要求重传
#define OTA_REUPDATE       0x04 // 重新升级
#define OTA_END		       0x05 // 更新结束
#define OTA_SUCCESS		   0x06 // 升级成功

/*系统消息 0x00~0x0f*/
/*响应报文*/
#define ATTR_RESPONSE     0x00 

#define TLV_VALUE_RTN_OK			0x00	// 操作成功
#define TLV_VALUE_RTN_FAIL		    0x01	// 操作失败
#define TLV_VALUE_RTN_ILLEGAL       0x02	// 非法请求
#define TLV_VALUE_RTN_PARAM		    0x03	// 参数错误
#define TLV_VALUE_RTN_BUSY		    0x04	// 系统忙

/*故障上报*/
#define ATTR_FAULT 0x01 

#define TLV_VALUE_FAULT_SOCKET    0x01 //插座保险丝损坏
#define TLV_VALUE_FAULT_PURIFIER  0x02 //净水器滤芯更换

#define ATTR_DEVICENAME     0x02

#define ATTR_WORKING_STATUS 0x03

#define TLV_VALUE_POWER_OFF  0x00 //断电
#define TLV_VALUE_POWER_ON   0x01 //上电
#define TLV_VALUE_STANDBY    0x02 //待机

/*插座数据*/
#define ATTR_SOCKET_E   0x10 //插座电量
#define ATTR_SOCKET_P   0x11 //插座功率
#define ATTR_SOCKET_V   0x12 //插座电压
#define ATTR_SOCKET_I   0x13 //插座电流
#define ATTR_SOCKET_BE  0x14 //插座总电量
#define ATTR_SOCKET_WORKTIME 0x15 //插座通电时间

/*设备属性*/
#define ATTR_WINDSPEED         0x04               //设备风速 中低高三档
#define ATTR_WINDSPEED_NUM     0x05               //设备风速 0~100
#define ATTR_TEMPERATURE       0x06               //设备温度 0~100


/*
**http常量
**
*/
#define HTTP_UPLOAD_UAR "http://123.206.15.63:8060/manager/upload"
#endif
