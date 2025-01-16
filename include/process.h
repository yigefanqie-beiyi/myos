#ifndef USER_PROCESS_H
#define USER_PROCESS_H
#include "thread.h"

#define USER_VADDR_START 0x8048000

/*构建用户进程的上下文信息*/
void start_process(void* filename_);
/*激活进程或者线程的页表*/
void page_dir_activate(struct task_struct* p_thread);
/*激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈*/
void process_activate(struct task_struct* p_thread);
/*创建页目录表，成功则返回页目录的虚拟地址，将当前页表的表示内核空间的pde复制，使用户进程目录表中的第768-1023个页目录项用内核所在页目录的第768-1023个页目录项代替*/
uint32_t* create_page_dir(void);
/*创建用户进程的虚拟地址位图*/
void create_user_vaddr_bitmap(struct task_struct* user_prog);
/*创建用户进程filename，并将其加入就绪队列等待执行*/
void process_execute(void* filename,char* name);

#endif