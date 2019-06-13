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
#define TYPE_NETWORK_NOCLOSE      0x04
#define TYPE_CLOSE_NETWORK        0x03

#define HOTWATER_DEFAULTTEMPERATURE 45

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
#define TOPICSNUM          4
#define RESPONSE_WAIT      3000 //��Ϣ��Ӧ�ȴ�ʱ��(����)
#define ZGB_ADDRESS_LENGTH 8
#define ZGBMSG_MAX_NUM     20

#define TOPIC_DEVICE_ADD    "devices/add"
#define TOPIC_DEVICE_STATUS "devices/status"
#define TOPIC_DEVICE_SHOW   "devices/show"
#define TOPIC_DEVICE_DELETE "/server/device/delete"

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
#define DEV_GATEWAY		              0x00	//����
#define DEV_CONTROL_PANEL             0x01  //�������
#define DEV_SOCKET		              0x02	//����
#define DEV_AIR_CON		              0x03	//�յ�����
#define DEV_FAN_COIL		          0x04	//����̹�
#define DEV_FLOOR_HEAT	              0x05	//��ů
#define DEV_FRESH_AIR			      0x06	//�·�
#define DEV_HUMIDIFIER                0x07  //��ʪ��
#define DEV_DEHUMIDIFIER              0x08  //��ʪ��
#define DEV_BOILER			          0x09	//��¯
#define DEV_WATER_PUMP	              0x0A	//ѭ����
#define DEV_DUST_CLEAN	              0x0B  //������

#define DEV_CURTAIN                   0x10  //����
#define DEV_WATER_PURIF	              0x11	//��ˮ��
#define DEV_WATER_HEAT	              0x12	//��ˮ��
#define DEV_LIGHT                     0x13  //�ƾ�
#define DEV_AJUSTABLE_LIGHT           0x14  //�ɵ���

#define SEN_WIND_PRESSURE             0x50  //��ѹ������
#define SEN_WATER_FLOW                0x51  //ˮ������
#define SEN_WATER_TEMPERATURE         0x52  //ˮ�´�����
#define SEN_ENV_BOX                   0x53  //�������ݴ�����(��������)
#define SEN_ELECTRICITY_METER         0x54  //������ 
#define SEN_GAS_METER                 0x55  //������
#define SEN_ANEMOGRAPH                0x56  //���ټ�
#define DEV_ANYONE                    0xFF  //�κ��豸



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
#define OTA_BEGIN          0x00 //����׼��
#define OTA_READY		   0x01	//׼���ý���
#define OTA_ONE_OK         0x02 //�������Ľ���OK
#define OTA_ONE_FAIL       0x03 //�������Ľ���ʧ�ܣ�Ҫ���ش�
#define OTA_REUPDATE       0x04 //��������
#define OTA_END		       0x05 //���½���
#define OTA_SUCCESS		   0x06 //�����ɹ�

/*ϵͳ��Ϣ 0x00~0x0f*/
/*��Ӧ����*/
#define ATTR_RESPONSE     0x00 

#define TLV_VALUE_RTN_OK			0x00	//�����ɹ�
#define TLV_VALUE_RTN_FAIL		    0x01	//����ʧ��
#define TLV_VALUE_RTN_ILLEGAL       0x02	//�Ƿ�����
#define TLV_VALUE_RTN_PARAM		    0x03	//��������
#define TLV_VALUE_RTN_BUSY		    0x04	//ϵͳæ

/*�����ϱ�*/
#define ATTR_FAULT 0xFF 

#define TLV_VALUE_FAULT_SOCKET    0x01 //��������˿��
#define TLV_VALUE_FAULT_PURIFIER  0x02 //��ˮ����о����

#define TLV_VALUE_POWER_OFF       0x00 //�ػ�
#define TLV_VALUE_POWER_ON        0x01 //�ϵ�
#define TLV_VALUE_STANDBY         0x02 //����

#define TLV_VALUE_HEAT             0 //����
#define TLV_VALUE_COLD             1 //����
#define TLV_VALUE_HUMIDIFICATION   0 //��ʪ
#define TLV_VALUE_DEHUMIDIFICATION 1 //��ʪ

#define TLV_VALUE_COND_HEAT        1 //�յ�����
#define TLV_VALUE_COND_COLD        2 //�յ�����
#define TLV_VALUE_BOILER_HEAT      3 //��¯����

#define TLV_VALUE_ONLINE 1
#define TLV_VALUE_OFFLINE 0


/*�豸����*/
#define ATTR_DEVICETYPE               0x01  //�豸����
#define ATTR_DEVICENAME               0x02  //�豸����
#define ATTR_DEVICESTATUS             0x03  //�豸����״̬��0-�ػ� 1-���� 2-���� 
#define ATTR_SYSMODE                  0x04  //ϵͳ�¿�ģʽ��1-�յ����� 2-�յ����� 3-��¯����
#define ATTR_AIR_CONDITION_MODE       0x05  //�յ�����ģʽ��1-���䡢����
#define ATTR_WINDSPEED                0x06  //���٣�0-�� 1-�� 2-��
#define ATTR_WINDSPEED_NUM            0x07  //�޼�����: 0-100
#define ATTR_SETTING_HUMIDITY         0x08  //�豸�趨ʪ�ȣ�0-100
#define ATTR_SETTING_TEMPERATURE      0x09  //�豸�趨�¶�: 0-100

/*��������*/
#define ATTR_SOCKET_E                 0x10 //����
#define ATTR_SOCKET_P                 0x11 //����
#define ATTR_SOCKET_V                 0x12 //��ѹ
#define ATTR_SOCKET_I                 0x13 //����
#define ATTR_SOCKET_WORKTIME          0x15 //����ͨ��ʱ��

#define ATTR_CONNECTED_AIRCONDITON    0x20  //����Ƿ������˿յ�����  0-δ���� 1-����
#define ATTR_SYSTEM_BOILER            0x21  //ϵͳ���ƹ�¯�Ĳ���

/*��������*/
#define ATTR_ENV_TEMPERATURE   0x40         //�¶�
#define ATTR_ENV_HUMIDITY      0x41         //ʪ��
#define ATTR_ENV_PM25          0x42         //PM2.5
#define ATTR_ENV_CO2           0x43         //CO2
#define ATTR_ENV_FORMALDEHYDE  0x44         //��ȩ
#define ATTR_ENV_TV0C          0x45         //TVOC
#define ATTR_SEN_WATER_YIELD   0x46         //ˮ��
#define ATTR_SEN_WATERPRESSURE 0x47         //ˮѹ
#define ATTR_SEN_WATERSPEED    0x48         //ˮ��
#define ATTR_SEN_WINDSPEED     0x49         //���� ��λm/s

#define ATTR_HOTWATER_SYSTEM_STATUS       0x50 //��ˮϵͳ״̬       0�ر� 1����
#define ATTR_HOTWATER_TEMPERATURE_TARGET  0x51 //��ˮ��Ŀ���¶�
#define ATTR_HOTWATER_TEMPERATURE_CURRENT 0x52 //��ˮ����ˮ�ڵ�ǰ�¶�
#define ATTR_HOTWATER_TEMPERATURE_CLEAN   0x53 //��ˮ������¶�
#define ATTR_HOTWATER_TIME                0x54 //��ˮ�����ʱ��
#define ATTR_HOTWATER_CONNECTED_SOCKET    0x55 //��ˮ����������
#define ATTR_HOTWATER_CONNECTED_TEMPERATURE_SEN 0x56 //��ˮ����ˮ���¶ȴ�����

#define TLV_VALUE_SPEED_HIGH    0x00     //���̸���
#define TLV_VALUE_SPEED_MEDIU   0x01     //��������
#define TLV_VALUE_SPEED_LOW     0x02     //���̵���


#define GATEWAY_ID "00000000000000000"
/*
**http����
**
*/
#define HTTP_UPLOAD_URL "http://123.206.15.63:8060/manager/upload"

#define OP_TYPE_HOUR 10
#define OP_TYPE_DAY 11
#define OP_TYPE_MONTH 12
#define OP_TYPE_YEAR 13


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

#define CHECK_NULL_RETURN(obj)  if(!obj) return;
#define CHECK_NULL_CONTINUE(obj) if(!obj) continue;
#define CHECK_NULL_BREAK(obj)   if(!obj) break; 

#define OPERATIONLOG 1
#define QUERYLOG     2

#endif
