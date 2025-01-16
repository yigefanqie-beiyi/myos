#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include "memory.h"
#include "stdint.h"
#include "list.h"
#include "bitmap.h"

#define MAX_FILES_OPEN_PER_PROC 8   //每个进程最多允许的文件描述符

/*自定义一个通用函数类型，将在很多线程函数中作为形参类型*/
typedef void thread_func(void*);

typedef int16_t pid_t;

/*存储进程或线程，即统称为任务的状态*/
enum task_status
{
    TASK_RUNNING, // 0
    TASK_READY,   // 1
    TASK_BLOCKED, // 2
    TASK_WAITING, // 3
    TASK_HANGING, // 4
    TASK_DIED     // 5
};

/*程序的中断栈，用于发生中断时保存程序的上下文，对应kernel.S中intr%1entry里压入栈的数据*/
struct intr_stack
{
    uint32_t vec_no; //中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    //以下由cpu从低特权级进入高特权级时压入
    uint32_t err_code;
    void (*eip) (void);        //这里声明了一个函数指针 
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

/*线程栈，保护线程环境*/
struct thread_stack
{
    uint32_t ebp;   //根据ABI规则，在c语言的函数需要调用汇编编写的函数时，要保护ebp，ebx，edi，esi，esp这五个寄存器的值
    uint32_t ebx;   //esp的值通过调用约定保证
    uint32_t edi;
    uint32_t esi;

    void (*eip) (thread_func* func,void* func_arg); //和下面的相互照应 以ret 汇编代码进入kernel_thread函数调用
    
    void (*unused_retaddr);                         //占位数 在栈顶站住了返回地址的位置 因为是汇编ret 
    thread_func* function;                          //进入kernel_thread要调用的函数地址
    void* func_arg;				      //参数指针
};

/*PCB结构体，存储进程或线程的基本信息*/
struct task_struct
{
    uint32_t* self_kstack;                          //当前线程对应的内核栈
    pid_t pid;
    enum task_status status;                        //线程状态
    char name[16];
    uint8_t priority;				      //特权级
    uint8_t ticks;              //每次在处理器上执行的时间滴答数
    uint32_t elapsed_ticks;     //此任务执行了多久，即此任务从上cpu运行后至今占用了多少cpu滴答数
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];      //文件描述符数组
    struct list_elem general_tag;       //用于线程在一般的队列中的节点
    struct list_elem all_list_tag;      //用于线程在线程队列中的节点
    uint32_t* pgdir;        //进程自己页表的虚拟地址
    struct virtual_addr userprog_vaddr;     //用户进程的虚拟地址池
    struct mem_block_desc u_block_desc[DESC_CNT];   //用户进程内存块描述符
    uint32_t cwd_inode_nr;			      //工作目录inode编号
    int16_t parent_pid;                 //父进程的pid，如果没有父进程则为-1
    uint32_t stack_magic;			      //越界检查  因为我们pcb上面的就是我们要用的栈了 到时候还要越界检查
};

/*创建线程栈，将要执行的函数和对应传参放入栈中*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
/*初始化线程的基本信息*/
void init_thread(struct task_struct* pthread, char* name, int prio);
/*创建优先级为prio的线程，线程名为name，所实行的函数为function(func_arg)*/
/*无论是用户的还是内核的线程，PCB都是在内核空间的*/
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
/*获取当前线程的PCB*/
struct task_struct* running_thread();
/*实现任务调度:将当前线程换下处理器，并在就绪队列中找到下一个可运行的线程，放上处理器*/
void schedule();
/*初始化线程环境*/
void thread_init(void);
/*当前线程将自己阻塞，并把线程状态标记为stat*/
void thread_block(enum task_status stat);
/*将线程pthread解除阻塞*/
void thread_unblock(struct task_struct* pthread);
/*主动让出cpu，换其他线程运行*/
void thread_yield(void);
/*为线程分配pid*/
pid_t fork_pid(void);
void pad_print(char* buf,int32_t buf_len,void* ptr,char format);
bool elem2thread_info(struct list_elem* pelem,int arg);
//打印任务列表
void sys_ps(void);


#endif