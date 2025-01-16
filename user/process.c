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

extern void intr_exit(void);    //�������ⲿ�������˳��жϵĳ���

extern struct list thread_ready_list;      //��������
extern struct list thread_all_list;        //�����������

/*�����û����̵���������Ϣ*/
void start_process(void* filename_)
{
    //schedule�̵߳��Ⱥ� ��������
    //��Ȩ��0�� �� 3��ͨ�� iretd "��ƭ" cpu  ���û����̵Ļ�����׼���� iretd������
    void* function = filename_;
    struct task_struct* cur = running_thread();
    cur -> self_kstack += sizeof(struct thread_stack);      
    struct intr_stack* proc_stack = (struct intr_stack*)cur -> self_kstack;
    												//proc_stackָ����PCB��intr_stackջ����ʹ�
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;         //����ϵͳ�������û������Դ棬����Դ�μĴ�����ʼ��Ϊ0
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;			//���ݶ�ѡ����
    proc_stack->eip = function;								//������ַ ip
    proc_stack->cs = SELECTOR_U_CODE;								//cs ip csѡ����
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);				//���ܹ��ر��ж� ELFAG_IF_1 ��Ȼ�ᵼ���޷�����
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR) + PG_SIZE);	//ջ�ռ���0xc0000000����һҳ�ĵط� ��Ȼ�����ڴ��ǲ���ϵͳ������
    proc_stack->ss = SELECTOR_U_DATA;								//���ݶ�ѡ����
    asm volatile ("movl %0,%%esp;jmp intr_exit": : "g"(proc_stack) : "memory");
}

/*������̻����̵߳�ҳ��*/
void page_dir_activate(struct task_struct* p_thread)
{
    uint32_t pagedir_phy_addr = 0x100000; //֮ǰ���õ�ҳĿ¼��������ַ
    if(p_thread->pgdir != NULL)
    {
    	pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir); //�õ�ʵ��ҳĿ¼��ַ
    }
    asm volatile ("movl %0,%%cr3" : : "r"(pagedir_phy_addr) : "memory");
}

/*�����̻߳���̵�ҳ������tss�е�esp0Ϊ���̵���Ȩ��0��ջ*/
void process_activate(struct task_struct* p_thread)
{
    page_dir_activate(p_thread);
    
    //tss�л���Ҫ
    if(p_thread->pgdir)
    	update_tss_esp(p_thread);
}

/*����ҳĿ¼���ɹ��򷵻�ҳĿ¼�������ַ������ǰҳ��ı�ʾ�ں˿ռ��pde���ƣ�ʹ�û�����Ŀ¼���еĵ�768-1023��ҳĿ¼�����ں�����ҳĿ¼�ĵ�768-1023��ҳĿ¼�����*/
uint32_t* create_page_dir(void)
{
    uint32_t* page_dir_vaddr = get_kernel_pages(1);				//���ں��ڴ��������һҳ
    if(page_dir_vaddr == NULL)
    {
    	console_put_str("create_page_dir: get_kernel_page failed!\n");
    	return NULL;
    }
    
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4),(uint32_t*)(0xfffff000+0x300*4),1024); // ���ں�ҳĿ¼��ĵ�768����256����Ƹ��û��ռ��ҳĿ¼���Ӧ��ҳĿ¼��
    
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);                    
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;                    //���һ����ҳĿ¼���Լ��ĵ�ַ
    return page_dir_vaddr;									     
}

/*�����û����̵������ַλͼ*/
void create_user_vaddr_bitmap(struct task_struct* user_prog)
{
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;	 //λͼ��ʼ�����λ��
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/ PG_SIZE / 8,PG_SIZE); //����ȡ��ȡ����ҳ��
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/*�����û�����filename�����������������еȴ�ִ��*/
void process_execute(void* filename,char* name)
{
    struct task_struct* thread = get_kernel_pages(1);  //����һҳ�ռ� �õ�pcb
    init_thread(thread,name,default_prio);		 //��ʼ��pcb
    create_user_vaddr_bitmap(thread);			 //Ϊ�����ַλͼ��ʼ�� ����ռ�
    thread_create(thread,start_process,filename);	 //�����߳� start_process ֮��ͨ��start_process intr_exit��ת���û�����
    thread->pgdir = create_page_dir();		 //��ҳĿ¼��ĵ�ַ������ ���Ұ��ں˵�ҳĿ¼�������ƹ�ȥ ��������ϵͳ��ÿ�����̶��ɼ�
    block_desc_init(thread -> u_block_desc);

    enum intr_status old_status = intr_disable();     
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);     //����߳� start_process������������
    
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_status(old_status);
}
