#ifndef _CONST_H_
#define _CONST_H_

/*COO AT命令*/
#define AT_CREATE_NETWORK       "AT+FORM=02"
#define AT_OPEN_NETWORK         "AT+PERMITJOIN=78"
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

#define LOCKFILE "/var/run/smarthome.pid"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) 

/*=====================LOG 常量=========================*/

#define LOG_FILE "smarthomelog.log"
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_ERROR 2


/*=====================MQTT 常量=========================*/
#define THREAD_NUMS 8
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define LAN_MQTT_SERVER "tcp://127.0.0.1:1883"
#define CLIENTID    "todlee"                   //客户端ID
#define CLIENTID1   "todlee_pub"              //客户端ID

#define WAN_CLIENT_ID 1
#define WAN_CLIENT_PUB_ID 2
#define LAN_CLIENT_ID 3
#define LAN_CLIENT_PUB_ID 4

#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM          3
#define RESPONSE_WAIT      3000 //消息响应等待时间(毫秒)
#define ZGB_ADDRESS_LENGTH 8
#define ZGBMSG_MAX_NUM     20

#define TOPIC_DEVICE_ADD    "devices/add"
#define TOPIC_DEVICE_STATUS "devices/status"
#define TOPIC_DEVICE_SHOW   "devices/show"

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
#define DEV_GATEWAY			   0x00	// 网关
#define DEV_SOCKET		       0x01	// 插座
#define DEV_AIR_CON		       0x02	// 空调
#define DEV_BOILER			   0x03	// 锅炉
#define DEV_FAN_COIL		   0x04	// 风机盘管
#define DEV_FLOOR_HEAT	       0x05	// 地暖
#define DEV_FRESH			   0x06	// 新风
#define DEV_WATER_PURIF	       0x07	// 净水器
#define DEV_WATER_HEAT	       0x08	// 热水器
#define DEV_WATER_PUMP	       0x09	// 循环泵
#define DEV_DUST_CLEAN	       0x0A // 除尘器
#define DEV_CONTROL_PANEL      0x0B // 控制面板
#define DEV_HUMIDIFIER         0x0C // 加湿器
#define DEV_DEHUMIDIFIER       0x0D // 除湿器
#define SEN_WIND_PRESSURE      0x50 // 风压传感器
#define SEN_WATER_FLOW         0x51 // 水流量计
#define SEN_WATER_TEMPERATURE  0x52 // 水温传感器
#define SEN_ENV_DATA           0x53 // 环境数据传感器
#define SEN_ELECTRICITY_METER  0x54 // 电量计 
#define SEN_GAS_METER          0x55 // 气量计
#define SEN_ANEMOGRAPH         0x56 // 风速计
#define SEN_WATER_MANOMETER    0x57 // 水压计
#define DEV_ANYONE             0xFF // 任何设备


#define DEV_ONLINE 1
#define DEV_OFFLINE 0
/*消息类型     对应msgtype*/
#define ZGB_MSGTYPE_DEVICEREGISTER          0x00 //要求设备注册
#define ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE 0x01 //设备注册响应
#define ZGB_MSGTYPE_DEVICE_OFFNET           0x02 //要求设备离网
#define ZGB_MSGTYPE_DEVICE_OPERATION        0x03 //设备操作
#define ZGB_MSGTYPE_DEVICE_OPERATION_RESULT 0x04 //设备操作结果
#define ZGB_MSGTYPE_DEVICE_STATUS_QUERY     0x05 //设备状态查询
#define ZGB_MSGTYPE_DEVICE_STATUS_REPORT    0x06 //设备状态上报
#define ZGB_MSGTYPE_DEVICE_LOCATION         0x07 //设备定位  
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
#define ATTR_FAULT 0xFF 

#define TLV_VALUE_FAULT_SOCKET    0x01 //插座保险丝损坏
#define TLV_VALUE_FAULT_PURIFIER  0x02 //净水器滤芯更换

#define TLV_VALUE_POWER_OFF  0x00 //关机
#define TLV_VALUE_POWER_ON   0x01 //上电
#define TLV_VALUE_STANDBY    0x02 //待机

#define TLV_VALUE_HEAT  0 //制热
#define TLV_VALUE_COLD  1 //制冷
#define TLV_VALUE_HUMIDIFICATION 0 //加湿
#define TLV_VALUE_DEHUMIDIFICATION 1 //除湿

#define TLV_VALUE_COND_HEAT 0 //空调制热
#define TLV_VALUE_COND_COLD 1 //空调制冷
#define TLV_VALUE_BOILER_HEAT 2 //锅炉制热

#define TLV_VALUE_ONLINE 1
#define TLV_VALUE_OFFLINE 0

/*插座数据*/
#define ATTR_SOCKET_E   0x10 //电量
#define ATTR_SOCKET_P   0x11 //功率
#define ATTR_SOCKET_V   0x12 //电压
#define ATTR_SOCKET_I   0x13 //电流
#define ATTR_SOCKET_WORKTIME 0x15 //插座通电时间
#define ATTR_SOCKET_MODE 0x16 //通电模式 0：按键可控，1：按键不可控，一直供电

/*设备属性*/
#define ATTR_DEVICETYPE     0x01      //设备类型
#define ATTR_DEVICENAME     0x02      //设备名称
#define ATTR_DEVICESTATUS   0x03      //设备工作状态：0关机 1工作 2待机 
#define ATTR_DEVICEMODE     0x04      //设备工作模式：（空调：0制热、1制冷，加湿机：0加湿、1除湿）
#define ATTR_SYSMODE        0x05      //系统温控模式 0空调制热 1空调制冷 2锅炉制热

#define ATTR_WINDSPEED         0x05               //设备风速 中低高三档
#define ATTR_WINDSPEED_NUM     0x06               //设备风速 0~100
#define ATTR_TEMPERATURE       0x07               //设备温度 0~100

/*环境属性*/
#define ATTR_ENV_TEMPERATURE   0x40              //温度
#define ATTR_ENV_HUMIDITY      0x41              //湿度
#define ATTR_ENV_PM25          0x42              //PM2.5
#define ATTR_ENV_CO2           0x43              //CO2
#define ATTR_ENV_FORMALDEHYDE  0x44              //甲醛
#define ATTR_ENV_TV0C          0x45              //TVOC

#define ATTR_SEN_WATERPRESSURE 0x50    //水压

/*
**http常量
**
*/
#define HTTP_UPLOAD_URL "http://123.206.15.63:8060/manager/upload"


/*LED*/
#define SYS_LED 1
#define NET_LED 2
#define ZGB_LED 3

#define SYS_LED_DIR "/sys/class/leds/wrtnode:blue:sys"
#define NET_LED_DIR "/sys/class/leds/wrtnode:blue:net"
#define ZGB_LED_DIR "/sys/class/leds/wrtnode:blue:zigbee"

#define LED_ACTION_OFF 0
#define LED_ACTION_ON 1
#define LED_ACTION_TRIGGER 2

#endif
