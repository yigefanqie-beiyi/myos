#ifndef THREAD_SYNC_H
#define THREAD_SYNC_H
#include "list.h"
#include "stdint.h"

struct task_struct;  // ǰ��������������� thread.h

/*�ź����ṹ*/
struct semaphore{
    uint8_t value;  //�ź���ֵ
    struct list waiters;    //���ڸ��ź���ֵ�ϵȴ������������߳�
};

/*���ṹ*/
struct lock{
    struct task_struct* holder; //���ĳ�����
    struct semaphore semaphore; //�����ź���
    uint32_t holder_repeat_nr;  //���ĳ������ظ��������Ĵ���
};

/*��ʼ���ź���*/
void sema_init(struct semaphore* psema, uint8_t value);
/*��ʼ����*/
void lock_init(struct lock* plock);
/*�ź���down����*/
void sema_down(struct semaphore* psema);
//�ź���value����,�����̣߳��������¼���׼��������
void sema_up(struct semaphore* psema);
//��ȡ����Դ
void lock_acquire(struct lock* plock);
//�ͷ�����Դ
void lock_release(struct lock* plock);

#endif