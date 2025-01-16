#include "io.h"
#include "timer.h"
#include "print.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY 	100
#define INPUT_FREQUENCY        1193180
#define COUNTER0_VALUE		INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT		0X40
#define COUNTER0_NO 		0
#define COUNTER_MODE		2
#define READ_WRITE_LATCH	3
#define PIT_COUNTROL_PORT	0x43
#define mil_second_per_inir	1000 / IRQ0_FREQUENCY

uint32_t ticks;     //ticks是内核开启时钟中断以来总共的滴答数

void frequency_set(uint8_t counter_port ,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value)
{
    outb(PIT_COUNTROL_PORT,(uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port,(uint8_t)counter_value);
    outb(counter_port,(uint8_t)counter_value >> 8);
    return;
} 

/*时钟的中断处理函数*/
static void intr_timer_handler(void){
    struct task_struct* cur_thread = running_thread();  //获取当前线程的PCB
    ASSERT(cur_thread -> stack_magic == 0x19870916);        //判断该线程的线程栈是否有溢出
    cur_thread -> elapsed_ticks++;      //让当前线程占用cpu的总时间++
    ticks++;        //ticks是内核开启时钟中断以来总共的滴答数
    //如果当前线程的tick用完了，则下CPU，进入调度函数，如果没用完，则继续该线程，并让该线程的tick自减
    if(cur_thread -> ticks == 0){
        schedule();
    }
    else{
        cur_thread -> ticks--;
    }
}

void timer_init(void)
{
    put_str("timer_init start!\n");
    frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER_MODE,COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);      //注册时钟中断处理程序的代码
    put_str("timer_init done!\n");
    return;
}

/*休息n个时间中断期*/
void ticks_to_sleep(uint32_t sleep_ticks)
{
    uint32_t start_tick = ticks;
    while(ticks - start_tick < sleep_ticks) thread_yield();
}

/*毫秒为单位 通过毫秒的中断数来调用ticks_to_sleep 来达到休息毫秒的作用*/
void mtime_sleep(uint32_t m_seconds)
{
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds,mil_second_per_inir);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}
