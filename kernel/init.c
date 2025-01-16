#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "console.h"
#include "thread.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"



/*�����ʼ������ģ��*/
void init_all(void){
    put_str("init_all\n");
    idt_init();     //��ʼ���ж�
    timer_init();   //��ʼ��PIT
    mem_init();     //��ʼ���ڴ����
    thread_init();  //��ʼ���̹߳���
    console_init(); //��ʼ���ն�
    keyboard_init();    //��ʼ������
    tss_init();     //��ʼ��tss�ṹ
    syscall_init(); //��ʼ��ϵͳ����
    ide_init();     //��ʼ��Ӳ������
    filesys_init();//�ڸ���������ʼ���ļ�ϵͳ
}