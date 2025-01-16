#ifndef USER_PROCESS_H
#define USER_PROCESS_H
#include "thread.h"

#define USER_VADDR_START 0x8048000

/*�����û����̵���������Ϣ*/
void start_process(void* filename_);
/*������̻����̵߳�ҳ��*/
void page_dir_activate(struct task_struct* p_thread);
/*�����̻߳���̵�ҳ������tss�е�esp0Ϊ���̵���Ȩ��0��ջ*/
void process_activate(struct task_struct* p_thread);
/*����ҳĿ¼���ɹ��򷵻�ҳĿ¼�������ַ������ǰҳ��ı�ʾ�ں˿ռ��pde���ƣ�ʹ�û�����Ŀ¼���еĵ�768-1023��ҳĿ¼�����ں�����ҳĿ¼�ĵ�768-1023��ҳĿ¼�����*/
uint32_t* create_page_dir(void);
/*�����û����̵������ַλͼ*/
void create_user_vaddr_bitmap(struct task_struct* user_prog);
/*�����û�����filename�����������������еȴ�ִ��*/
void process_execute(void* filename,char* name);

#endif