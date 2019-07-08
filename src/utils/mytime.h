#ifndef _MY_TIME_
#define _MY_TIME_

typedef struct mytimer{
    int timevalue; //定时时间,单位秒
    int lefttime;  //定时器剩余时间
    void(*handler)(struct mytimer* t); //处理函数
    struct mytimer* next;
}timer;

timer* createtimer(int timevlaue,void(*fun)());

void addtimer(timer* t);

int deltimer(timer* t);

int restarttimer(timer* t);

/*定时回调任务*/
void sigalrm_fn(int sig);

void timerinit();

void electtimerfun(timer* t);

void envdatatimerfun(timer* t);

void statustimerfun(timer* t);

void airconditiontimerfun(timer* t);

void hotwatertimerfun(timer* t);
#endif
