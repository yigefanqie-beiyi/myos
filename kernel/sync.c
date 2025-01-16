#include "sync.h"
#include "list.h"
#include "stdint.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

/*��ʼ���ź���*/
void sema_init(struct semaphore* psema, uint8_t value){
    psema -> value = value;
    list_init(&psema -> waiters);
}

/*��ʼ����*/
void lock_init(struct lock* plock){
    plock -> holder = NULL;
    plock -> holder_repeat_nr = 0;
    sema_init(&plock -> semaphore, 1);
}

/*�ź���down����*/
void sema_down(struct semaphore* psema){
    enum intr_status old_status = intr_disable();
    while(psema -> value == 0){
        //��valueΪ0����ʾ�Ѿ������˳���
        ASSERT(!elem_find(&psema->waiters,&(running_thread()->general_tag)));
        //��ǰ���̲߳�Ӧ�����ź�����waiters������
        if(elem_find(&psema->waiters,&(running_thread()->general_tag)))
    	    PANIC("sema_down: seme_down thread already in ready_list\n");
        list_append(&psema->waiters,&(running_thread()->general_tag));  //��ӵ��ȴ�����
    	thread_block(TASK_BLOCKED);                                     //�������л��߳�
    }
    //��valueΪ1���߱����Ѻ�ִ����������
    psema -> value--;
    ASSERT(psema->value == 0);
    //�ָ�֮ǰ���ж�״̬
    intr_set_status(old_status);
}

//�ź���value����,�����̣߳��������¼���׼��������
void sema_up(struct semaphore* psema)
{
    //���жϣ���֤ԭ�Ӳ���
    enum intr_status old_status = intr_disable();
    ASSERT(!psema->value);
    if(!list_empty(&psema->waiters))
    {
    	thread_unblock((struct task_struct*)((uint32_t)list_pop(&psema->waiters) & 0xfffff000));
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}

//��ȡ����Դ
void lock_acquire(struct lock* plock)
{
    if(plock->holder != running_thread())
    {
    	sema_down(&plock->semaphore);		//����Ѿ���ռ�� ��ᱻ����
    	plock->holder = running_thread();	//֮ǰ���������߳� ����ִ�� û������ֱ�Ӽ�������
    	ASSERT(!plock->holder_repeat_nr);
    	plock->holder_repeat_nr = 1;			//������Ϊ1
    }
    else	++plock->holder_repeat_nr;
}

//�ͷ�����Դ
void lock_release(struct lock* plock)
{
    ASSERT(plock->holder == running_thread());	//�ͷ������̱߳�������ӵ����
    if(plock->holder_repeat_nr > 1)	//���ٵ��ĵ�ǰһ���߳� ����ֻ��һ�η��������ʱ ����������
    {
    	--plock->holder_repeat_nr;		
    	return;
    }
    ASSERT(plock->holder_repeat_nr == 1);	//�ٸ����� ���߳���ӵ����ʱ ���λ�ȡ�� �ڶ��ο϶����޷���ȡ����
    						//���Ǳ���ͬ��ҲҪ������release���㳹���ͷ� ��Ȼֻ�е�һ�ε�relase
    						//�ڶ��ζ�����Ҫrelease ��ֱ���ͷ��� �϶��ǲ��е�
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);   
}

