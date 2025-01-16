#ifndef USERPROG__FORK_H
#define USERPROG__FORK_H

#include "stdint.h"
#include "string.h"
#include "thread.h"
//���Ƹ����̵�pcb���ӽ��̣�ֱ���Ȱ�pcb����ҳ �����ں�ջ �ж�ջȫ��һ���ƹ��� ��������Ҫ�޸ĵ���һ�����
int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct* child_thread,struct task_struct* parent_thread);
//�����������ڴ���������ȫ�����Ƹ��ӽ���
//buf_page����Ϊ�û����̼��޷������ڴ� �������˴� ֻ��ͨ��buf_page����Ϊ����
void copy_body_stack3(struct task_struct* parent_thread,struct task_struct* child_thread,void* buf_page);
//���ӽ��̹����ں�ջ���޸ķ���ֵ
int32_t build_child_stack(struct task_struct* child_thread);
//����inode����
void updata_inode_open_cnts(struct task_struct* thread);
//���ܺ��� ��װ �Ѹ�������Դ���ӽ���
int32_t copy_process(struct task_struct* child_thread,struct task_struct* parent_thread);
//��ֹ���ں˵��� ֻ�ܴ��û����̵���
pid_t sys_fork(void);

#endif
