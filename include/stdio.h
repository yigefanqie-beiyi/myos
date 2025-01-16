#ifndef LIB_STDIO_H
#define LIB_STDIO_H
#include "stdint.h"


typedef void* va_list;
void itoa(uint32_t value,char** buf_ptr_addr,uint8_t base);
uint32_t vsprintf(char* str,const char* format,va_list ap);
uint32_t printf(const char* format, ...);
uint32_t sprintf(char* _des,const char* format, ...);

#endif
