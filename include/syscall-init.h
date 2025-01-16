#ifndef USER_SYSCALL_INIT_H
#define USER_SYSCALL_INIT_H
#include "stdint.h"

uint32_t sys_getpid(void);
void syscall_init(void);

#endif
