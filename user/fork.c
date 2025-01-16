#include "fork.h"
#include "global.h"
#include "stdint.h"
#include "string.h"
#include "memory.h"
#include "interrupt.h"
#include "sync.h"
#include "thread.h"
#include "debug.h"
#include "process.h"
#include "stdio-kernel.h"
#include "file.h"
#include "list.h"

extern void intr_exit(void);
extern struct list thread_ready_list;      //��������
extern struct list thread_all_list;        //�����������

//���Ƹ����̵�pcb���ӽ��̣�ֱ���Ȱ�pcb����ҳ �����ں�ջ �ж�ջȫ��һ���ƹ��� ��������Ҫ�޸ĵ���һ�����
int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread,struct task_struct* parent_thread)
{
    //ֱ���Ȱ�pcb����ҳ �����ں�ջ �ж�ջȫ��һ���ƹ��� ��������Ҫ�޸ĵ���һ�����
    memcpy(child_thread,parent_thread,PG_SIZE);
    child_thread->pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY; //֮��Ҫ�ŵ��������� �����ȵ�
    child_thread->ticks = child_thread->priority; //ʱ��Ƭ����
    child_thread->parent_pid = parent_thread->pid; //Ĭ����-1 �����ӽ��̵�parent_pid �����̵�pid
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL; 
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    block_desc_init(child_thread->u_block_desc);	//malloc �ڴ��������ʼ��
    //����λͼ��Ҫ����ҳ ��ҳ�� �Ͼ��������̲��ܹ��������ڴ�λͼ�� ������������Ҫ�Ѹ����̵ĸ������� 
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);  
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    //���Ƹ����̵������ڴ�λͼ �����Լ��շ���õĶ���λͼ�����ǵ��ӽ��̸�ֵ
    memcpy(vaddr_btmp,child_thread->userprog_vaddr.vaddr_bitmap.bits,bitmap_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;
    ASSERT(strlen(child_thread->name) < 11); //���̺���Ӹ�����
    strcat(child_thread->name,"_fork");
    return 0;
}

//�����������ڴ���������ȫ�����Ƹ��ӽ���
//buf_page����Ϊ�û����̼��޷������ڴ� �������˴� ֻ��ͨ��buf_page����Ϊ����
void copy_body_stack3(struct task_struct* parent_thread,struct task_struct* child_thread,void* buf_page)
{
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t prog_vaddr = 0;
    
    //���������ڴ�λͼ���� ����ֻ��Ҫ��λͼ�п�����Щҳ������
    //���ǰ���Щҳ�����ƹ�ȥ���� ͬʱҲ��Ҫ��ҳ�����½����а�װһ��
    while(idx_byte < btmp_bytes_len)
    {
        if(vaddr_btmp[idx_byte])
        {
            idx_bit = 0;
            while(idx_bit < 8) //һ�ֽ�8λ
            {
                if((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte])
                {
                    prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
                    memcpy(buf_page,(void*)prog_vaddr,PG_SIZE);
                    page_dir_activate(child_thread); //�л����û�ҳ�� ��ֹ��װ������������ȥ��
                    get_a_page_without_opvaddrbitmap(PF_USER,prog_vaddr);
                    memcpy((void*)prog_vaddr,buf_page,PG_SIZE);
                    page_dir_activate(parent_thread); //�л��ظ�����
                }
                ++idx_bit;
            }
        }
        ++idx_byte;
    }    
}

//���ӽ��̹����ں�ջ���޸ķ���ֵ
int32_t build_child_stack(struct task_struct* child_thread)
{
    //�ں�ջ����ߵ�ַ�� intr�ж�ջ��͵�ַ��
    struct intr_stack* intr_0_stack = \
      (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
    //����ֵ 0
    intr_0_stack->eax = 0;
    //�����Ұ���ṹ��������
    /*struct thread_stack
    {
        uint32_t ebp;
        uint32_t ebx;
        uint32_t edi;
        uint32_t esi;

        void (*eip) (thread_func* func,void* func_arg); //��������໥��Ӧ ��ret ���������kernel_thread��������
    
        void (*unused_retaddr);                         //ռλ�� ��ջ��վס�˷��ص�ַ��λ�� ��Ϊ�ǻ��ret 
        thread_func* function;                          //����kernel_threadҪ���õĺ�����ַ
        void* func_arg;				      //����ָ��
    };*/
    //���ص�ַ�Ͼ��Ǹߵ�ַ
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;
    uint32_t* esi_ptr_in_thread_stack =  (uint32_t*)intr_0_stack - 2;
    uint32_t* edi_ptr_in_thread_stack =  (uint32_t*)intr_0_stack - 3;
    uint32_t* ebx_ptr_in_thread_stack =  (uint32_t*)intr_0_stack - 4;
    uint32_t* ebp_ptr_in_thread_stack =  (uint32_t*)intr_0_stack - 5;
    
    *ret_addr_in_thread_stack = (uint32_t)intr_exit;
    //����֮���pop���Ḳ��
    *esi_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = \
    *ebp_ptr_in_thread_stack = 0;
    //�ں�ջջ��
    child_thread->self_kstack = ebp_ptr_in_thread_stack;
    return 0;
}

//����inode����
void updata_inode_open_cnts(struct task_struct* thread)
{
    int32_t local_fd = 3,global_fd = 0;
    while(local_fd < MAX_FILES_OPEN_PER_PROC)
    {
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if(global_fd != -1)
            ++file_table[global_fd].fd_inode->i_open_cnts;
        ++local_fd;
    }
}

//���ܺ��� ��װ �Ѹ�������Դ���ӽ���
int32_t copy_process(struct task_struct* child_thread,struct task_struct* parent_thread)
{
    //���ڸ�memcpy ���ɵ�ҳ��
    void* buf_page = get_kernel_pages(1);
    if(buf_page == NULL)
    {
        printk("copy_process: buf_page alloc fail\n");
        return -1;
    }
    if(copy_pcb_vaddrbitmap_stack0(child_thread,parent_thread) == -1)
    {
        printk("copy_process: copy_pcb_vaddrbitmap_stack0 fail\n");
        return -1;
    }
    
    child_thread->pgdir = create_page_dir();
    printk("child_thread->pgdir %x\nparent_thread->pgdir %x\n",child_thread->pgdir,parent_thread->pgdir);
    if(child_thread->pgdir == NULL)
    {
        printk("copy_process: child_thread->pgdir create fail\n");
    	return -1;
    }
    
    copy_body_stack3(parent_thread,child_thread,buf_page);
    build_child_stack(child_thread);
    updata_inode_open_cnts(child_thread);
    mfree_page(PF_KERNEL,buf_page,1);
    return 0;
}

//��ֹ���ں˵��� ֻ�ܴ��û����̵���
pid_t sys_fork(void)
{
    struct task_struct* parent_thread = running_thread();
    struct task_struct* child_thread  = get_kernel_pages(1);
    if(child_thread == NULL)
    	return -1;
    
    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);
    
    if(copy_process(child_thread,parent_thread) == -1)
    	return -1;
    	
    ASSERT(!elem_find(&thread_ready_list,&child_thread->general_tag));
    list_append(&thread_ready_list,&child_thread->general_tag);
    ASSERT(!elem_find(&thread_all_list,&child_thread->all_list_tag));
    list_append(&thread_all_list,&child_thread->all_list_tag);
    
    return child_thread->pid;
}
