#include "string.h"
#include "debug.h"
#include "global.h"

/*将dst_起始的size个字节都设置为value*/
void memset(void* dst_,uint8_t value,uint32_t size)
{
    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t*) dst_;
    while((size--) > 0)
    	*(dst++) = value;
    return;
}

/*将src_起始的size个字节复制到dst_*/
void memcpy(void* dst_,const void* src_,uint32_t size)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_;
    while((size--) > 0)
    	*(dst++) = *(src++);
    return;
}

/*比较a_和b_开头的size个字节，若相等则返回0*/
int memcmp(const void* a_,const void* b_, uint32_t size)
{
    const char* a = a_;
    const char* b = b_;
    ASSERT(a != NULL || b != NULL);
    while((size--) > 0)
    {
    	if(*a != *b)	return (*a > *b) ? 1 : -1;
   	++a,++b;
    }
    return 0;
}

/*将字符串src_复制到dsc_*/
char* strcpy(char* dsc_,const char* src_)
{
    ASSERT(dsc_ != NULL && src_ != NULL);
    char* dsc = dsc_;
    while((*(dsc_++) = *(src_++) ));
    return dsc;     
}

/*返回字符串长度*/
uint32_t strlen(const char* str)
{
    ASSERT(str != NULL);
    const char* ptr = str;
    while(*(ptr++));
    return (ptr - str - 1);             //例如一个字 1 '\0' ptr会指向'\0'后面一位
}

/*比较两个字符串，若a>b则返回1，相等返回0，小于则返回-1*/
int8_t strcmp(const char* a,const char* b)
{
    ASSERT(a != NULL && b != NULL);
    while(*a && *a == *b)
    {
    	a++,b++;
    }   
    return (*a < *b) ? -1 : (*a > *b) ; //这个表达式太猛了 用活了
}

/*从左到右查找字符串str中首次出现字符ch的地址*/
char* strchr(const char* str,const char ch)
{
    ASSERT(str != NULL);
    while(*str)
    {
    	if(*str == ch)	return (char*)str;
    	++str;
    } 
    return NULL;
}

/*从右到左查找字符串str中首次出现字符ch的地址*/
char* strrchr(const char* str,const uint8_t ch)
{
    ASSERT(str != NULL);
    char* last_chrptr = NULL;
    while(*str != 0)
    {
    	if(ch == *str)	last_chrptr = (char*)str;
    	str++;
    }
    return last_chrptr;   
}

/*将src拼接到dsc后面，返回拼接后字符串的地址*/
char* strcat(char* dsc_,const char* src_)
{
    ASSERT(dsc_ != NULL && src_ != NULL);
    char* str = dsc_;
    while(*(str++));
    str--;
    while(*(str++) = *(src_++));
    return dsc_;
}

/*在字符串str中查找字符ch出现的次数*/
char* strchrs(const char* str,uint8_t ch)
{
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    while(*str)
    {
    	if(*str == ch) ++ch_cnt;
    	++str;
    }
    return ch_cnt;
}
