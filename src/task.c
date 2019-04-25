#include "app.h"
#include "mqtt.h"
#include "test/test.h"

/*全局变量*/
int g_uartfd;
int g_log_level = 2;
int g_operationflag = 0;
int g_system_mode = 1;
int g_queueid;
sqlite3* g_db;
char g_mac[20] = {0};
char g_boilerid[20] = {0};
char g_topicroot[20] = {0};
char g_topics[TOPICSNUM][50] ={{0},{0},{0}};
timer* g_zgbtimer;
cJSON* g_devices_status_json;
ZGB_MSG_STATUS g_devicemsgstatus[ZGBMSG_MAX_NUM];
pthread_mutex_t g_devices_status_mutex = PTHREAD_MUTEX_INITIALIZER;


//程序启动后申请堆存放需要订阅的topic
char g_topicthemes[TOPICSNUM][20] = {{"devices/operation"},{"devices/electric"},{"gateway"}};
char g_clientid[30], g_clientid_pub[30];


/*创建需要的数据库表*/
int sqlitedb_init()
{
    size_t rc; 
    char sql[512];
    char db_zgbaddress[17] = "0000000000000000";
    char db_deviceid[20] = "00000000000000000";
    size_t nrow = 0, ncolumn = 0;
	char **dbresult;     
    char* zErrMsg = NULL;

    if(is_file_exist("/usr/smarthome.db") == -1)
    {
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

        sprintf(sql,"CREATE TABLE [wateryield_day]([deviceid] TEXT NOT NULL,[wateryield] INT NOT NULL,[day] INT NOT NULL, primary key(deviceid, day));");
        exec_sql_create(sql);

        sprintf(sql,"CREATE TABLE [wateryield_month]([deviceid] TEXT NOT NULL,[wateryield] INT NOT NULL,[month] INT NOT NULL, primary key(deviceid, month));");
        exec_sql_create(sql);

        sprintf(sql,"CREATE TABLE [wateryield_year]([deviceid] TEXT NOT NULL,[wateryield] INT NOT NULL,[year] INT NOT NULL, primary key(deviceid, year));");
        exec_sql_create(sql);

        sprintf(sql,"CREATE TABLE [wateryield_hour]([deviceid] TEXT NOT NULL,[wateryield] INT NOT NULL,[hour] INT NOT NULL, primary key(deviceid, hour));");
        exec_sql_create(sql);

        sprintf(sql,"CREATE TABLE gatewaycfg (mode INTERGER NOT NULL DEFAULT 0, boilerid TEXT DEFAULT \"\");");
        exec_sql_create(sql);        

        //把网关设备写入devices表
        sprintf(sql, "replace into devices values('%s', '%s', %d, %d, 1);", db_deviceid, db_zgbaddress, DEV_GATEWAY, 0);
        exec_sql_create(sql);

        sprintf(sql, "replace into gatewaycfg values(2, \"\");");
        exec_sql_create(sql);          
    }
    else
    {
        rc = sqlite3_open("/usr/smarthome.db", &g_db);
        if(rc != SQLITE_OK)
        {
            MYLOG_ERROR("open smarthome.db error!");
            return -1;
        }        
    }


    sprintf(sql,"select mode from gatewaycfg;");
    sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);

    if(nrow == 0)
    {
        set_gateway_mode(TLV_VALUE_COND_COLD);
    }    
    return 0;
}


/*创建程序需要用到的消息队列，用于MQTT的订阅、发布、去订阅和存取设备操作消息以及zgb消息的存储*/
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

void zgbledtimerfun(timer* t)
{
    ledcontrol(ZGB_LED, LED_ACTION_ON, 0);
    deltimer(t);
    g_zgbtimer = NULL;
}

/*局域网监听任务*/
void* lantask(void *argc)
{
    struct sockaddr_in servaddr,appaddr;
    char text[] = "smarthome app";
    int listenfd,sendfd;
    char buf[128];               

    MYLOG_INFO("Thread lantask begin!");                         
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

    bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));  //端口绑定
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
    char sql[200];  
    int rc; 
    char *zErrMsg = NULL;

    //获取网关mac地址
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
    ledcontrol(ZGB_LED, LED_ACTION_ON, 0);//点亮ZGB LED灯
    
    if(createmessagequeue() == -1)
    {
        MYLOG_ERROR("CreateMessageQueue failed!");//这里后续要添加重启的功能
    } 

    if(sqlitedb_init() == -1)
    {
        MYLOG_ERROR("Create db failed!");      
    }

	for(; i < TOPICSNUM; i++)
	{
		sprintf(g_topics[i], "%s%s\0", g_topicroot, g_topicthemes[i]);
	}
    devices_status_json_init();
    gatewayregister();
    ledcontrol(SYS_LED, LED_ACTION_ON, 0);//点亮SYS LED灯
}



/*APP下发的设备消息的处理进程*/
void* devicemsgprocess(void *argc)
{
    MYLOG_INFO("Thread devicemsgprocess begin!");
	devicequeuemsg msg; //消息队列中取出的设备操作消息
	char topic[TOPIC_LENGTH] = { 0 };//对操作的响应topic
	int mqttid = 0;
	cJSON *devices, *device, *operations, *operation, *tmp, *device_mqtt_json, *result_json;
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
    cJSON* devicestatus;

	while(1)
	{
		if (rcvret = msgrcv(g_queueid, (void*)&msg, msglen, QUEUE_MSG_DEVIC, 0) <= 0)
		{
            MYLOG_ERROR("recive devicequeuemsg fail!rcvret = %d", rcvret);
            continue;
		}
        
		MYLOG_INFO("devicemsgprocess recive a msg");
		MYLOG_INFO("the msg is %s", cJSON_PrintUnformatted(msg.p_operation_json));
        
		device_mqtt_json = msg.p_operation_json;

        tmp = cJSON_GetObjectItem(device_mqtt_json, "mqttid");
        if (tmp != NULL)
        {
        	mqttid = tmp->valueint;
     		sprintf(topic, "%s%s/response/%d", g_topicroot, g_topicthemes[0], mqttid);
     
        }else{
		    cJSON_Delete(device_mqtt_json);
		    g_operationflag = 0;
            continue;
        }
        
		result_json = cJSON_CreateObject();
		cJSON_AddNumberToObject(result_json, "mqttid", mqttid);
		cJSON_AddNumberToObject(result_json, "resultcode", MQTT_MSG_ERRORCODE_SUCCESS);
		
        tmp = cJSON_GetObjectItem(device_mqtt_json, "operation");
        if (tmp == NULL)
        {
            MYLOG_ERROR("Wrong MQTT msg, NO 'operation:'!");
            cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            goto response;
        }          
        operationtype = tmp->valueint;//操作类型

        //操作的coo模块
        if(operationtype == 0)
        {
            int actiontype;
            tmp = cJSON_GetObjectItem(device_mqtt_json, "cmdid");
            if (tmp == NULL)
            {
                cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                goto response;
            }             
            actiontype = tmp->valueint;//coo操作值
                     
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
                    MYLOG_INFO("COO operation AT+PERMITJOIN=78");
                    if(g_zgbtimer == NULL)
                    {
                        ledcontrol(ZGB_LED, LED_ACTION_TRIGGER, 1000);
                        timer* zgbledtimer = createtimer(120, zgbledtimerfun);
                        addtimer(zgbledtimer);
                        g_zgbtimer = zgbledtimer;
                    }
                    else
                    {
                        rebuildtimer(g_zgbtimer); //zigbee灯闪烁时间重新计时
                    }
                    reportdevices();
                    break;
                case TYPE_DEVICE_LIST:
                    write(g_uartfd, AT_DEVICE_LIST, strlen(AT_DEVICE_LIST));
                    MYLOG_INFO("COO operation AT+LIST");                    
                    break;   
                case TYPE_NETWORK_NOCLOSE:
                    write(g_uartfd, AT_NETWORK_NOCLOSE, strlen(AT_NETWORK_NOCLOSE));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=FF");
                    if(g_zgbtimer == NULL)
                    {
                        ledcontrol(ZGB_LED, LED_ACTION_TRIGGER, 1000);    
                    }
                    else
                    {
                        deltimer(g_zgbtimer);
                        g_zgbtimer = NULL;
                    }
                    break;
                case TYPE_CLOSE_NETWORK:
                    write(g_uartfd, AT_CLOSE_NETWORK, strlen(AT_CLOSE_NETWORK));
                    MYLOG_INFO("COO operation AT+PERMITJOIN=00");
                    ledcontrol(ZGB_LED, LED_ACTION_ON, 0);
                    break;
                case TYPE_NETWORK_INFO:
                    write(g_uartfd, AT_NETWORK_INFO, strlen(AT_NETWORK_INFO));
                    MYLOG_INFO("COO operation AT+GETINFO");
                    break;   
                default:
                    MYLOG_ERROR("Unknow actiontype!");
                    cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            }
            goto response;
        }
        
		devices = cJSON_GetObjectItem(device_mqtt_json, "devices");
		if (devices == NULL)
		{
            cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
            goto response;
		} 
        
		devicenum = cJSON_GetArraySize(devices);//一个消息中操作的设备数量

        //设备操作
        if (operationtype == 1)
        {   
            cJSON* devs = cJSON_CreateArray();
            cJSON* dev;
    		for (i=0; i< devicenum; i++)
    		{
    		    dev = cJSON_CreateObject();
    			device = cJSON_GetArrayItem(devices, i);
                /***
                **全局的设备操作数组中添加一个新的未处理的
                **设备操作动作
                ***/
    			packetid = getpacketid(); //packetid是用来跟zgb设备通信使用的
    			g_devicemsgstatus[i].packetid = packetid;
    			g_devicemsgstatus[i].result = MQTT_MSG_ERRORCODE_OPERATIONFAIL; //1代表失败，0代表成功，2代表设备未响应
    			g_devicemsgstatus[i].finish = 1; //1代表未处理完
                
                tmp = cJSON_GetObjectItem(device, "deviceid");
                if(tmp == NULL)
    		    {
                    cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                    goto response;
    			}                
                
    			deviceid = tmp->valuestring;
                cJSON_AddItemToObject(dev, "deviceid", cJSON_CreateString(deviceid));

                int nrow = 0, ncolumn = 0;
    	        char **dbresult; 
                
                sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", deviceid);
                MYLOG_INFO(sql);
                
                sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
                if(nrow == 0) //数据库中没有该设备
                {
                    MYLOG_ERROR("MQTT:The device is not exist! id:%s", deviceid);
                    g_devicemsgstatus[i].result = MQTT_MSG_ERRORCODE_DEVICENOEXIST;
                    g_devicemsgstatus[i].finish = 0;
                    cJSON_AddItemToArray(devs, dev);
                    sqlite3_free_table(dbresult);        			    
    		        continue;                    
                }
                
                strncpy(db_zgbaddress, dbresult[ncolumn+1], 20);
                dbaddresstozgbaddress(db_zgbaddress, src);
                devicetype  = atoi(dbresult[ncolumn+2]);
                deviceindex = atoi(dbresult[ncolumn+3]) ;
                operations  = cJSON_GetObjectItem(device, "operations");
    		    if (operations == NULL)
    		    {
                    g_devicemsgstatus[i].result = MQTT_MSG_ERRORCODE_FORMATERROR;
                    g_devicemsgstatus[i].finish = 0;
                    cJSON_AddItemToArray(devs, dev);
                    sqlite3_free_table(dbresult);        			    
    		        continue;
    		    }
    		    cJSON_AddItemToObject(dev, "operations", cJSON_Duplicate(operations, 1));
    		    cJSON_AddItemToObject(dev, "devicetype", cJSON_CreateNumber(devicetype));

                //操作的设备是网关本身
                if (devicetype == DEV_GATEWAY)
                {
                    int ret = gatewayproc(operations);
                    g_devicemsgstatus[i].result = ret;
                    g_devicemsgstatus[i].finish = 0; //0代表已处理完
                    cJSON_AddItemToArray(devs, dev);
                    sqlite3_free_table(dbresult);
                    continue;
                }

                //检查设备是否在线
                int online = check_device_online(deviceid);
                if(online == 0)
                {
                    g_devicemsgstatus[i].result = MQTT_MSG_ERRORCODE_DEVICEOFFLINE;
                    g_devicemsgstatus[i].finish = 0;
                    cJSON_AddItemToArray(devs, dev);
                    sqlite3_free_table(dbresult);        			    
    		        continue;                    
                }    		    
    		  
    		    BYTE data[125];
                int datalen = mqtttozgb(operations, data, devicetype);
                sendzgbmsg(src, data, datalen, ZGB_MSGTYPE_DEVICE_OPERATION, devicetype, deviceindex, packetid);
                cJSON_AddItemToArray(devs, dev);
                sqlite3_free_table(dbresult);             
    		}

    		for(i=0; i<RESPONSE_WAIT/50; i++)  //50毫秒循环一次
    		{
    			int j = 0;
    			for (; j < devicenum; j++)
    			{
    				if (g_devicemsgstatus[j].finish == 1)//有未处理完的设备直接跳出
    					break;
    			}
    			if (j == devicenum)//所有的zgb消息都已处理完
    			{
    				break;
    			}
    			milliseconds_sleep(50);
    		}

    		for (i=0; i<devicenum; i++)
    		{
    			device = cJSON_GetArrayItem(devs, i);
    			cJSON_AddNumberToObject(device, "result", g_devicemsgstatus[i].result);    			
    		}
    		cJSON_AddItemToObject(result_json, "devices", devs);
        }
        
        //设备状态查询
        if (operationtype == 2)
        {
            cJSON* statusarray = cJSON_CreateArray();
    		for (i=0; i< devicenum; i++)
    		{
    			device = cJSON_GetArrayItem(devices, i); 
                tmp = cJSON_GetObjectItem(device, "deviceid");
                MYLOG_DEBUG("The device is %s", cJSON_PrintUnformatted(device));
                if(tmp == NULL)
    		    {
                    cJSON_ReplaceItemInObject(result_json, "resultcode", cJSON_CreateNumber(MQTT_MSG_ERRORCODE_FORMATERROR));
                    goto response;
    			}                
                
    			deviceid = tmp->valuestring;

                devicestatus = dup_device_status_json(deviceid);
                if (devicestatus == NULL)
                {
    			    cJSON* status = cJSON_CreateObject();
                    cJSON_AddItemToObject(status, "deviceid", cJSON_CreateString(deviceid));
                    cJSON_AddItemToObject(status, "status", cJSON_CreateArray());
                    cJSON_AddItemToArray(statusarray, status);
                    MYLOG_ERROR("Can't find the device status!,id:%s", deviceid);
                }
                else
                {
                    cJSON_AddItemToArray(statusarray, devicestatus);
                    MYLOG_DEBUG("MQTT: Device status query,The status is %s", cJSON_PrintUnformatted(devicestatus));                   
                }
            }
            cJSON_AddItemToObject(result_json, "devices", statusarray);
            
        }
        
response:
    	sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(result_json), QOS_LEVEL_2, 0);  
		cJSON_Delete(device_mqtt_json);
		cJSON_Delete(result_json);
		g_operationflag = 0;
	}
	pthread_exit(NULL);
}

/*ZGB消息发送进程*/
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
	    if ( write(g_uartfd, (char *)&qmsg.msg+sizeof(qmsg.msg)-2, 2) != 2)//发送check和footer
	    {
		    MYLOG_ERROR("com write check and footer error!");
	    }
    }
	pthread_exit(NULL);    
}

/*zgb消息处理*/
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

        memcpy(src, qmsg.msg.payload.src, 8);
        zgbaddresstodbaddress(src, db_zgbaddress);

        if(qmsg.msg.payload.adf.index[0] == 0x00 && qmsg.msg.payload.adf.index[1] == 0x00) //设备入网消息
        {
            int nrow = 0, ncolumn = 0;
	        char **dbresult;
	                
            MYLOG_INFO("[ZGB DEVICE]Get a device network joining message.");
		    MYLOG_ZGBMSG(qmsg.msg);            
            sprintf(sql,"SELECT * FROM devices WHERE zgbaddress = '%s';", db_zgbaddress);
            MYLOG_INFO(sql);
            
            sqlite3_get_table(g_db, sql, &dbresult, &nrow, &ncolumn, &zErrMsg);
            //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
            if(nrow == 0) //数据库中没有该设备
            {
                milliseconds_sleep(1000);
                sendzgbmsg(src, NULL, 0, ZGB_MSGTYPE_DEVICEREGISTER, DEV_ANYONE, 0, getpacketid());//要求设备注册
            }
            sqlite3_free_table(dbresult);
            continue;
        }
        else if(qmsg.msg.payload.adf.index[0] == 0xF2 && qmsg.msg.payload.adf.index[1] == 0x03) //设备离网消息
        {   
            MYLOG_INFO("[ZGB DEVICE]Get a device network leaving message.");
            MYLOG_ZGBMSG(qmsg.msg);
            sprintf(sql,"DELETE FROM devices WHERE zgbaddress = '%s';", db_zgbaddress);            
            exec_sql_create(sql);

            int devicenum;
            cJSON* devicestatus = NULL;
            char* array_deviceid;
            cJSON* device_array = cJSON_CreateArray();
            cJSON* id;

            pthread_mutex_lock(&g_devices_status_mutex);
            devicenum = cJSON_GetArraySize(g_devices_status_json);

            //删除内存设备状态表中相关的设备
            for (int i=0; i < devicenum;)
            {
                devicestatus = cJSON_GetArrayItem(g_devices_status_json, i);
                if(devicestatus == NULL)
                {
                    break;
                }
                cJSON* deviceid = cJSON_GetObjectItem(devicestatus, "deviceid");
                if(deviceid == NULL)
                {
                    continue;
                }
                array_deviceid = deviceid->valuestring;
                if(strncmp(db_zgbaddress, array_deviceid, 16) == 0)//同一个ZGB设备上的设备
                {
                    id = cJSON_CreateString(array_deviceid);
                    cJSON_AddItemToArray(device_array, id);
                    cJSON_DeleteItemFromArray(g_devices_status_json, i);
                    devicenum--;
                }
                else
                {
                     i++;
                }
            }
            if(cJSON_GetArraySize(device_array)>0)
            {
                sendmqttmsg(MQTT_MSG_TYPE_PUB, TOPIC_DEVICE_DELETE, cJSON_PrintUnformatted(device_array), 0, 0);
            }
            pthread_mutex_unlock(&g_devices_status_mutex);
            continue;
        }


        //如果是广播报文，直接丢弃
        ZGBADDRESS broadcastaddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if(memcmp(src, broadcastaddr, 8) == 0)
        {
            MYLOG_INFO("Drop a broadcast msg!");
            goto end;
        }

        if(qmsg.msg.payload.adf.index[0] != 0xA0 || qmsg.msg.payload.adf.index[1] != 0x0F)
        {
            MYLOG_INFO("Drop a not own msg!");
            goto end;            
        }

        zgbdata      = &qmsg.msg.payload.adf.data;
        msgtype      = zgbdata->msgtype;
        devicetype   = zgbdata->devicetype;
        deviceindex  = zgbdata->deviceindex;
        packetid     = zgbdata->packetid;
        
        sprintf(db_deviceid, "%s%d", db_zgbaddress, deviceindex);        
        
        switch (msgtype)
        {
            case ZGB_MSGTYPE_DEVICEREGISTER_RESPONSE:
            {
                 cJSON* root = cJSON_CreateObject();
                 int nrow = 0, ncolumn = 0;
                 char **azResult; 
                 
                 MYLOG_INFO("[ZGB DEVICE]Get a device network joining response message.");
                 MYLOG_ZGBMSG(qmsg.msg);
                 sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", db_deviceid);
                 MYLOG_INFO(sql);
            
                 sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
                 //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
                 if(nrow != 0) //数据库中有该设备
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
                 sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(root), QOS_LEVEL_2, 0);//mqtt发布设备注册信息
                 cJSON_Delete(root);
                 break;
            }
            case ZGB_MSGTYPE_DEVICE_OPERATION_RESULT:
            {
                MYLOG_INFO("[ZGB DEVICE]Get a device operation response message.");
                MYLOG_ZGBMSG(qmsg.msg);            
                for (int i = 0; i < ZGBMSG_MAX_NUM; ++i)
                {
                    if (g_devicemsgstatus[i].packetid == packetid)
                    {
                        g_devicemsgstatus[i].finish = 0;
                        g_devicemsgstatus[i].result = zgbdata->pdu[4];
                        break;
                    }
                }                              
                break;
            } 
            case ZGB_MSGTYPE_DEVICE_STATUS_REPORT://设备状态上报
            {
                MYLOG_INFO("[ZGB DEVICE]Get a device status message.");
                MYLOG_ZGBMSG(qmsg.msg);             
                cJSON *device_json;
                cJSON *attr_json;
                cJSON *replace_value_json;
                cJSON *temp;
                int value,oldvalue;
                int i = 0;
                BYTE attr; 

                needmqtt = false;//是否需要上报状态的标志
                change_device_online(db_deviceid, 1);//收到状态查询报文，改变设备上线状态
                device_json = dup_device_status_json(db_deviceid);
                if(device_json == NULL)
                {
                    goto end;
                }
               
                while(i < (zgbdata->length - 7))
                {
                    attr = zgbdata->pdu[i++];
                    
                    attr_json = get_attr_value_object_json(device_json, attr);
                    
                    if(attr_json == NULL)
                    {
                        goto end;
                    }
                    value = zgbdata->pdu[i++]*256*256*256 + zgbdata->pdu[i++]*256*256 
                        + zgbdata->pdu[i++]*256 + zgbdata->pdu[i++];
                replace_value_json = cJSON_CreateNumber(value);

                switch(attr)
                {
                case ATTR_DEVICESTATUS:
                case ATTR_AIR_CONDITION_MODE:
                case ATTR_WINDSPEED:
                case ATTR_ENV_TEMPERATURE:
                case ATTR_ENV_HUMIDITY:
                case ATTR_ENV_PM25:
                case ATTR_ENV_CO2:
                case ATTR_ENV_FORMALDEHYDE:
                case ATTR_SOCKET_V:
                case ATTR_SETTING_TEMPERATURE:
                case ATTR_SETTING_HUMIDITY:
                case ATTR_SYSMODE:
                {
                    temp = cJSON_GetObjectItem(attr_json, "value");
                    if(temp == NULL)
                    {
                        if(device_json != NULL)
                        {
                            cJSON_Delete(device_json);
                        }
                        goto end;
                    }
                    oldvalue = temp->valueint;
                    if(value == oldvalue)
                    {
                        needmqtt = false || needmqtt;
                    }
                    else
                    {
                        //修改的是复制出来的设备属性表
                        cJSON_ReplaceItemInObject(attr_json, "value", replace_value_json);
                        //修改的是全局的设备属性表
                        change_device_attr_value(db_deviceid, attr, value);
                        needmqtt = true || needmqtt;
                        switch (attr)
                        {
                        case ATTR_SYSMODE:
                            if(value != g_system_mode)
                                change_system_mode(value);
                            break;
                        default:
                            break;
                        }
                    }
                    break;
                }
                case ATTR_SEN_WATER_YIELD:
                {
                    MYLOG_ERROR("Get a water yield report msg, the deviceid is %s", db_deviceid);
                    needmqtt = false || needmqtt;
                    change_device_attr_value(db_deviceid, attr, value);                 
                    wateryield_stat(db_deviceid, value);
                    break;
                }                
                case ATTR_SOCKET_E:
                {
                    MYLOG_ERROR("Get a socket electricity report msg, the deviceid is %s", db_deviceid);
                    needmqtt = false || needmqtt;
                    change_device_attr_value(db_deviceid, attr, value);                 
                    electricity_stat(db_deviceid, value);
                    break;
                }
                default:
                    break;
                }
            }
            if(needmqtt)//是否需要发送mqtt消息
            {
                sprintf(topic, "%s%s", g_topicroot, TOPIC_DEVICE_STATUS);
                //如果是设备状态关闭，则不上报其他属性
                attr_json = get_attr_value_object_json(device_json, ATTR_DEVICESTATUS);
                if(attr_json != NULL)
                {
                    int value = cJSON_GetObjectItem(attr_json, "value")->valueint;
                    if(value == TLV_VALUE_POWER_OFF)
                    {
                        cJSON* status = cJSON_GetObjectItem(device_json, "status");
                        int sum = cJSON_GetArraySize(status);
                        cJSON* tmp;
                        for(int i = 0; i < sum; )
                        {
                            tmp = cJSON_GetArrayItem(status, i);
                            attr = cJSON_GetObjectItem(tmp, "type")->valueint;
                            //如果不是类型和状态属性全部删除
                            if(attr != ATTR_DEVICESTATUS&& attr != ATTR_DEVICETYPE)
                            {
                                cJSON_DeleteItemFromArray(status, i);
                                sum--;
                            }
                            else
                            {
                                i++;
                            }
                        }
                    }

                 }
                    sendmqttmsg(MQTT_MSG_TYPE_PUB, topic, cJSON_PrintUnformatted(device_json), 0, 0);
              }
                cJSON_Delete(device_json);
                break;
            }
            case ZGB_MSGTYPE_DEVICE_LOCATION:
            {
                MYLOG_INFO("[ZGB DEVICE]Get a device location message.");
                MYLOG_ZGBMSG(qmsg.msg);             
                cJSON* root = cJSON_CreateObject();
                int nrow = 0, ncolumn = 0;
                char **azResult; 
                 
                MYLOG_INFO("[ZGB DEVICE]Get a device location message.");
                 
                sprintf(sql,"SELECT * FROM devices WHERE deviceid = '%s';", db_deviceid);
                            
                sqlite3_get_table(g_db, sql, &azResult, &nrow, &ncolumn, &zErrMsg);
                //MYLOG_DEBUG("The nrow is %d, the ncolumn is %d, the zErrMsg is %s", nrow, ncolumn, zErrMsg);
                if(nrow == 0) //数据库中没有该设备
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


/*监听串口的进程，提取单片机上传的ZGB消息*/
void* uartlisten(void *argc)
{
	BYTE msgbuf[1024]; 
	pthread_t threads[2];
	int nbyte=0, bitnum=0;
	zgbmsg zmsg;
    zgbqueuemsg zgbqmsg;
	int i, j, sum;
    int ret;

    MYLOG_INFO("Thread uartlisten begin!");
    pthread_create(&threads[0], NULL, uartsend, NULL);
    pthread_create(&threads[1], NULL, zgbmsgprocess, NULL);

    MYLOG_DEBUG("pthread uartlisten begin");
    
	while(1)
	{
		nbyte = read(g_uartfd, msgbuf, 110);
        bitnum = nbyte;
		for(i = 0; i < bitnum; )
		{
			if(msgbuf[i] != 0x2A)//查找ZGB自定义报文开始字节
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
            
			/*开始提取ZGB消息*/
			zgbmsginit(&zmsg);
			zmsg.msglength = msgbuf[i + 1];
            if(zmsg.msglength+4 > bitnum-i) //如果ZGB消息长度大于接受到字节剩余长度说明还要接受后续字节
            {
                int needbyte = (zmsg.msglength + 4) -  (bitnum - i);//构造当前报文还需接收的字节数
                        
                nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                bitnum = bitnum + nbyte;
                while(needbyte > nbyte)//循环读取直到全部读取
                {
                    needbyte = needbyte - nbyte;
                    nbyte = read(g_uartfd, msgbuf+bitnum, needbyte);
                    bitnum = bitnum + nbyte;
                }
            }
			zmsg.check = msgbuf[i + zmsg.msglength + 2];
			sum = 0;

			//ZGB消息的check校验
			for (j = 0; j < zmsg.msglength; j++)
			{
				sum += msgbuf[i + 2 + j];//check取值为Payload相加值的最低字节
			}
			
			//如果check值不对，需要整个报文从i-1分析
			if (sum%256 != zmsg.check)
			{
                MYLOG_DEBUG("The zmsg.check is %d", zmsg.check);
                MYLOG_DEBUG("The sum is %d,the sum/256 = %d", sum, sum%256);
				MYLOG_ERROR("Wrong format!");
				
				i++;
				continue;
			}

			memcpy((void*)zmsg.payload.src, (void*)msgbuf + i + 10, ZGB_ADDRESS_LENGTH); //ZGB消息中src位于报文第10字节
    		if(addresszero(zmsg.payload.src) == 0)//不是ROT发来的报文直接丢弃
    		{
                //移除前一个报文
                if((i + zmsg.msglength + 4)!= (bitnum -1))
                {
                    MYLOG_DEBUG("move a message!");
                    memmove(msgbuf, msgbuf + i + zmsg.msglength + 4, bitnum - (i + zmsg.msglength + 4));
                    bitnum = bitnum - (i + zmsg.msglength + 4);
                    i = 0;
                    continue;                    
                }
                else
                {
                    break;   
                }     
            }

            zmsg.payload.cmdid[0]     = msgbuf[i + (int)&zmsg.payload.cmdid - (int)&zmsg];
            zmsg.payload.cmdid[1]     = msgbuf[i + 1 + (int)&zmsg.payload.cmdid - (int)&zmsg];            
            zmsg.payload.adf.index[0] = msgbuf[i + (int)&zmsg.payload.adf.index - (int)&zmsg];
            zmsg.payload.adf.index[1] = msgbuf[i + 1 + (int)&zmsg.payload.adf.index - (int)&zmsg]; 
            zmsg.payload.adf.sub      = msgbuf[i + (int)&zmsg.payload.adf.sub - (int)&zmsg];
            zmsg.payload.adf.opt      = msgbuf[i + (int)&zmsg.payload.adf.opt - (int)&zmsg];
            zmsg.payload.adf.length   = msgbuf[i + (int)&zmsg.payload.adf.length - (int)&zmsg];

            memcpy((void*)&zmsg.payload.adf.data, (void*)(msgbuf + i + 37), zmsg.payload.adf.length);

            zgbqmsg.msgtype = QUEUE_MSG_ZGB;
            memcpy((void*)&zgbqmsg.msg, (void*)&zmsg, sizeof(zgbmsg));

          	if (ret = msgsnd(g_queueid, &zgbqmsg, sizeof(zgbqueuemsg), 0) != 0)
        	{
		        MYLOG_ERROR("send zgbqueuemsg fail!");
	        }
            //移除前一个报文
            if((i + zmsg.msglength + 4)!= bitnum)
            {
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
