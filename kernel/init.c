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



/*负责初始化所有模块*/
void init_all(void){
    put_str("init_all\n");
    idt_init();     //初始化中断
    timer_init();   //初始化PIT
    mem_init();     //初始化内存管理
    thread_init();  //初始化线程管理
    console_init(); //初始化终端
    keyboard_init();    //初始化键盘
    tss_init();     //初始化tss结构
    syscall_init(); //初始化系统调用
    ide_init();     //初始化硬盘驱动
    filesys_init();//在各个分区初始化文件系统
}