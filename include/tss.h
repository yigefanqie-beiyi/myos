#ifndef USER_TSS_H
#define USER_TSS_H
#include "thread.h"

/*更新tss中的esp0字段的值为pthread的0级栈*/
void update_tss_esp(struct task_struct* pthread);
/*创建一个gdt描述符*/
struct gdt_desc make_gdt_desc(uint32_t* desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high);
/*在gdt中创建tss并重新加载gdt*/
void tss_init();


#endif