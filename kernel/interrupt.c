#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "print.h"
#include "io.h"

#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1

#define IDT_DESC_CNT 0x81       //目前总共支持的中断数
#define EFLAGS_IF   0x00000200  //eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR)   asm volatile ("pushfl; popl %0" : "=g" (EFLAG_VAR)) //将此时eflag寄存器中的值传入指定参数EFLAG_VAR

extern uint32_t syscall_handler(void);  //系统调用对应的中断入口例程

/*中断门描述符结构体*/
/*16位中断处理函数入口地址低16位， 16位中断处理函数代码段选择子，8位固定为0, 8位属性，16位中断处理函数入口地址高16位 */
struct gate_desc{
    uint16_t func_offest_low_word;
    uint16_t selector;
    uint8_t dcount;
    uint8_t attribute;
    uint16_t func_offest_high_word;
};

char* intr_name[IDT_DESC_CNT];      //保存异常的名字
intr_handler idt_table[IDT_DESC_CNT];       //中断处理程序数组,里面存放每种中断处理函数的地址
static struct gate_desc idt[IDT_DESC_CNT];  //创建了一个中断描述符表IDT，里面每个元素都是八个字节的门描述符
extern intr_handler intr_entry_table[IDT_DESC_CNT];     //声明了一个intr_handler类型的定义在kernel.S的中断处理函数入口数组

/*初始化8259A可编程中断控制器PIC*/
static void pic_init(void){
    /*初始化主片*/
   outb (PIC_M_CTRL, 0x11);   // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为0x20,也就是IR[0-7] 为 0x20 ~ 0x27.
   outb (PIC_M_DATA, 0x04);   // ICW3: IR2接从片. 
   outb (PIC_M_DATA, 0x01);   // ICW4: 8086模式, 正常EOI

   /* 初始化从片 */
   outb (PIC_S_CTRL, 0x11);    // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_S_DATA, 0x28);    // ICW2: 起始中断向量号为0x28,也就是IR[8-15] 为 0x28 ~ 0x2F.
   outb (PIC_S_DATA, 0x02);    // ICW3: 设置从片连接到主片的IR2引脚
   outb (PIC_S_DATA, 0x01);    // ICW4: 8086模式, 正常EOI
   
   /*开了键盘中断和定时器中断和IRQ2从片*/
   outb (PIC_M_DATA, 0xf8);
   outb (PIC_S_DATA, 0xbf);

   put_str("pic_init done\n");
}

/*创建中断门描述符*/
/*输入：未初始化的门描述符指针，门描述符内对应的属性，中断处理函数入口地址*/
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function){
    p_gdesc -> func_offest_low_word = (uint32_t)function & 0x0000ffff;
    p_gdesc -> selector = SELECTOR_K_CODE;
    p_gdesc -> dcount = 0;
    p_gdesc -> attribute = attr;
    p_gdesc -> func_offest_high_word = ((uint32_t)function & 0xffff0000) >> 16;
}

/*初始化中断描述符表*/
static void idt_desc_init(){
    int i;
    int lastindex = IDT_DESC_CNT - 1;
    for(i = 0; i < IDT_DESC_CNT; i++){
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3,syscall_handler);
    put_str("idt_desc_init done!\n");
}

/*一些通用的中断对应的处理函数，一般用在异常出现时的处理*/
static void general_intr_handler(uint8_t vec_nr)
{
    // IRQ7和IRQ15会产生伪中断(spurious interrupt),无须处理。
    if (vec_nr == 0x27 || vec_nr == 0x2f)
    {
        return;
    }
    set_cursor(0); // 光标设置在0号位
    int cursor_pos = 0;
    while ((cursor_pos++) < 320) // 一行80字 4行空格
        put_char(' ');

    set_cursor(0);
    put_str("!!!!!!            excetion message begin            !!!!!!\n");
    set_cursor(88);             // 第二行第八个字开始打印
    put_str(intr_name[vec_nr]); // 打印中断向量号
    if (vec_nr == 14)   //处理发生PageFault时的情况
    {
        int page_fault_vaddr = 0;
        asm("movl %%cr2,%0" : "=r"(page_fault_vaddr)); // 发生PF时控制器会将产生错误的虚拟地址存到cr2寄存器中
        put_str("\npage fault addr is ");
        put_int(page_fault_vaddr);
    }
    put_str("!!!!!!            excetion message end              !!!!!!\n");
    while (1);
}

/*完成一般中断处理函数注册及异常名称注册*/
/*构造好intr_name中每个中断的名字，以及idt_table中每个中断对应的处理函数地址*/
static void exception_init(void) {			    
   int i;
   for (i = 0; i < IDT_DESC_CNT; i++) {
      idt_table[i] = general_intr_handler;		    // 默认所有中断函数初始的处理函数为general_intr_handler。
      intr_name[i] = "unknown";				    // 先统一赋值为unknown 
   }
   intr_name[0] = "#DE Divide Error";
   intr_name[1] = "#DB Debug Exception";
   intr_name[2] = "NMI Interrupt";
   intr_name[3] = "#BP Breakpoint Exception";
   intr_name[4] = "#OF Overflow Exception";
   intr_name[5] = "#BR BOUND Range Exceeded Exception";
   intr_name[6] = "#UD Invalid Opcode Exception";
   intr_name[7] = "#NM Device Not Available Exception";
   intr_name[8] = "#DF Double Fault Exception";
   intr_name[9] = "Coprocessor Segment Overrun";
   intr_name[10] = "#TS Invalid TSS Exception";
   intr_name[11] = "#NP Segment Not Present";
   intr_name[12] = "#SS Stack Fault Exception";
   intr_name[13] = "#GP General Protection Exception";
   intr_name[14] = "#PF Page-Fault Exception";
   // intr_name[15] 第15项是intel保留项，未使用
   intr_name[16] = "#MF x87 FPU Floating-Point Error";
   intr_name[17] = "#AC Alignment Check Exception";
   intr_name[18] = "#MC Machine-Check Exception";
   intr_name[19] = "#XF SIMD Floating-Point Exception";
}


/*完成有关中断的所有初始化工作*/
void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();        //初始化中断描述符表
    exception_init();
    pic_init();     //初始化8259A
    /*利用idtr指令加载idt*/
    /*中断描述符表寄存器idtr，低16位是界限，高32位是idt基地址*/
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}

/*获取当前中断状态*/
enum intr_status intr_get_status(){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/*打开中断，返回打开中断前的状态*/
enum intr_status intr_enable(){
    enum intr_status old_status;
    if(intr_get_status() == INTR_ON){
        old_status = INTR_ON;
    }
    else{
        old_status = INTR_OFF;
        asm volatile ("sti");
    }
    return old_status;
}

/*关闭中断，返回打开中断前的状态*/
enum intr_status intr_disable(){
    enum intr_status old_status;
    if(intr_get_status() == INTR_ON){
        old_status = INTR_ON;
        asm volatile ("cli":::"memory");
    }
    else{
        old_status = INTR_OFF;
    }
    return old_status;
}

/*设置中断状态为status*/
enum intr_status intr_set_status(enum intr_status status){
    return status & INTR_ON ? intr_enable() : intr_disable();
}

/*在中断处理程序数组第vector_no个元素中注册安装中断处理程序funciton*/
void register_handler(uint8_t vector_no, intr_handler function){
    idt_table[vector_no] = function;
}