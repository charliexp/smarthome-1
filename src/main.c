#include "app.h"
#include "paho/MQTTAsync.h"
#include "task.h"
#include "mqtt.h"

//sem_t g_mqttconnetionsem,g_lanmqttconnetionsem;

int main(int argc, char* argv[])
{
    pthread_t threads[THREAD_NUMS];
    
    chdir("/usr");//更改进程工作目录
    /*只运行单例*/
    if (already_running(LOCKFILE))
    {
        return 0;    
    }
    
    log_init();	
    timefun();//定时去请求智能插座的电量
    
    init(); //程序启动初始任务
   
	pthread_create(&threads[0], NULL, mqttlient,        NULL);
	pthread_create(&threads[1], NULL, devicemsgprocess, NULL);
	pthread_create(&threads[2], NULL, uartlisten,       NULL);
	pthread_create(&threads[3], NULL, mqttqueueprocess, NULL);
    pthread_create(&threads[4], NULL, lantask,          NULL);

	//pthread_create(&threads[5], NULL, lanmqttlient,         NULL);
    //pthread_create(&threads[6], NULL, lanmqttqueueprocess,  NULL);    

    pthread_join(threads[0],NULL);
}
