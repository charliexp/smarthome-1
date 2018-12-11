#include "app.h"
#include "paho/MQTTAsync.h"
#include "task.h"
#include "mqtt.h"

extern int g_log_level;

int main(int argc, char* argv[])
{
    int ch;
    int level;
    pthread_t threads[THREAD_NUMS];

    //更改进程工作目录
    chdir("/usr");
    /*只运行单例*/
    if (already_running(LOCKFILE))
    {
        return 0;    
    }

    //通过入参设置日志级别
    if((ch = getopt(argc, argv, "g:"))!=-1)
    {
        level = *optarg - '0';
        if(level == 1 || level == 0)
        {
            g_log_level = level;
        }
    }
    
    log_init();	
    timerinit();
    init(); //程序启动初始任务
   
	pthread_create(&threads[0], NULL, mqttclient,       NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten,       NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);
    pthread_create(&threads[4], NULL, lantask,          NULL);

	//pthread_create(&threads[5], NULL, lanmqttlient,         NULL);
    //pthread_create(&threads[6], NULL, lanmqttqueueprocess,  NULL);    

    pthread_join(threads[0],NULL);
}
