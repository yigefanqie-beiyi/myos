#include "tss.h"
#include "process.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "console.h"

extern void intr_exit(void);    //声明了外部函数，退出中断的出口

extern struct list thread_ready_list;      //就绪队列
extern struct list thread_all_list;        //所有任务队列

/*构建用户进程的上下文信息*/
void start_process(void* filename_)
{
    //schedule线程调度后 来到这里
    //特权级0级 到 3级通过 iretd "欺骗" cpu  把用户进程的环境给准备好 iretd即进入
    void* function = filename_;
    struct task_struct* cur = running_thread();
    cur -> self_kstack += sizeof(struct thread_stack);      
    struct intr_stack* proc_stack = (struct intr_stack*)cur -> self_kstack;
    												//proc_stack指向了PCB里intr_stack栈的最低处
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;         //操作系统不允许用户访问显存，因此显存段寄存器初始化为0
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;			//数据段选择子
    proc_stack->eip = function;								//函数地址 ip
    proc_stack->cs = SELECTOR_U_CODE;								//cs ip cs选择子
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);				//不能够关闭中断 ELFAG_IF_1 不然会导致无法调度
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR) + PG_SIZE);	//栈空间在0xc0000000以下一页的地方 当然物理内存是操作系统来分配
    proc_stack->ss = SELECTOR_U_DATA;								//数据段选择子
    asm volatile ("movl %0,%%esp;jmp intr_exit": : "g"(proc_stack) : "memory");
}

/*激活进程或者线程的页表*/
void page_dir_activate(struct task_struct* p_thread)
{
    uint32_t pagedir_phy_addr = 0x100000; //之前设置的页目录表的物理地址
    if(p_thread->pgdir != NULL)
    {
    	pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir); //得到实际页目录地址
    }
    asm volatile ("movl %0,%%cr3" : : "r"(pagedir_phy_addr) : "memory");
}

/*激活线程或进程的页表，更新tss中的esp0为进程的特权级0的栈*/
void process_activate(struct task_struct* p_thread)
{
    page_dir_activate(p_thread);
    
    //tss切换需要
    if(p_thread->pgdir)
    	update_tss_esp(p_thread);
}

/*创建页目录表，成功则返回页目录的虚拟地址，将当前页表的表示内核空间的pde复制，使用户进程目录表中的第768-1023个页目录项用内核所在页目录的第768-1023个页目录项代替*/
uint32_t* create_page_dir(void)
{
    uint32_t* page_dir_vaddr = get_kernel_pages(1);				//在内核内存池中申请一页
    if(page_dir_vaddr == NULL)
    {
    	console_put_str("create_page_dir: get_kernel_page failed!\n");
    	return NULL;
    }
    
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4),(uint32_t*)(0xfffff000+0x300*4),1024); // 将内核页目录项的第768项后的256项都复制给用户空间的页目录表对应的页目录项
    
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);                    
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;                    //最后一项是页目录项自己的地址
    return page_dir_vaddr;									     
}

/*创建用户进程的虚拟地址位图*/
void create_user_vaddr_bitmap(struct task_struct* user_prog)
{
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;	 //位图开始管理的位置
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/ PG_SIZE / 8,PG_SIZE); //向上取整取虚拟页数
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/*创建用户进程filename，并将其加入就绪队列等待执行*/
void process_execute(void* filename,char* name)
{
    struct task_struct* thread = get_kernel_pages(1);  //分配一页空间 得到pcb
    init_thread(thread,name,default_prio);		 //初始化pcb
    create_user_vaddr_bitmap(thread);			 //为虚拟地址位图初始化 分配空间
    thread_create(thread,start_process,filename);	 //创造线程 start_process 之后通过start_process intr_exit跳转到用户进程
    thread->pgdir = create_page_dir();		 //把页目录表的地址分配了 并且把内核的页目录都给复制过去 这样操作系统对每个进程都可见
    block_desc_init(thread -> u_block_desc);

    enum intr_status old_status = intr_disable();     
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);     //添加线程 start_process到就绪队列中
    
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_status(old_status);
}
