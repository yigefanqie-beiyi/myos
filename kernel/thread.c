#include "thread.h"   //函数声明 各种结构体
#include "stdint.h"   //前缀
#include "string.h"   //memset
#include "global.h"   //不清楚
#include "memory.h"   //分配页需要
#include "interrupt.h"  //中断需要
#include "list.h"
#include "debug.h"
#include "process.h"
#include "file.h"

#define PG_SIZE 4096

struct task_struct* main_thread;        //主线程main的PCB
struct task_struct* idle_thread;			  //休眠线程的PCB
struct list thread_ready_list;      //就绪队列
struct list thread_all_list;        //所有任务队列
struct lock pid_lock;
static struct list_elem* thread_tag;        //保存队列中的线程节点

extern void switch_to(struct task_struct* cur,struct task_struct* next);
extern void init(void);

/*为每个线程分配pid*/
static pid_t allocate_pid(void)
{
    static pid_t next_pid = 0;			  //约等于全局变量 全局性+可修改性
    lock_acquire(&pid_lock);
    ++next_pid;
    lock_release(&pid_lock);
    return next_pid;
}

/*为线程分配pid*/
pid_t fork_pid(void){
    return allocate_pid();
}

/*获取当前线程的PCB*/
struct task_struct* running_thread(){
    uint32_t esp;
    asm("mov %%esp, %0" : "=g" (esp));
    return (struct task_struct*)(esp & 0xfffff000);     //各个线程所用的栈都是在自己的PCB中，且我们设定PCB占一页，所以取高20位就是PCB的地址
}

/*由kernel_thread去执行funciton*/
static void kernel_thread(thread_func* function, void* func_arg){
    intr_enable();      //在执行线程的函数前要打开中断，避免时钟中断被屏蔽，导致无法通过时钟中断调度其它线程
    function(func_arg);
}

/*创建线程栈，将要执行的函数和对应传参放入栈中*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
    pthread -> self_kstack -= sizeof(struct intr_stack);    //预留中断栈的位置
    pthread -> self_kstack -= sizeof(struct thread_stack);  //预留线程栈的位置
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread -> self_kstack;  //创建一个线程栈，指向对应位置，要注意C语言的结构体是从低内存排到高内存的
    kthread_stack -> eip = kernel_thread;
    kthread_stack -> function = function;
    kthread_stack -> func_arg = func_arg;
    kthread_stack -> ebp = kthread_stack -> ebx = kthread_stack -> esi = kthread_stack -> edi = 0;
}

/*初始化线程的基本信息*/
void init_thread(struct task_struct* pthread, char* name, int prio){
    memset(pthread, 0, sizeof(*pthread));   
    pthread -> pid = allocate_pid();
    strcpy(pthread -> name, name);
    if(pthread == main_thread){
        pthread -> status = TASK_RUNNING;
    }
    else{
        pthread -> status = TASK_READY;
    }
    pthread -> ticks = prio;
    pthread -> elapsed_ticks = 0;
    pthread -> pgdir = NULL;
    pthread -> priority = prio;
    pthread -> fd_table[0] = 0; //标准输入
    pthread -> fd_table[1] = 1; //标准输出
    pthread -> fd_table[2] = 2; //标准错误
    /*其余全部置1*/
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC){
        pthread -> fd_table[fd_idx] = -1;
        fd_idx++;
    }
    pthread->cwd_inode_nr = 0;					//默认工作路径在根目录
    pthread -> parent_pid = -1;             //默认没有父进程
    pthread -> stack_magic = 0x19870916;    //魔数，可以随意设置
    pthread -> self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);  //设置线程在内核态下栈顶的位置
}

/*创建优先级为prio的线程，线程名为name，所实行的函数为function(func_arg)*/
/*无论是用户的还是内核的线程，PCB都是在内核空间的*/
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
    struct task_struct* thread = get_kernel_pages(1);   //从内核池分配一页用来存放PCB指针thread，此时thread指向PCB的最低地址
    init_thread(thread, name, prio);    //  初始化线程的基本信息
    thread_create(thread, function, func_arg);  //创建对应的线程栈

    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));    //确保该线程之前就不在就绪队列中
    list_append(&thread_ready_list, &thread->general_tag);       //加入就绪队列

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));    //确保该线程之前就不在全部队列中
    list_append(&thread_all_list, &thread->all_list_tag);       //加入全部线程队列

    return thread;
}

/*将kernel中的main函数完善为主线程*/
/*main早已运行，我们在loader进入kernel时mov esp， 0xc009f000, 就是预留PCB的*/
/*因此main的PCB地址为0xc009e000，不需要get_kernel_pages额外分配一页*/
static void make_main_thread(void){
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);
    //main函数是当前线程，肯定不在准备队列中，因此添加到全部队列中
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));  
    list_append(&thread_all_list, &main_thread->all_list_tag);       
}

/*实现任务调度:将当前线程换下处理器，并在就绪队列中找到下一个可运行的线程，放上处理器*/
void schedule(){
    ASSERT(intr_get_status() == INTR_OFF);      //？？？为什么要关中断，在哪里关了 ！在锁那里
    struct task_struct* cur = running_thread();     //获取当前线程的PCB
    if(cur -> status == TASK_RUNNING){
        //如果当前线程只是因为时间片到期了，则将其重新放到准备队列中，
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur -> ticks = cur -> priority;
        cur -> status = TASK_READY;
    }
    else{
        //如果当前线程还需要某些条件才能重新上CPU，则暂时不放入准备队列中
    }
    if(list_empty(&thread_ready_list))  thread_unblock(idle_thread);
    ASSERT(!list_empty(&thread_ready_list));    //为了避免队列里无线程可用的情况，暂时用断言来保证
    //获取准备队列中的队首线程PCB，将其状态设置为running，并通过switch_to切换寄存器影响
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = (struct task_struct*)((uint32_t)thread_tag & 0xfffff000);
    next -> status = TASK_RUNNING;
    process_activate(next);
    switch_to(cur, next);
}

/*系统空闲时运行的线程*/
static void idle(void)
{
    while(1)
    {
    	thread_block(TASK_BLOCKED);	//先阻塞后 被唤醒之后即通过命令hlt 使cpu挂起 直到外部中断cpu恢复
    	asm volatile ("sti; hlt" : : :"memory");
    }
}

/*主动让出cpu，换其他线程运行*/
void thread_yield(void)
{
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
    list_append(&thread_ready_list,&cur->general_tag);	//放到就绪队列末尾
    cur->status = TASK_READY;					//状态设置为READY 可被调度
    schedule();						
    intr_set_status(old_status);
}

/*初始化线程环境*/
void thread_init(void){
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    process_execute(init,"init");
    make_main_thread();
    idle_thread = thread_start("idle",10,idle,NULL);	//创建休眠进程
    put_str("thread_init done\n");
}

/*当前线程将自己阻塞，并把线程状态标记为stat*/
void thread_block(enum task_status stat){
    //只有TASK_BLOCKED TASK_WAITING TASK_HANGING 这三种状态，才不会被调度
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || stat == TASK_HANGING));
    enum intr_status old_status = intr_disable();       //关中断
    struct task_struct* cur = running_thread();
    //更改当前线程的状态，这样再进行schedule就不会放到准备队列中，完成阻塞
    cur -> status = stat;
    schedule();
    //下面代码在阻塞解除之前都不会执行
    intr_set_status(old_status);
}

/*将线程pthread解除阻塞*/
void thread_unblock(struct task_struct* pthread)
{
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if(pthread->status != TASK_READY)
    {
    	//被阻塞线程 不应该存在于就绪队列中）
    	ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
    	if(elem_find(&thread_ready_list,&pthread->general_tag))
    	    PANIC("thread_unblock: blocked thread in ready_list\n"); //debug.h中定义过
    	//让阻塞了很久的任务放在就绪队列最前面
    	list_push(&thread_ready_list,&pthread->general_tag);
    	//状态改为就绪态
    	pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

//对齐输出，不足的部分以填充空格的方式输出buf
void pad_print(char* buf,int32_t buf_len,void* ptr,char format)
{
    memset(buf,0,buf_len);
    uint8_t out_pad_0idx = 0;
    switch(format)
    {
        case 's':
            out_pad_0idx = sprintf(buf,"%s",ptr);
            break;
        case 'd':
            out_pad_0idx = sprintf(buf,"%d",*((int16_t*)ptr));
            break;
        case 'x':
            out_pad_0idx = sprintf(buf,"%x",*((uint32_t*)ptr));   
    }
    while(out_pad_0idx < buf_len)
    {
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no,buf,buf_len-1);
}

bool elem2thread_info(struct list_elem* pelem,int arg)
{
    struct task_struct* pthread = elem2entry(struct task_struct,all_list_tag,pelem);
    char out_pad[16] = {0};
    pad_print(out_pad,16,&pthread->pid,'d');
    
    if(pthread->parent_pid == -1)
    	pad_print(out_pad,16,"NULL",'s');
    else
        pad_print(out_pad,16,&pthread->parent_pid,'d');
        
    switch(pthread->status)
    {
        case 0:
            pad_print(out_pad,16,"RUNNING",'s');
            break;
        case 1:
            pad_print(out_pad,16,"READY",'s');
            break;
        case 2:
            pad_print(out_pad,16,"BLOCKED",'s');
            break;
        case 3:
            pad_print(out_pad,16,"WAITING",'s');
            break;
        case 4:
            pad_print(out_pad,16,"HANGING",'s');
            break;
        case 5:
            pad_print(out_pad,16,"DIED",'s');
            break;
    }
    pad_print(out_pad,16,&pthread->elapsed_ticks,'x');
    
    memset(out_pad,0,16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad,pthread->name,strlen(pthread->name));
    strcat(out_pad,"\n");
    sys_write(stdout_no,out_pad,strlen(out_pad));
    return false;
}
//打印任务列表
void sys_ps(void)
{
    char* ps_title = "PID             PPID            STAT             TICKS            COMMAND\n";
    sys_write(stdout_no,ps_title,strlen(ps_title));
    list_traversal(&thread_all_list,elem2thread_info,0);
}

