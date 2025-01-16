#ifndef __DEVICE_TIME_H
#define __DEVICE_TIME_H
#include "stdint.h"
void timer_init(void);
void mtime_sleep(uint32_t m_seconds);
//休息n个时间中断期
void ticks_to_sleep(uint32_t sleep_ticks);
//毫秒为单位 通过毫秒的中断数来调用ticks_to_sleep 来达到休息毫秒的作用
void mtime_sleep(uint32_t m_seconds);
#endif
