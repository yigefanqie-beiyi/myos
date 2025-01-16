#ifndef USER_TSS_H
#define USER_TSS_H
#include "thread.h"

/*����tss�е�esp0�ֶε�ֵΪpthread��0��ջ*/
void update_tss_esp(struct task_struct* pthread);
/*����һ��gdt������*/
struct gdt_desc make_gdt_desc(uint32_t* desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high);
/*��gdt�д���tss�����¼���gdt*/
void tss_init();


#endif