#ifndef THREAD_SYNC_H
#define THREAD_SYNC_H
#include "list.h"
#include "stdint.h"

struct task_struct;  // 前向声明，避免包含 thread.h

/*信号量结构*/
struct semaphore{
    uint8_t value;  //信号量值
    struct list waiters;    //处于该信号量值上等待阻塞的所有线程
};

/*锁结构*/
struct lock{
    struct task_struct* holder; //锁的持有者
    struct semaphore semaphore; //锁的信号量
    uint32_t holder_repeat_nr;  //锁的持有者重复申请锁的次数
};

/*初始化信号量*/
void sema_init(struct semaphore* psema, uint8_t value);
/*初始化锁*/
void lock_init(struct lock* plock);
/*信号量down操作*/
void sema_down(struct semaphore* psema);
//信号量value增加,唤醒线程，把它重新加入准备队列中
void sema_up(struct semaphore* psema);
//获取锁资源
void lock_acquire(struct lock* plock);
//释放锁资源
void lock_release(struct lock* plock);

#endif