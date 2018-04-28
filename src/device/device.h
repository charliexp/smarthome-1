#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include "cJSON.h"

#define USERNAME_MAXLENGTH 50
#define TOPIC_LENGTH 100
#define MSG_LENGTH 200

/*设备类型*/
#define DEV_BOOT			0x00	// 空设备，特指Boot Loader
#define DEV_SOCKET		    0x01	// 插座
#define DEV_AIR_CON		    0x02	// 空调外机
#define DEV_BOILER			0x03	// 锅炉
#define DEV_FAN_COIL		0x04	// 风机盘管
#define DEV_FLOOR_HEAT	    0x05	// 地暖
#define DEV_FRESH			0x06	// 新风
#define DEV_WATER_PURIF	    0x07	// 净水器
#define DEV_WATER_HEAT	    0x08	// 热水器
#define DEV_WATER_PUMP	    0x09	// 循环泵
#define DEV_DUST_CLEAN	    0x0A // 除尘器
#define DEV_MULTI_PANEL     0x0B // 多合一控制面板

/*操作类型*/
#define ZGB_RESPONSE 0x01 //响应报文
#define ZGB_DEVICETYPE 0x11 //设备类型
#define ZGB_HUMIDITY 0x21 //湿度调节
#define ZGB_COLD_WARM_REGULATION 0x22 //冷暖调节
#define ZGB_AIR_CONDITIONING_THREE_WIND_LEVEL 0x23 //空调三级风速
//#define ZGB_HUMIDITY 0x24 //空调无级风速
//#define ZGB_HUMIDITY 0x25 //新风三级风速
//#define ZGB_HUMIDITY 0x26 //新风无级风速
//#define ZGB_HUMIDITY 0x27 //空调温度调节
//#define ZGB_HUMIDITY 0x28 //地暖温度调节
//#define ZGB_HUMIDITY 0x40 //环境盒子温度数据
//#define ZGB_HUMIDITY 0x41 //环境盒子湿度数据
//#define ZGB_HUMIDITY 0x42 //环境盒子PM2.5数据
//#define ZGB_HUMIDITY 0x43 //环境盒子CO2数据
//#define ZGB_HUMIDITY 0x44 //环境盒子甲醛数据
//#define ZGB_HUMIDITY 0x45 //环境盒子TVOC数据

typedef char zgbaddress[8];
typedef char BYTES2[2];

typedef struct
{
	BYTES2 index;
	char sub;
	char opt;
	char length;
	char data[72];
}ADF;


typedef struct  
{
	BYTES2 framecontrol;
	char reserved0[6];
	zgbaddress src;
	zgbaddress dest;
	char reserved1[2];
	BYTES2 cmdid;
	BYTES2 groupid;
	ADF adf;
}Payload;

typedef struct
{
	char header;
	char lenght;
	Payload payload;
	char check;
	char footer;
} zigbeemsg;

typedef enum
{
    BOX = 1,
	AIRCONDITIONING,
	NEWTREND,
	OTHERS,
} DEVICETYPE;

typedef enum
{
	ON,
    CLOSE,
    STANDBY,
} DEVICESTATUS;

typedef struct
{
	char username[USERNAME_MAXLENGTH];
	char mqttid[16];
    zgbaddress address;
	char devicename[125];
	DEVICETYPE devicetype;
	char operation;
} deviceoperation;

typedef struct  
{
	long msgtype;
	cJSON* p_operation_json;
}deviceoperationmsg;

typedef struct
{
	int qos;
	int retained;
	char topic[TOPIC_LENGTH];
	char msg[MSG_LENGTH];
}mqttqueuemsg;

typedef struct
{
	long msgtype;
	mqttqueuemsg msg;
}mqttmsg;

typedef struct
{
	char packetid;
	char result;
	char over;//判断该消息是否已收到回复
}ZGB_MSG_STATUS;

/*分配zigbee报文的packetid*/
char getpacketid(void);

/*空调控制接口*/
int airconditioningcontrol(cJSON *op);

/*新风控制接口*/
int airconcontrol(cJSON *op);

/*zigbee消息发送接口*/
int sendzgbmsg(zgbaddress address, char *data, char length);

#endif
