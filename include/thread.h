#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include "memory.h"
#include "stdint.h"
#include "list.h"
#include "bitmap.h"

#define MAX_FILES_OPEN_PER_PROC 8   //ÿ���������������ļ�������

/*�Զ���һ��ͨ�ú������ͣ����ںܶ��̺߳�������Ϊ�β�����*/
typedef void thread_func(void*);

typedef int16_t pid_t;

/*�洢���̻��̣߳���ͳ��Ϊ�����״̬*/
enum task_status
{
    TASK_RUNNING, // 0
    TASK_READY,   // 1
    TASK_BLOCKED, // 2
    TASK_WAITING, // 3
    TASK_HANGING, // 4
    TASK_DIED     // 5
};

/*������ж�ջ�����ڷ����ж�ʱ�������������ģ���Ӧkernel.S��intr%1entry��ѹ��ջ������*/
struct intr_stack
{
    uint32_t vec_no; //�жϺ�
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
    //������cpu�ӵ���Ȩ���������Ȩ��ʱѹ��
    uint32_t err_code;
    void (*eip) (void);        //����������һ������ָ�� 
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

/*�߳�ջ�������̻߳���*/
struct thread_stack
{
    uint32_t ebp;   //����ABI������c���Եĺ�����Ҫ���û���д�ĺ���ʱ��Ҫ����ebp��ebx��edi��esi��esp������Ĵ�����ֵ
    uint32_t ebx;   //esp��ֵͨ������Լ����֤
    uint32_t edi;
    uint32_t esi;

    void (*eip) (thread_func* func,void* func_arg); //��������໥��Ӧ ��ret ���������kernel_thread��������
    
    void (*unused_retaddr);                         //ռλ�� ��ջ��վס�˷��ص�ַ��λ�� ��Ϊ�ǻ��ret 
    thread_func* function;                          //����kernel_threadҪ���õĺ�����ַ
    void* func_arg;				      //����ָ��
};

/*PCB�ṹ�壬�洢���̻��̵߳Ļ�����Ϣ*/
struct task_struct
{
    uint32_t* self_kstack;                          //��ǰ�̶߳�Ӧ���ں�ջ
    pid_t pid;
    enum task_status status;                        //�߳�״̬
    char name[16];
    uint8_t priority;				      //��Ȩ��
    uint8_t ticks;              //ÿ���ڴ�������ִ�е�ʱ��δ���
    uint32_t elapsed_ticks;     //������ִ���˶�ã������������cpu���к�����ռ���˶���cpu�δ���
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];      //�ļ�����������
    struct list_elem general_tag;       //�����߳���һ��Ķ����еĽڵ�
    struct list_elem all_list_tag;      //�����߳����̶߳����еĽڵ�
    uint32_t* pgdir;        //�����Լ�ҳ��������ַ
    struct virtual_addr userprog_vaddr;     //�û����̵������ַ��
    struct mem_block_desc u_block_desc[DESC_CNT];   //�û������ڴ��������
    uint32_t cwd_inode_nr;			      //����Ŀ¼inode���
    int16_t parent_pid;                 //�����̵�pid�����û�и�������Ϊ-1
    uint32_t stack_magic;			      //Խ����  ��Ϊ����pcb����ľ�������Ҫ�õ�ջ�� ��ʱ��ҪԽ����
};

/*�����߳�ջ����Ҫִ�еĺ����Ͷ�Ӧ���η���ջ��*/
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
/*��ʼ���̵߳Ļ�����Ϣ*/
void init_thread(struct task_struct* pthread, char* name, int prio);
/*�������ȼ�Ϊprio���̣߳��߳���Ϊname����ʵ�еĺ���Ϊfunction(func_arg)*/
/*�������û��Ļ����ں˵��̣߳�PCB�������ں˿ռ��*/
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
/*��ȡ��ǰ�̵߳�PCB*/
struct task_struct* running_thread();
/*ʵ���������:����ǰ�̻߳��´����������ھ����������ҵ���һ�������е��̣߳����ϴ�����*/
void schedule();
/*��ʼ���̻߳���*/
void thread_init(void);
/*��ǰ�߳̽��Լ������������߳�״̬���Ϊstat*/
void thread_block(enum task_status stat);
/*���߳�pthread�������*/
void thread_unblock(struct task_struct* pthread);
/*�����ó�cpu���������߳�����*/
void thread_yield(void);
/*Ϊ�̷߳���pid*/
pid_t fork_pid(void);
void pad_print(char* buf,int32_t buf_len,void* ptr,char format);
bool elem2thread_info(struct list_elem* pelem,int arg);
//��ӡ�����б�
void sys_ps(void);


#endif