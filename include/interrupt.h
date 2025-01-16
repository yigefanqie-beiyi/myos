#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);

/*�����жϵ�����״̬*/
enum intr_status{
    INTR_OFF,       //�ر���0
    INTR_ON     //����1
};

/*��ȡ��ǰ�ж�״̬*/
enum intr_status intr_get_status();
/*���жϣ����ش��ж�ǰ��״̬*/
enum intr_status intr_enable();
/*�ر��жϣ����ش��ж�ǰ��״̬*/
enum intr_status intr_disable();
/*�����ж�״̬Ϊstatus*/
enum intr_status intr_set_status(enum intr_status status);
/*���жϴ�����������vector_no��Ԫ����ע�ᰲװ�жϴ������funciton*/
void register_handler(uint8_t vector_no, intr_handler function);

#endif

