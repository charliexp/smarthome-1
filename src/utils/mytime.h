#ifndef _MY_TIME_
#define _MY_TIME_

typedef struct mytimer{
    int timevalue; //��ʱʱ��,��λ��
    int lefttime;  //��ʱ��ʣ��ʱ��
    void(*handler)(struct mytimer* t); //������
    struct mytimer* next;
}timer;

timer* createtimer(int timevlaue,void(*fun)());

void addtimer(timer* t);

int deltimer(timer* t);

int rebuildtimer(timer* t);

/*��ʱ�ص�����*/
void sigalrm_fn(int sig);

void timerinit();

void electtimerfun(timer* t);

void statustimerfun(timer* t);

#endif
