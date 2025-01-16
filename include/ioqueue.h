#ifndef DEVICE__IOQUEUE_H
#define DEVICE__IOQUEUE_H

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64      //���û�������СΪ64�ֽ�

//����ṹ�廷�λ�����
struct ioqueue
{
    struct lock lock;
    struct task_struct* consumer;
    struct task_struct* producer;
    char buf[bufsize];
    uint32_t head;			//ͷ����������
    uint32_t tail;			//β��������
};

void init_ioqueue(struct ioqueue* ioq);
uint32_t next_pos(uint32_t pos);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
void ioq_wait(struct task_struct** waiter); //������waiter�Ķ���ָ�� ȡ����ָ���ԭ�����������Զ�ָ��ĵ�ֵַ�����޸�
void wakeup(struct task_struct** waiter); 
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq,char chr);

#endif
