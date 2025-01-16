#ifndef DEVICE_CONSOLE_H
#define DEVICE_CONSOLE_H
#include "print.h"
#include "sync.h"

void console_init(void);
void console_acquire(void);
void console_release(void);
void console_put_str(char* str);
void console_put_int(uint32_t num);
void console_put_char(uint8_t chr);



#endif