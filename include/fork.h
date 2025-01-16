#ifndef USERPROG__FORK_H
#define USERPROG__FORK_H

#include "stdint.h"
#include "string.h"
#include "thread.h"
//复制父进程的pcb给子进程，直接先把pcb所在页 包括内核栈 中断栈全部一起复制过来 其他的需要修改的再一项项改
int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread,struct task_struct* parent_thread);
//将父进程在内存区的数据全部复制给子进程
//buf_page是因为用户进程间无法共享内存 看不见彼此 只能通过buf_page来作为过渡
void copy_body_stack3(struct task_struct* parent_thread,struct task_struct* child_thread,void* buf_page);
//给子进程构建内核栈和修改返回值
int32_t build_child_stack(struct task_struct* child_thread);
//更新inode打开数
void updata_inode_open_cnts(struct task_struct* thread);
//汇总函数 包装 把父进程资源给子进程
int32_t copy_process(struct task_struct* child_thread,struct task_struct* parent_thread);
//禁止从内核调用 只能从用户进程调用
pid_t sys_fork(void);

#endif
