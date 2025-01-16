#include "stdio-kernel.h"
#include "stdint.h"
#include "stdio.h"
#include "console.h"

#define va_start(ap,v) ap = (va_list)&v          //apָ���һ���̶�����v
#define va_arg(ap,t)   *((t*)(ap +=4))	   //apָ����һ��������������ֵ
#define va_end(ap)	ap = NULL           //���ap

void printk(const char* format, ...)
{
    va_list args;
    va_start(args,format);
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    console_put_str(buf);
}

void sys_putchar(const char chr)
{
    console_put_char(chr);
}
