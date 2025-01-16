#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

void panic_spin(char* filename, int line, const char* func, const char* condition);

/*__VA_ARGS__��Ԥ�������ṩ�ģ�����ʡ�Ժ����Ӧ�Ĳ���*/
/*__FILE__,__LINE__,__func__Ҳ��Ԥ�������ṩ�ĺ�*/
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION) \
    if(CONDITION){} \
    else{PANIC(#CONDITION);} 
#endif

#endif