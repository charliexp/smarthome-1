#include "app.h"
#include "mqtt.h"
#include "test/test.h"

/*ȫ�ֱ���*/
int g_uartfd;
int g_log_level = 2;
int g_operationflag = 0;
int g_queueid;
sqlite3* g_db;
char g_mac[20] = {0};
char g_topicroot[20] = {0};
char* g_topics[TOPICSNUM] ={0x0, 0x0, 0x0};
int g_zgbmsgnum;
cJSON* g_device_mqtt_json, *g_devices_status_json;
ZGB_MSG_STATUS g_devicemsgstatus[ZGBMSG_MAX_NUM];

//��������������Ѵ����Ҫ���ĵ�topic
char g_topicthemes[TOPICSNUM][20] = {{"devices/operation"},{"devices/electric"}{"gateway"}};
char g_clientid[30], g_clientid_pub[30];


/*������Ҫ�����ݿ��*/
int sqlitedb_init()
{
    int rc; 
    char sql[512];  

    rc = sqlite3_open("/usr/smarthome.db", &g_db);
    if(rc != SQLITE_OK)  
    {  
        MYLOG_ERROR("open smarthome.db error!");  
        return -1;  
    }
    sprintf(sql,"CREATE TABLE devices (deviceid TEXT, zgbaddress TEXT, devicetype CHAR(1), deviceindex CHAR(1), online CHAR(1), primary key(deviceid));");
    exec_sql_create(sql);
        
    sprintf(sql,"CREATE TABLE [electricity_day]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[day] INT NOT NULL, primary key(deviceid, day));");
    exec_sql_create(sql);

    sprintf(sql,"CREATE TABLE [electricity_month]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[month] INT NOT NULL, primary key(deviceid, month));");
    exec_sql_create(sql);

    sprintf(sql,"CREATE TABLE [electricity_year]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[year] INT NOT NULL, primary key(deviceid, year));");
    exec_sql_create(sql);

    sprintf(sql,"CREATE TABLE [electricity_hour]([deviceid] TEXT NOT NULL,[electricity] INT NOT NULL,[hour] INT NOT NULL, primary key(deviceid, hour));");
    exec_sql_create(sql);
    
    return 0;
}


/*����������Ҫ�õ�����Ϣ���У�����MQTT�Ķ��ġ�������ȥ���ĺʹ�ȡ�豸������Ϣ�Լ�zgb��Ϣ�Ĵ洢*/
int createmessagequeue()
{   
	key_t key;
	key = ftok("/", 0);
    
	g_queueid = msgget(key, IPC_CREAT | 0644);
	if (g_queueid < 0)
	{
		MYLOG_ERROR("get ipc_id error!");
		return -1;
	}
    return 0;  
}


/*��������������*/
void* lantask(void *argc)
{
    struct sockaddr_in servaddr,appaddr;
    char text[] = "smarthome app";
    int listenfd,sendfd;
    char buf[128];               
                         
    listenfd = socket(AF_INET,SOCK_DGRAM,0);
    sendfd = socket(AF_INET,SOCK_DGRAM,0);
    int set = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int));
    setsockopt(sendfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int));

    bzero(&servaddr,sizeof(servaddr));
    bzero(&appaddr,sizeof(appaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(8787);

    bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));  //�˿ڰ�
    int recvbytes;
    int addrlen = sizeof(struct sockaddr_in);

    while(1)
    {
        if((recvbytes = recvfrom(listenfd, buf, 128, 0, (struct sockaddr *)&appaddr, &addrlen)) != -1)
        {
            buf[recvbytes] = '\0';
            MYLOG_DEBUG("Lantask revc a udp msg!, the msg is %s", buf);
            if(memcmp(buf, text, strlen(text))==0)
            {
                int sendBytes;
                appaddr.sin_family = AF_INET;
                appaddr.sin_addr.s_addr = inet_addr(inet_ntoa(appaddr.sin_addr));
                appaddr.sin_port = htons(8787);  

                MYLOG_DEBUG("Send a msg to app!srcip is %s", inet_ntoa(appaddr.sin_addr));
                if((sendBytes = sendto(sendfd, text, strlen(text), 0, (struct sockaddr *)&appaddr, sizeof(struct sockaddr))) == -1)
                {
                    MYLOG_ERROR("sendto fail!");
                }
            }
        }
        else
        {
            MYLOG_ERROR("recvfrom fail\n");
        }
    }
	pthread_exit(NULL);    
}


void init()
{
	int i = 0;

    //��ȡ����mac��ַ
	if(getmac(g_mac) == 0)
	{
        sprintf(g_topicroot, "/%s/", g_mac);
        sprintf(g_clientid, "[%s]", g_mac);
        sprintf(g_clientid_pub, "[%s]_pub", g_mac);
	}
    
    if(init_uart("/dev/ttyS1") == -1)
    {
        MYLOG_ERROR("init_uart fail!");
    }

    if(createmessagequeue() == -1)
    {
        MYLOG_ERROR("CreateMessageQueue failed!");//�������Ҫ��������Ĺ���
    } 

    if(sqlitedb_init() == -1)
    {
        MYLOG_ERROR("Create db failed!");      
    }   
    
	for(; i < TOPICSNUM; i++)
	{
		g_topics[i] = (char*)malloc(sizeof(g_topicroot) + strlen(g_topicthemes[i])-1);
		sprintf(g_topics[i], "%s%s", g_topicroot, g_topicthemes[i]);
	}
    devices_status_json_init();
    gatewayregister(); 
}



/*APP�·����豸��Ϣ�Ĵ������*/
void* devicemsgprocess(void *argc)
{
	devicequeuemsg msg; //��Ϣ������ȡ�����豸������Ϣ
	char topic[TOPIC_LENGTH] = { 0 };//�Բ�������Ӧtopic
	int mqttid = 0;
	cJSON *devices, *device, *operations, *operation, *tmp;
	int devicenum = 0, operationnum = 0;
	int i = 0;
	int rcvret;
    int operationtype;
	char packetid;
	char* deviceid;
	size_t msglen = sizeof(devicequeuemsg);

    ZGBADDRESS src;
    zgbqueuemsg qmsg;
    char db_zgbaddress[20];
    char devicetype,deviceindex;
    char *zErrMsg = 0;  
    char sql[512]; 
    BYTE data[72];
    int rc;  
    int len = 0;         

	while(1)
	{
		if (rcvret = msgrcv(g_queueid, (void*)&msg, msglen, QUEUE_MSG_DEVIC, 0) <= 0)
		{
            MYLOG_ERROR("recive devicequeuemsg fail!rcvret = %d", rcvret);
            continue;
		}
        
		MYLOG_INFO("devicemsgprocess recive a msg");
		MYLOG_INFO("the msg is %s", cJSON_PrintUnformatted(msg.p_operation_json));
        
		g_device_mqtt_json = msg.p_operation_json;
		cJSON_AddNumberToObject(g_device_mqtt_json, "resultcode", 0);
        tmp = cJSON_GetObjectItem(g_device_mqtt_json, "operation");
        if (tmp == NULL)
        {
            MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
            cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
            cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            goto response;
        }          
        operationtype = tmp->valueint;//��������

        //������cooģ��
        if(operationtype == 0)
        {
            int actiontype;
            tmp = cJSON_GetObjectItem(g_device_mqtt_json, "cmdid");
            if (tmp == NULL)
            {
                MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                goto response;
            }             
            actiontype = tmp->valueint;//coo����ֵ
                     
            switch (actiontype)
            {
                case TYPE_CREATE_NETWORK:
                    write(g_uartfd, AT_CREATE_NETWORK, strlen(AT_CREATE_NETWORK));
                    MYLOG_INFO("COO operation AT+FORM=02");
                    break;
                case TYPE_OPEN_NETWORK:
                    write(g_uartfd, AT_CREATE_NETWORK, strlen(AT_CREATE_NETWORK));
                    MYLOG_INFO("COO operation AT+FORM=02");
                    milliseconds_sleep(2000);
                    write(g_uartfd, AT_OPEN_NETWORK, strlen(AT_OPEN_NETWORK));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=60");
                    reportdevices();
                    break;
                case TYPE_DEVICE_LIST:
                    write(g_uartfd, AT_DEVICE_LIST, strlen(AT_DEVICE_LIST));
                    MYLOG_INFO("COO operation AT+LIST");                    
                    break;   
                case TYPE_NETWORK_NOCLOSE:
                    write(g_uartfd, AT_NETWORK_NOCLOSE, strlen(AT_NETWORK_NOCLOSE));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=FF");
                    break;
                case TYPE_CLOSE_NETWORK:
                    write(g_uartfd, AT_CLOSE_NETWORK, strlen(AT_CLOSE_NETWORK));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=00");
                    break;
                case TYPE_NETWORK_INFO:
                    write(g_uartfd, AT_NETWORK_INFO, strlen(AT_NETWORK_INFO));
                    MYLOG_INFO("COO operation AT+GETINFO");
                    break;   
                default:
                    MYLOG_ERROR("Unknow actiontype!");
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            }
            goto response;
        }
        
		devices = cJSON_GetObjectItem(g_device_mqtt_json, "devices");
		if (devices == NULL)
		{
			MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
            cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
            cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            goto response;
		} 
        
		devicenum = cJSON_GetArraySize(devices);//һ����Ϣ�в������豸����

        //�豸����
        if (operationtype == 1)
        {
    		for (i=0; i< devicenum; i++)
    		{
    			device = cJSON_GetArrayItem(devices, i);

                /***
                **ȫ�ֵ��豸�������������һ���µ�δ�����
                **�豸��������
                ***/
    			packetid = getpacketid(); //packetid��������zgb�豸ͨ��ʹ�õ�
    			g_devicemsgstatus[i].packetid = packetid;
    			g_devicemsgstatus[i].result = 1; //1����δ��Ӧ��0����ɹ�
    			g_devicemsgstatus[i].finish = 0;
    			g_zgbmsgnum++;
                
                tmp = cJSON_GetObjectItem(device, "deviceid");
                if(tmp == NULL)
    		    {
                    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                    goto response;
    			}                
                
    			deviceid = tmp->valuestring;

                int nrow = 0, ncolumn = 0;
    	        char **dbresult; 
                
                sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", deviceid);
                MYLOG_INFO(sql);
                
                sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
                if(nrow == 0) //���ݿ���û�и��豸
                {
                    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_UNKNOW_DEVICE);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_DEVICENOEXIST));
                    goto response;
                }

                strncpy(db_zgbaddress, dbresult[ncolumn+1], 20);
                dbaddresstozgbaddress(db_zgbaddress, src);
                devicetype  = dbresult[ncolumn+2][0] - '0';
                deviceindex = dbresult[ncolumn+3][0] - '0';
                operations  = cJSON_GetObjectItem(device, "operations");
    		    if (operations == NULL)
    		    {
    			    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                    goto response;
    		    }
    		    
    		    BYTE data[125];
                int datalen = mqtttozgb(operations, data, devicetype);
                sendzgbmsg(src, data, datalen, ZGB_MSGTYPE_DEVICE_OPERATION, devicetype, deviceindex, packetid);
                sqlite3_free_table(dbresult);             
    		}

    		for (i=0; i<RESPONSE_WAIT/50; i++)  //50����ѭ��һ��
    		{
    			int j = 0;
    			for (; j < g_zgbmsgnum; j++)
    			{
    				if (g_devicemsgstatus[j].finish == 0)
    					break;
    			}
    			if (j == g_zgbmsgnum)//���е�zgb��Ϣ���Ѵ�����
    			{
    				break;
    			}
    			milliseconds_sleep(50);
    		}

    		for (i=0; i<devicenum; i++)
    		{
    			device = cJSON_GetArrayItem(devices, i);
    			cJSON_AddNumberToObject(device, "result", g_devicemsgstatus[i].result);
    		}            
        }
        
        //�豸״̬��ѯ
        if (operationtype == 2)
        {
    		for (i=0; i< devicenum; i++)
    		{
    			device = cJSON_GetArrayItem(devices, i); 
                tmp = cJSON_GetObjectItem(device, "deviceid");
                MYLOG_DEBUG("The device is %s", cJSON_PrintUnformatted(device));
                if(tmp == NULL)
    		    {
                    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_FORMAT_ERROR);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                    goto response;
    			}                
                
    			deviceid = tmp->valuestring;

                int nrow = 0, ncolumn = 0;
    	        char **dbresult; 
                
                sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", deviceid);
                MYLOG_INFO(sql);
                
                sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
                MYLOG_DEBUG("query db over!");
                if(nrow == 0) //���ݿ���û�и��豸
                {
                    MYLOG_ERROR(MQTT_MSG_FORMAT_ERROR);
                    cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_UNKNOW_DEVICE);
                    cJSON_ReplaceItemInObject(g_device_mqtt_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_DEVICENOEXIST));
                    goto response;
                }
                cJSON* devicestatus = get_device_status_json(deviceid);
                if (devicestatus == NULL)
                {
                    MYLOG_ERROR("Can't find the device status!");
                }              
                cJSON* status = cJSON_GetObjectItem(devicestatus, "status");
                cJSON_AddItemToObject(device, "status", status);
                MYLOG_DEBUG("The status is %s", cJSON_PrintUnformatted(status));
            }   	    	
        }
        cJSON_AddStringToObject(g_device_mqtt_json, "result", MQTT_MSG_SUCCESS);
        
response:
        tmp = cJSON_GetObjectItem(g_device_mqtt_json, "mqttid");
        if (tmp != NULL)
        {
        	mqttid = tmp->valueint;
     		sprintf(topic, "%s%s/response/%d", g_topicroot, g_topicthemes[0], mqttid);
    		sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(g_device_mqtt_json), QOS_LEVEL_2, 0);       
        }

		cJSON_Delete(g_device_mqtt_json);
        g_zgbmsgnum = 0;
		g_operationflag = 0;
        g_device_mqtt_json = NULL;
	}
	pthread_exit(NULL);
}

/*ZGB��Ϣ���ͽ���*/
void* uartsend(void *argc)
{    
    uartsendqueuemsg qmsg;    
    int rcvret;
    
    MYLOG_DEBUG("Enter pthread uartsend");
    while(true)
    {
		if ( rcvret = msgrcv(g_queueid, (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_UART, 0) <= 0 )
		{
			MYLOG_ERROR("rcvret = %d", rcvret);
			MYLOG_ERROR("recive uartsendmsg fail!");
		}
        MYLOG_INFO("uart begin to send a msg!");
        MYLOG_ZGBMSG(qmsg.msg);
        if ( write(g_uartfd, (char *)&qmsg.msg, (int)(qmsg.msg.msglength + 2)) != (qmsg.msg.msglength + 2))
	    {
		    MYLOG_ERROR("com write error!");
	    }
	    if ( write(g_uartfd, (char *)&qmsg.msg+sizeof(qmsg.msg)-2, 2) != 2)//����check��footer
	    {
		    MYLOG_ERROR("com write check and footer error!");
	    }
    }
	pthread_exit(NULL);    
}

/*zgb��Ϣ����*/
void* zgbmsgprocess(void* argc)
{
	int rcvret;
    ZGBADDRESS src;
    DATA* zgbdata;
    bool needmqtt;
    zgbqueuemsg qmsg;
    char db_zgbaddress[17] = {0};
    char db_deviceid[20] = {0}; 
    char msgtype,devicetype,deviceindex,packetid;
    zgbmsg responsemsg;
    char topic[TOPIC_LENGTH] = {0};
    char *zErrMsg = NULL;  
    char sql[256] ={0}; 
    BYTE data[72];
    int rc;  
    int len = 0; 

    MYLOG_DEBUG("Enter pthread zgbmsgprocess");

	while(1)
	{
		if (rcvret = msgrcv(g_queueid , (void*)&qmsg, sizeof(qmsg), QUEUE_MSG_ZGB, 0) <= 0)
		{
            MYLOG_ERROR("recive zgbqueuemsg fail!");
            MYLOG_ERROR("rcvret = %d", rcvret);
		}
		MYLOG_INFO("zgbmsgprocess recive a msg");
		MYLOG_ZGBMSG(qmsg.msg);
        memcpy(src, qmsg.msg.payload.src, 8);
        zgbaddresstodbaddress(src, db_zgbaddress);

        if(qmsg.msg.payload.adf.index[0] == 0x00 && qmsg.msg.payload.adf.index[1] == 0x00) //�豸������Ϣ
        {
            int nrow = 0, ncolumn = 0;
	        char **dbresult;
        
            MYLOG_INFO("[ZGB DEVICE]Get a device network joining message.");
            sprintf(sql,"SELECT * FROM devices WHERE zgbaddress = '%s';", db_zgbaddress);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
            if(nrow == 0) //���ݿ���û�и��豸
            {
                milliseconds_sleep(1000);
                sendzgbmsg(src, NULL, 0, ZGB_MSGTYPE_DEVICEREGISTER, DEV_ANYONE , 0, getpacketid());//Ҫ���豸ע��
            }
            sqlite3_free_table(dbresult);
            continue;
        }
        
        zgbdata      = &qmsg.msg.payload.adf.data;
        msgtype      = zgbdata->msgtype;
        devicetype   = zgbdata->devicetype;
        deviceindex  = zgbdata->deviceindex;
        packetid     = zgbdata->packetid;
        
        sprintf(db_deviceid, "%s%d", db_zgbaddress, deviceindex);

        //����ǹ㲥���ģ�ֱ�Ӷ���
        ZGBADDRESS broadcastaddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if(memcmp(src, broadcastaddr, 8) == 0)
        {
            MYLOG_INFO("Drop a broadcast msg!");
            MYLOG_ZGBMSG(qmsg.msg);
            goto end;
        }

        if(zgbdata->magicnum != 0xAA)
        {
            MYLOG_INFO("Drop a not own msg!");
            MYLOG_ZGBMSG(qmsg.msg);
            goto end;
        }
        
        switch (msgtype)
        {
            case ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE:
            {
                 cJSON* root = cJSON_CreateObject();
                 int nrow = 0, ncolumn = 0;
                 char **azResult; 
                 
                 MYLOG_INFO("[ZGB DEVICE]Get a device network joining response message.");
                 
                 sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", db_deviceid);
                 MYLOG_INFO(sql);
            
                 sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
                 //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
                 if(nrow != 0) //���ݿ����и��豸
                 {
                    MYLOG_INFO("The device has been registered!");
                    break;
                 }
                 sqlite3_free_table(azResult);                 
                 nrow = 0, ncolumn = 0;
                 
                 sprintf(sql, "replace into devices values('%s', '%s', %d, %d, 1);", db_deviceid, db_zgbaddress, devicetype, deviceindex);
                 MYLOG_INFO("The sql is %s", sql);
                 rc = sqlite3_exec(g_db, sql, 0, 0, &zErrMsg);
                 if(rc != SQLITE_OK)
                 {
                     MYLOG_ERROR(zErrMsg);
                     break;
                 }
                 cJSON* devicestatus = create_device_status_json(db_deviceid, devicetype);
                 cJSON_AddItemToArray(g_devices_status_json, devicestatus);
                 MYLOG_INFO("The devices status is %s", cJSON_PrintUnformatted(g_devices_status_json));
                 
                 sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_ADD);
                 cJSON_AddStringToObject(root, "deviceid", db_deviceid);
                 cJSON_AddNumberToObject(root, "devicetype", devicetype);
                 sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt�����豸ע����Ϣ
                 cJSON_Delete(root);
                 break;
            }
            case ZGB_MSGTYPE_DEVICE_OPERATION_RESULT:
            {                
                for (int i = 0; i < ZGBMSG_MAX_NUM; ++i)
                {
                    if (g_devicemsgstatus[i].packetid == packetid)
                    {
                        g_devicemsgstatus[i].finish = 1;
                        g_devicemsgstatus[i].result = zgbdata->pdu[1];
                        break;
                    }
                }                              
                break;
            } 
            case ZGB_MSGTYPE_DEVICE_STATUS_REPORT://�豸״̬�ϱ�
            {
                cJSON *device_json = get_device_status_json(db_deviceid);
                cJSON *attr_json;
                cJSON *replace_value_json;
                int value,oldvalue;
                int i = 0;
                BYTE attr; 

                needmqtt = false;
                if(device_json == NULL)
                {
                    goto end;
                }
                
                while(i < (zgbdata->length - 7))
                {
                    attr = zgbdata->pdu[i++];
                    
                    attr_json = get_attr_value_object_json(device_json, attr);
                    replace_value_json = cJSON_CreateNumber(value);
                    
                    if(device_json == NULL)
                    {
                        goto end;
                    }
                    value = zgbdata->pdu[i++]*256*256*256 + zgbdata->pdu[i++]*256*256 
                        + zgbdata->pdu[i++]*256 + zgbdata->pdu[i++];
                       
                    switch(attr)
                    {
                        case ATTR_DEVICESTATUS:
                        case ATTR_SOCKET_V:
                        {                           
                            oldvalue = cJSON_GetObjectItem(attr_json, "value")->valueint;
                            if(value == oldvalue)
                            {
                                needmqtt = false || needmqtt;    
                            }
                            else
                            {
                                //�޸ĵ��Ǹ��Ƴ������豸���Ա�
                                cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);
                                //�޸ĵ���ȫ�ֵ��豸���Ա�
                                change_device_attr_value(db_deviceid, attr, value);
                                needmqtt = true || needmqtt;
                            }
                            break;
                        }
                        case ATTR_SOCKET_E:
                        {
                            change_device_attr_value(db_deviceid, attr, value);                        
                            needmqtt = false || needmqtt;
                            MYLOG_INFO("Get a socket electricity report msg!the value is %lu", value);
                            electricity_stat(db_deviceid, value);
                            break;
                        }
                        default:
                            break;
                    }
                }
                if(needmqtt)
                {
                    sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_STATUS);
                    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(device_json), 0, 0);
                }
                break;
            }
            case ZGB_MSGTYPE_DEVICE_LOCATION:
            {
                 cJSON* root = cJSON_CreateObject();
                 int nrow = 0, ncolumn = 0;
                 char **azResult; 
                 
                 MYLOG_INFO("[ZGB DEVICE]Get a device location message.");
                 
                 sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", db_deviceid);
                            
                 sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
                 //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
                 if(nrow == 0) //���ݿ���û�и��豸
                 {
                    MYLOG_INFO("The device has been registered!");
                    break;
                 }
                 sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_SHOW);
                 cJSON_AddStringToObject(root, "deviceid", db_deviceid);
                 cJSON_AddNumberToObject(root, "type", devicetype);
                 sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), 0, 0);
                 sqlite3_free_table(azResult);                
            }
            default: 
                ;
        }
end:
    ;
	}
	pthread_exit(NULL);        
}


/*�������ڵĽ��̣���ȡ��Ƭ���ϴ���ZGB��Ϣ*/
void* uartlisten(void *argc)
{
	BYTE msgbuf[1024]; 
	pthread_t threads[2];
	int nbyte=0, bitnum=0;
	zgbmsg zmsg;
    zgbqueuemsg zgbqmsg;
	int i, j, sum;
    int ret;

    pthread_create(&threads[0], NULL, uartsend, NULL);
    pthread_create(&threads[1], NULL, zgbmsgprocess, NULL);

    MYLOG_DEBUG("pthread uartlisten begin");
    
	while(1)
	{
		nbyte = read(g_uartfd, msgbuf, 110);
        //MYLOG_INFO("Uart recv %d byte:", nbyte);
        //MYLOG_BYTE(msgbuf, nbyte);
        bitnum = nbyte;
		for(i = 0; i < bitnum; )
		{
			if(msgbuf[i] != 0x2A)//����ZGB�Զ��屨�Ŀ�ʼ�ֽ�
			{
				i++;
				continue;
			}

            if(nbyte > 2 &&(msgbuf[i+2] != 0x41)) 
            {
                continue;
            }

            if(nbyte > 3 &&(msgbuf[i+3] != 0x88))
            {
                continue;
            }
            
			/*��ʼ��ȡZGB��Ϣ*/
			zgbmsginit(&zmsg);
			zmsg.msglength = msgbuf[i + 1];
            if(zmsg.msglength+4 > bitnum-i) //���ZGB��Ϣ���ȴ��ڽ��ܵ��ֽ�ʣ�೤��˵����Ҫ���ܺ����ֽ�
            {
                int needbyte = (zmsg.msglength + 4) -  (bitnum - i);//���쵱ǰ���Ļ�����յ��ֽ���
                        
                nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                    
                //MYLOG_INFO("Uart extern recv %d byte:", nbyte);
                //MYLOG_BYTE(msgbuf+bitnum, nbyte);
                bitnum = bitnum + nbyte;
                
                while(needbyte > nbyte)//ѭ����ȡֱ��ȫ����ȡ
                {
                    needbyte = needbyte - nbyte;
                    nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                    //MYLOG_INFO("Uart extern recv %d byte:", nbyte);
                    //MYLOG_BYTE(msgbuf+bitnum, nbyte); 
                    bitnum = bitnum + nbyte;
                }
                //MYLOG_DEBUG("The complete msg is:");
                //MYLOG_BYTE(msgbuf+i, zmsg.msglength + 4);
            }
			zmsg.check = msgbuf[i + zmsg.msglength + 2];
			sum = 0;

			//ZGB��Ϣ��checkУ��
			for (j = 0; j < zmsg.msglength; j++)
			{
				sum += msgbuf[i + 2 + j];//checkȡֵΪPayload���ֵ������ֽ�
			}
			
			//���checkֵ���ԣ���Ҫ�������Ĵ�i-1����
			if (sum%256 != zmsg.check)
			{
                MYLOG_DEBUG("The zmsg.check is %d", zmsg.check);
                MYLOG_DEBUG("The sum is %d,the sum/256 = %d", sum, sum%256);
				MYLOG_ERROR("Wrong format!");
				
				i++;
				continue;
			}

			memcpy((void*)zmsg.payload.src, (void*)msgbuf + i + 10, ZGB_ADDRESS_LENGTH); //ZGB��Ϣ��srcλ�ڱ��ĵ�10�ֽ�
    		if(addresszero(zmsg.payload.src) == 0)//����ROT�����ı���
    		{
                //�Ƴ�ǰһ������
                if((i + zmsg.msglength + 4)!= (bitnum -1))
                {
                    MYLOG_DEBUG("move a message!");
                    memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                    bitnum = bitnum - (i + zmsg.msglength + 4);
                    i = 0;
                    continue;                    
                }
            }

            zmsg.payload.cmdid[0]     = msgbuf[i + (int)&zmsg.payload.cmdid - (int)&zmsg];
            zmsg.payload.cmdid[1]     = msgbuf[i + 1 + (int)&zmsg.payload.cmdid - (int)&zmsg];            

            zmsg.payload.adf.index[0] = msgbuf[i + (int)&zmsg.payload.adf.index - (int)&zmsg];
            zmsg.payload.adf.index[1] = msgbuf[i + 1 + (int)&zmsg.payload.adf.index - (int)&zmsg]; 

            if(zmsg.payload.cmdid[0] != 0x20 && zmsg.payload.cmdid[1] == 0x98)//����ROT������͸������
            {
                //�Ƴ�ǰһ������
                if((i + zmsg.msglength + 4)!= (bitnum -1))
                {
                    MYLOG_DEBUG("move a message!");
                    memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                    bitnum = bitnum - (i + zmsg.msglength + 4);
                    i = 0;
                    continue;                    
                }            
            }

            zmsg.payload.adf.sub    = msgbuf[i + (int)&zmsg.payload.adf.sub - (int)&zmsg];
            zmsg.payload.adf.opt    = msgbuf[i + (int)&zmsg.payload.adf.opt - (int)&zmsg];
            zmsg.payload.adf.length = msgbuf[i + (int)&zmsg.payload.adf.length - (int)&zmsg];
            
            if(zmsg.payload.adf.index[0] == 0xA0 && zmsg.payload.adf.index[1] == 0x0F)
            {
                zmsg.payload.adf.data.magicnum    = msgbuf[i + (int)&zmsg.payload.adf.data.magicnum - (int)&zmsg];
                zmsg.payload.adf.data.length      = msgbuf[i + (int)&zmsg.payload.adf.data.length - (int)&zmsg];
                zmsg.payload.adf.data.version     = msgbuf[i + (int)&zmsg.payload.adf.data.version - (int)&zmsg];
                zmsg.payload.adf.data.devicetype  = msgbuf[i + (int)&zmsg.payload.adf.data.devicetype - (int)&zmsg];
                zmsg.payload.adf.data.deviceindex = msgbuf[i + (int)&zmsg.payload.adf.data.deviceindex - (int)&zmsg];
    			zmsg.payload.adf.data.packetid    = msgbuf[i + (int)&zmsg.payload.adf.data.packetid - (int)&zmsg];
    			zmsg.payload.adf.data.msgtype     = msgbuf[i + (int)&zmsg.payload.adf.data.msgtype - (int)&zmsg];
                
    			memcpy((void*)zmsg.payload.adf.data.pdu, (void*)(msgbuf + i + 44), zmsg.payload.adf.data.length);                
            }

            zgbqmsg.msgtype = QUEUE_MSG_ZGB;
            memcpy((void*)&zgbqmsg.msg, (void*)&zmsg, sizeof(zgbmsg));

          	if (ret = msgsnd(g_queueid, &zgbqmsg, sizeof(zgbqueuemsg), 0) != 0)
        	{
		        MYLOG_ERROR("send zgbqueuemsg fail!");
	        }
            //�Ƴ�ǰһ������
            if((i + zmsg.msglength + 4)!= bitnum)
            {
                MYLOG_DEBUG("move a message!");
                memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                bitnum = bitnum - (i + zmsg.msglength + 4);
                i = 0;                   
            } 
            else
            {
                i = bitnum;
            }
		}
	}
	pthread_exit(NULL);
}
