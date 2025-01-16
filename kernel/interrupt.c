#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "print.h"
#include "io.h"

#define PIC_M_CTRL 0x20	       // �����õĿɱ���жϿ�������8259A,��Ƭ�Ŀ��ƶ˿���0x20
#define PIC_M_DATA 0x21	       // ��Ƭ�����ݶ˿���0x21
#define PIC_S_CTRL 0xa0	       // ��Ƭ�Ŀ��ƶ˿���0xa0
#define PIC_S_DATA 0xa1	       // ��Ƭ�����ݶ˿���0xa1

#define IDT_DESC_CNT 0x81       //Ŀǰ�ܹ�֧�ֵ��ж���
#define EFLAGS_IF   0x00000200  //eflags�Ĵ����е�ifλΪ1
#define GET_EFLAGS(EFLAG_VAR)   asm volatile ("pushfl; popl %0" : "=g" (EFLAG_VAR)) //����ʱeflag�Ĵ����е�ֵ����ָ������EFLAG_VAR

extern uint32_t syscall_handler(void);  //ϵͳ���ö�Ӧ���ж��������

/*�ж����������ṹ��*/
/*16λ�жϴ�������ڵ�ַ��16λ�� 16λ�жϴ����������ѡ���ӣ�8λ�̶�Ϊ0, 8λ���ԣ�16λ�жϴ�������ڵ�ַ��16λ */
struct gate_desc{
    uint16_t func_offest_low_word;
    uint16_t selector;
    uint8_t dcount;
    uint8_t attribute;
    uint16_t func_offest_high_word;
};

char* intr_name[IDT_DESC_CNT];      //�����쳣������
intr_handler idt_table[IDT_DESC_CNT];       //�жϴ����������,������ÿ���жϴ������ĵ�ַ
static struct gate_desc idt[IDT_DESC_CNT];  //������һ���ж���������IDT������ÿ��Ԫ�ض��ǰ˸��ֽڵ���������
extern intr_handler intr_entry_table[IDT_DESC_CNT];     //������һ��intr_handler���͵Ķ�����kernel.S���жϴ������������

/*��ʼ��8259A�ɱ���жϿ�����PIC*/
static void pic_init(void){
    /*��ʼ����Ƭ*/
   outb (PIC_M_CTRL, 0x11);   // ICW1: ���ش���,����8259, ��ҪICW4.
   outb (PIC_M_DATA, 0x20);   // ICW2: ��ʼ�ж�������Ϊ0x20,Ҳ����IR[0-7] Ϊ 0x20 ~ 0x27.
   outb (PIC_M_DATA, 0x04);   // ICW3: IR2�Ӵ�Ƭ. 
   outb (PIC_M_DATA, 0x01);   // ICW4: 8086ģʽ, ����EOI

   /* ��ʼ����Ƭ */
   outb (PIC_S_CTRL, 0x11);    // ICW1: ���ش���,����8259, ��ҪICW4.
   outb (PIC_S_DATA, 0x28);    // ICW2: ��ʼ�ж�������Ϊ0x28,Ҳ����IR[8-15] Ϊ 0x28 ~ 0x2F.
   outb (PIC_S_DATA, 0x02);    // ICW3: ���ô�Ƭ���ӵ���Ƭ��IR2����
   outb (PIC_S_DATA, 0x01);    // ICW4: 8086ģʽ, ����EOI
   
   /*���˼����жϺͶ�ʱ���жϺ�IRQ2��Ƭ*/
   outb (PIC_M_DATA, 0xf8);
   outb (PIC_S_DATA, 0xbf);

   put_str("pic_init done\n");
}

/*�����ж���������*/
/*���룺δ��ʼ������������ָ�룬���������ڶ�Ӧ�����ԣ��жϴ�������ڵ�ַ*/
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function){
    p_gdesc -> func_offest_low_word = (uint32_t)function & 0x0000ffff;
    p_gdesc -> selector = SELECTOR_K_CODE;
    p_gdesc -> dcount = 0;
    p_gdesc -> attribute = attr;
    p_gdesc -> func_offest_high_word = ((uint32_t)function & 0xffff0000) >> 16;
}

/*��ʼ���ж���������*/
static void idt_desc_init(){
    int i;
    int lastindex = IDT_DESC_CNT - 1;
    for(i = 0; i < IDT_DESC_CNT; i++){
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3,syscall_handler);
    put_str("idt_desc_init done!\n");
}

/*һЩͨ�õ��ж϶�Ӧ�Ĵ�������һ�������쳣����ʱ�Ĵ���*/
static void general_intr_handler(uint8_t vec_nr)
{
    // IRQ7��IRQ15�����α�ж�(spurious interrupt),���봦��
    if (vec_nr == 0x27 || vec_nr == 0x2f)
    {
        return;
    }
    set_cursor(0); // ���������0��λ
    int cursor_pos = 0;
    while ((cursor_pos++) < 320) // һ��80�� 4�пո�
        put_char(' ');

    set_cursor(0);
    put_str("!!!!!!            excetion message begin            !!!!!!\n");
    set_cursor(88);             // �ڶ��еڰ˸��ֿ�ʼ��ӡ
    put_str(intr_name[vec_nr]); // ��ӡ�ж�������
    if (vec_nr == 14)   //������PageFaultʱ�����
    {
        int page_fault_vaddr = 0;
        asm("movl %%cr2,%0" : "=r"(page_fault_vaddr)); // ����PFʱ�������Ὣ��������������ַ�浽cr2�Ĵ�����
        put_str("\npage fault addr is ");
        put_int(page_fault_vaddr);
    }
    put_str("!!!!!!            excetion message end              !!!!!!\n");
    while (1);
}

/*���һ���жϴ�����ע�ἰ�쳣����ע��*/
/*�����intr_name��ÿ���жϵ����֣��Լ�idt_table��ÿ���ж϶�Ӧ�Ĵ�������ַ*/
static void exception_init(void) {			    
   int i;
   for (i = 0; i < IDT_DESC_CNT; i++) {
      idt_table[i] = general_intr_handler;		    // Ĭ�������жϺ�����ʼ�Ĵ�����Ϊgeneral_intr_handler��
      intr_name[i] = "unknown";				    // ��ͳһ��ֵΪunknown 
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
   // intr_name[15] ��15����intel�����δʹ��
   intr_name[16] = "#MF x87 FPU Floating-Point Error";
   intr_name[17] = "#AC Alignment Check Exception";
   intr_name[18] = "#MC Machine-Check Exception";
   intr_name[19] = "#XF SIMD Floating-Point Exception";
}


/*����й��жϵ����г�ʼ������*/
void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();        //��ʼ���ж���������
    exception_init();
    pic_init();     //��ʼ��8259A
    /*����idtrָ�����idt*/
    /*�ж���������Ĵ���idtr����16λ�ǽ��ޣ���32λ��idt����ַ*/
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}

/*��ȡ��ǰ�ж�״̬*/
enum intr_status intr_get_status(){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/*���жϣ����ش��ж�ǰ��״̬*/
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

/*�ر��жϣ����ش��ж�ǰ��״̬*/
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

/*�����ж�״̬Ϊstatus*/
enum intr_status intr_set_status(enum intr_status status){
    return status & INTR_ON ? intr_enable() : intr_disable();
}

/*���жϴ�����������vector_no��Ԫ����ע�ᰲװ�жϴ������funciton*/
void register_handler(uint8_t vector_no, intr_handler function){
    idt_table[vector_no] = function;
}