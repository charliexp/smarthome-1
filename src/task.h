#ifndef _TASK_H_
#define _TASK_H_

/*��������������*/
void* lantask(void *argc);

/*��ʱ����*/
void timefun(void);

/*ϵͳ��ʼ������*/
void init();

/*APP�·����豸��Ϣ�Ĵ������*/
void* devicemsgprocess(void *argc);

/*�������ڵĽ��̣���ȡ��Ƭ���ϴ���ZGB��Ϣ*/
void* uartlisten(void *argc);

#endif
