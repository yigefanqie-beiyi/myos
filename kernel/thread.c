#include "thread.h"   //�������� ���ֽṹ��
#include "stdint.h"   //ǰ׺
#include "string.h"   //memset
#include "global.h"   //�����
#include "memory.h"   //����ҳ��Ҫ
#include "interrupt.h"  //�ж���Ҫ
#include "list.h"
#include "debug.h"
#include "process.h"
#include "file.h"

#define PG_SIZE 4096

struct task_struct* main_thread;        //���߳�main��PCB
struct task_struct* idle_thread;			  //�����̵߳�PCB
struct list thread_ready_list;      //��������
struct list thread_all_list;        //�����������
struct lock pid_lock;
static struct list_elem* thread_tag;        //��������е��߳̽ڵ�

extern void switch_to(struct task_struct* cur,struct task_struct* next);
extern void init(void);

/*Ϊÿ���̷߳���pid*/
static pid_t allocate_pid(void)
{
    static pid_t next_pid = 0;			  //Լ����ȫ�ֱ��� ȫ����+���޸���
    lock_acquire(&pid_lock);
    ++next_pid;
    lock_release(&pid_lock);
    return next_pid;
}

/*Ϊ�̷߳���pid*/
pid_t fork_pid(void){
    return allocate_pid();
}

/*��ȡ��ǰ�̵߳�PCB*/
struct task_struct* running_thread(){
    uint32_t esp;
    asm("mov %%esp, %0" : "=g" (esp));
    return (struct task_struct*)(esp & 0xfffff000);     //�����߳����õ�ջ�������Լ���PCB�У��������趨PCBռһҳ������ȡ��20λ����PCB�ĵ�ַ
}

/*��kernel_threadȥִ��funciton*/
static void kernel_thread(thread_func* function, void* func_arg){
    intr_enable();      //��ִ���̵߳ĺ���ǰҪ���жϣ�����ʱ���жϱ����Σ������޷�ͨ��ʱ���жϵ��������߳�
    function(func_arg);
}

/*�����߳�ջ����Ҫִ�еĺ����Ͷ�Ӧ���η���ջ��*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg){
    pthread -> self_kstack -= sizeof(struct intr_stack);    //Ԥ���ж�ջ��λ��
    pthread -> self_kstack -= sizeof(struct thread_stack);  //Ԥ���߳�ջ��λ��
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread -> self_kstack;  //����һ���߳�ջ��ָ���Ӧλ�ã�Ҫע��C���ԵĽṹ���Ǵӵ��ڴ��ŵ����ڴ��
    kthread_stack -> eip = kernel_thread;
    kthread_stack -> function = function;
    kthread_stack -> func_arg = func_arg;
    kthread_stack -> ebp = kthread_stack -> ebx = kthread_stack -> esi = kthread_stack -> edi = 0;
}

/*��ʼ���̵߳Ļ�����Ϣ*/
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
    pthread -> fd_table[0] = 0; //��׼����
    pthread -> fd_table[1] = 1; //��׼���
    pthread -> fd_table[2] = 2; //��׼����
    /*����ȫ����1*/
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC){
        pthread -> fd_table[fd_idx] = -1;
        fd_idx++;
    }
    pthread->cwd_inode_nr = 0;					//Ĭ�Ϲ���·���ڸ�Ŀ¼
    pthread -> parent_pid = -1;             //Ĭ��û�и�����
    pthread -> stack_magic = 0x19870916;    //ħ����������������
    pthread -> self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);  //�����߳����ں�̬��ջ����λ��
}

/*�������ȼ�Ϊprio���̣߳��߳���Ϊname����ʵ�еĺ���Ϊfunction(func_arg)*/
/*�������û��Ļ����ں˵��̣߳�PCB�������ں˿ռ��*/
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg){
    struct task_struct* thread = get_kernel_pages(1);   //���ں˳ط���һҳ�������PCBָ��thread����ʱthreadָ��PCB����͵�ַ
    init_thread(thread, name, prio);    //  ��ʼ���̵߳Ļ�����Ϣ
    thread_create(thread, function, func_arg);  //������Ӧ���߳�ջ

    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));    //ȷ�����߳�֮ǰ�Ͳ��ھ���������
    list_append(&thread_ready_list, &thread->general_tag);       //�����������

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));    //ȷ�����߳�֮ǰ�Ͳ���ȫ��������
    list_append(&thread_all_list, &thread->all_list_tag);       //����ȫ���̶߳���

    return thread;
}

/*��kernel�е�main��������Ϊ���߳�*/
/*main�������У�������loader����kernelʱmov esp�� 0xc009f000, ����Ԥ��PCB��*/
/*���main��PCB��ַΪ0xc009e000������Ҫget_kernel_pages�������һҳ*/
static void make_main_thread(void){
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);
    //main�����ǵ�ǰ�̣߳��϶�����׼�������У������ӵ�ȫ��������
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));  
    list_append(&thread_all_list, &main_thread->all_list_tag);       
}

/*ʵ���������:����ǰ�̻߳��´����������ھ����������ҵ���һ�������е��̣߳����ϴ�����*/
void schedule(){
    ASSERT(intr_get_status() == INTR_OFF);      //������ΪʲôҪ���жϣ���������� ����������
    struct task_struct* cur = running_thread();     //��ȡ��ǰ�̵߳�PCB
    if(cur -> status == TASK_RUNNING){
        //�����ǰ�߳�ֻ����Ϊʱ��Ƭ�����ˣ��������·ŵ�׼�������У�
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur -> ticks = cur -> priority;
        cur -> status = TASK_READY;
    }
    else{
        //�����ǰ�̻߳���ҪĳЩ��������������CPU������ʱ������׼��������
    }
    if(list_empty(&thread_ready_list))  thread_unblock(idle_thread);
    ASSERT(!list_empty(&thread_ready_list));    //Ϊ�˱�����������߳̿��õ��������ʱ�ö�������֤
    //��ȡ׼�������еĶ����߳�PCB������״̬����Ϊrunning����ͨ��switch_to�л��Ĵ���Ӱ��
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = (struct task_struct*)((uint32_t)thread_tag & 0xfffff000);
    next -> status = TASK_RUNNING;
    process_activate(next);
    switch_to(cur, next);
}

/*ϵͳ����ʱ���е��߳�*/
static void idle(void)
{
    while(1)
    {
    	thread_block(TASK_BLOCKED);	//�������� ������֮��ͨ������hlt ʹcpu���� ֱ���ⲿ�ж�cpu�ָ�
    	asm volatile ("sti; hlt" : : :"memory");
    }
}

/*�����ó�cpu���������߳�����*/
void thread_yield(void)
{
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
    list_append(&thread_ready_list,&cur->general_tag);	//�ŵ���������ĩβ
    cur->status = TASK_READY;					//״̬����ΪREADY �ɱ�����
    schedule();						
    intr_set_status(old_status);
}

/*��ʼ���̻߳���*/
void thread_init(void){
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    process_execute(init,"init");
    make_main_thread();
    idle_thread = thread_start("idle",10,idle,NULL);	//�������߽���
    put_str("thread_init done\n");
}

/*��ǰ�߳̽��Լ������������߳�״̬���Ϊstat*/
void thread_block(enum task_status stat){
    //ֻ��TASK_BLOCKED TASK_WAITING TASK_HANGING ������״̬���Ų��ᱻ����
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || stat == TASK_HANGING));
    enum intr_status old_status = intr_disable();       //���ж�
    struct task_struct* cur = running_thread();
    //���ĵ�ǰ�̵߳�״̬�������ٽ���schedule�Ͳ���ŵ�׼�������У��������
    cur -> status = stat;
    schedule();
    //����������������֮ǰ������ִ��
    intr_set_status(old_status);
}

/*���߳�pthread�������*/
void thread_unblock(struct task_struct* pthread)
{
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if(pthread->status != TASK_READY)
    {
    	//�������߳� ��Ӧ�ô����ھ��������У�
    	ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
    	if(elem_find(&thread_ready_list,&pthread->general_tag))
    	    PANIC("thread_unblock: blocked thread in ready_list\n"); //debug.h�ж����
    	//�������˺ܾõ�������ھ���������ǰ��
    	list_push(&thread_ready_list,&pthread->general_tag);
    	//״̬��Ϊ����̬
    	pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

//�������������Ĳ��������ո�ķ�ʽ���buf
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
//��ӡ�����б�
void sys_ps(void)
{
    char* ps_title = "PID             PPID            STAT             TICKS            COMMAND\n";
    sys_write(stdout_no,ps_title,strlen(ps_title));
    list_traversal(&thread_all_list,elem2thread_info,0);
}

