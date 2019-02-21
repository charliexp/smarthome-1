#ifndef _CONST_H_
#define _CONST_H_

/*COO AT����*/
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

/*=====================LOG ����=========================*/

#define LOG_FILE "smarthomelog.log"
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_ERROR 2


/*=====================MQTT ����=========================*/
#define THREAD_NUMS 8
#define ADDRESS     "tcp://123.206.15.63:1883" //mosquitto server ip
#define LAN_MQTT_SERVER "tcp://127.0.0.1:1883"
#define CLIENTID    "todlee"                   //�ͻ���ID
#define CLIENTID1   "todlee_pub"              //�ͻ���ID

#define WAN_CLIENT_ID 1
#define WAN_CLIENT_PUB_ID 2
#define LAN_CLIENT_ID 3
#define LAN_CLIENT_PUB_ID 4

#define USERNAME    "root"
#define PASSWORD    "root"
#define TOPICSNUM          3
#define RESPONSE_WAIT      3000 //��Ϣ��Ӧ�ȴ�ʱ��(����)
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

/*�豸����*/
#define DEV_GATEWAY			   0x00	// ����
#define DEV_SOCKET		       0x01	// ����
#define DEV_AIR_CON		       0x02	// �յ�
#define DEV_BOILER			   0x03	// ��¯
#define DEV_FAN_COIL		   0x04	// ����̹�
#define DEV_FLOOR_HEAT	       0x05	// ��ů
#define DEV_FRESH			   0x06	// �·�
#define DEV_WATER_PURIF	       0x07	// ��ˮ��
#define DEV_WATER_HEAT	       0x08	// ��ˮ��
#define DEV_WATER_PUMP	       0x09	// ѭ����
#define DEV_DUST_CLEAN	       0x0A // ������
#define DEV_CONTROL_PANEL      0x0B // �������
#define DEV_HUMIDIFIER         0x0C // ��ʪ��
#define DEV_DEHUMIDIFIER       0x0D // ��ʪ��
#define SEN_WIND_PRESSURE      0x50 // ��ѹ������
#define SEN_WATER_FLOW         0x51 // ˮ������
#define SEN_WATER_TEMPERATURE  0x52 // ˮ�´�����
#define SEN_ENV_DATA           0x53 // �������ݴ�����
#define SEN_ELECTRICITY_METER  0x54 // ������ 
#define SEN_GAS_METER          0x55 // ������
#define SEN_ANEMOGRAPH         0x56 // ���ټ�
#define SEN_WATER_MANOMETER    0x57 // ˮѹ��
#define DEV_ANYONE             0xFF // �κ��豸


#define DEV_ONLINE 1
#define DEV_OFFLINE 0
/*��Ϣ����     ��Ӧmsgtype*/
#define ZGB_MSGTYPE_DEVICEREGISTER          0x00 //Ҫ���豸ע��
#define ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE 0x01 //�豸ע����Ӧ
#define ZGB_MSGTYPE_DEVICE_OFFNET           0x02 //Ҫ���豸����
#define ZGB_MSGTYPE_DEVICE_OPERATION        0x03 //�豸����
#define ZGB_MSGTYPE_DEVICE_OPERATION_RESULT 0x04 //�豸�������
#define ZGB_MSGTYPE_DEVICE_STATUS_QUERY     0x05 //�豸״̬��ѯ
#define ZGB_MSGTYPE_DEVICE_STATUS_REPORT    0x06 //�豸״̬�ϱ�
#define ZGB_MSGTYPE_DEVICE_LOCATION         0x07 //�豸��λ  
#define ZGB_MSGTYPE_UPDATE                  0x10 //���������ʾ
#define ZGB_MSGTYPE_UPDATE_RESPONSE         0x11 //���������Ӧ
#define ZGB_MSGTYPE_UPDATE_PAYLOAD          0x12 //��������
#define ZGB_MSGTYPE_UPDATE_RESULT           0x13 //�������


/*����״̬ msgtype��ӦZGB_MSGTYPE_UPDATE_RESPONSE*/
#define OTA_BEGIN          0x00 // ����׼��
#define OTA_READY		   0x01	// ׼���ý���
#define OTA_ONE_OK         0x02 // �������Ľ���OK
#define OTA_ONE_FAIL       0x03 // �������Ľ���ʧ�ܣ�Ҫ���ش�
#define OTA_REUPDATE       0x04 // ��������
#define OTA_END		       0x05 // ���½���
#define OTA_SUCCESS		   0x06 // �����ɹ�

/*ϵͳ��Ϣ 0x00~0x0f*/
/*��Ӧ����*/
#define ATTR_RESPONSE     0x00 

#define TLV_VALUE_RTN_OK			0x00	// �����ɹ�
#define TLV_VALUE_RTN_FAIL		    0x01	// ����ʧ��
#define TLV_VALUE_RTN_ILLEGAL       0x02	// �Ƿ�����
#define TLV_VALUE_RTN_PARAM		    0x03	// ��������
#define TLV_VALUE_RTN_BUSY		    0x04	// ϵͳæ

/*�����ϱ�*/
#define ATTR_FAULT 0xFF 

#define TLV_VALUE_FAULT_SOCKET    0x01 //��������˿��
#define TLV_VALUE_FAULT_PURIFIER  0x02 //��ˮ����о����

#define TLV_VALUE_POWER_OFF  0x00 //�ػ�
#define TLV_VALUE_POWER_ON   0x01 //�ϵ�
#define TLV_VALUE_STANDBY    0x02 //����

#define TLV_VALUE_HEAT  0 //����
#define TLV_VALUE_COLD  1 //����
#define TLV_VALUE_HUMIDIFICATION 0 //��ʪ
#define TLV_VALUE_DEHUMIDIFICATION 1 //��ʪ

#define TLV_VALUE_COND_HEAT 0 //�յ�����
#define TLV_VALUE_COND_COLD 1 //�յ�����
#define TLV_VALUE_BOILER_HEAT 2 //��¯����

#define TLV_VALUE_ONLINE 1
#define TLV_VALUE_OFFLINE 0

/*��������*/
#define ATTR_SOCKET_E   0x10 //����
#define ATTR_SOCKET_P   0x11 //����
#define ATTR_SOCKET_V   0x12 //��ѹ
#define ATTR_SOCKET_I   0x13 //����
#define ATTR_SOCKET_WORKTIME 0x15 //����ͨ��ʱ��
#define ATTR_SOCKET_MODE 0x16 //ͨ��ģʽ 0�������ɿأ�1���������ɿأ�һֱ����

/*�豸����*/
#define ATTR_DEVICETYPE     0x01      //�豸����
#define ATTR_DEVICENAME     0x02      //�豸����
#define ATTR_DEVICESTATUS   0x03      //�豸����״̬��0�ػ� 1���� 2���� 
#define ATTR_DEVICEMODE     0x04      //�豸����ģʽ�����յ���0���ȡ�1���䣬��ʪ����0��ʪ��1��ʪ��
#define ATTR_SYSMODE        0x05      //ϵͳ�¿�ģʽ 0�յ����� 1�յ����� 2��¯����

#define ATTR_WINDSPEED         0x05               //�豸���� �е͸�����
#define ATTR_WINDSPEED_NUM     0x06               //�豸���� 0~100
#define ATTR_TEMPERATURE       0x07               //�豸�¶� 0~100

/*��������*/
#define ATTR_ENV_TEMPERATURE   0x40              //�¶�
#define ATTR_ENV_HUMIDITY      0x41              //ʪ��
#define ATTR_ENV_PM25          0x42              //PM2.5
#define ATTR_ENV_CO2           0x43              //CO2
#define ATTR_ENV_FORMALDEHYDE  0x44              //��ȩ
#define ATTR_ENV_TV0C          0x45              //TVOC

#define ATTR_SEN_WATERPRESSURE 0x50    //ˮѹ

/*
**http����
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
