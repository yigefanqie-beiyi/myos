#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include "stdint.h"
typedef void* intr_handler;
void idt_init(void);

/*定义中断的两种状态*/
enum intr_status{
    INTR_OFF,       //关闭是0
    INTR_ON     //打开是1
};

/*获取当前中断状态*/
enum intr_status intr_get_status();
/*打开中断，返回打开中断前的状态*/
enum intr_status intr_enable();
/*关闭中断，返回打开中断前的状态*/
enum intr_status intr_disable();
/*设置中断状态为status*/
enum intr_status intr_set_status(enum intr_status status);
/*在中断处理程序数组第vector_no个元素中注册安装中断处理程序funciton*/
void register_handler(uint8_t vector_no, intr_handler function);

#endif

