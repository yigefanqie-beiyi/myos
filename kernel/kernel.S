[bits 32]
%define ERROR_CODE nop  ;若在相关的异常中cpu已经自动压入了错误码,为保持栈中格式统一,这里不做操作
%define ZERO push 0     ;若在相关的异常中cpu没有压入错误码,为了统一栈中格式,就手工压入一个0

extern put_str  ;   声明put_str函数是外部声明
extern idt_table    ;   声明idt_table是外部声明
extern syscall_table    ;

section .data
global intr_entry_table ;创建了一个中断处理程序的入口表并导出
intr_entry_table:

%macro VECTOR 2 ;宏定义，接收两个参数，是为了某个中断向量号而定义的中断处理程序
section .text
intr%1entry:
    %2      ;有错误码的中断cpu会自动压入错误码地址，所以不用压入，执行nop，为了对齐，无错误码则压入0
    ;因为要从汇编跳到C语言，肯定会影响上下文的寄存器，因此要保存上下文
    push ds
    push es
    push fs 
    push gs 
    pushad
    mov al, 0x20        ;0x20是EOI命令码
    out 0xa0, al        ;向从片发送EOI命令码
    out 0x20, al        ;向主片发送EOI命令码
    push %1     ;   压入中断向量号
    call [idt_table + %1 * 4];      调用idt_table对应的C语言版本的中断处理函数
    jmp intr_exit
section .data  ;定义数组元素，元素值为intr%1entry，存储入口地址，由于每个宏用的都是同一个section，所以会合并成一个segment，保证数组元素是连续的
    dd  intr%1entry
%endmacro

section .text
global intr_exit
intr_exit:
    ;就恢复上下文
    add esp, 4  ; 跳过中断号
    popad
    pop gs 
    pop fs
    pop es 
    pop ds 
    add esp, 4  ;跳过error_code
    iretd

;利用宏定义生成中断处理程序入口表数组
VECTOR 0x0 ,ZERO
VECTOR 0X1 ,ZERO
VECTOR 0X2 ,ZERO
VECTOR 0x3 ,ZERO
VECTOR 0X4 ,ZERO
VECTOR 0X5 ,ZERO
VECTOR 0x6 ,ZERO
VECTOR 0X7 ,ZERO
VECTOR 0X8 ,ERROR_CODE
VECTOR 0x9 ,ZERO
VECTOR 0XA ,ERROR_CODE
VECTOR 0XB ,ERROR_CODE
VECTOR 0XC ,ERROR_CODE
VECTOR 0XD ,ERROR_CODE
VECTOR 0XE ,ERROR_CODE
VECTOR 0XF ,ZERO
VECTOR 0X10 ,ZERO
VECTOR 0X11 ,ERROR_CODE
VECTOR 0x12 ,ZERO
VECTOR 0X13 ,ZERO
VECTOR 0X14 ,ZERO
VECTOR 0x15 ,ZERO
VECTOR 0X16 ,ZERO
VECTOR 0X17 ,ZERO
VECTOR 0X18 ,ZERO
VECTOR 0X19 ,ZERO
VECTOR 0X1A ,ZERO
VECTOR 0X1B ,ZERO
VECTOR 0X1C ,ZERO
VECTOR 0X1D ,ZERO
VECTOR 0X1E ,ERROR_CODE                               ;处理器自动推错误码
VECTOR 0X1F ,ZERO
VECTOR 0X20 ,ZERO       ;时钟中断
VECTOR 0X21 ,ZERO					;键盘中断
VECTOR 0X22 ,ZERO					;级联
VECTOR 0X23 ,ZERO					;串口2
VECTOR 0X24 ,ZERO					;串口1
VECTOR 0X25 ,ZERO					;并口2
VECTOR 0X26 ,ZERO					;软盘
VECTOR 0X27 ,ZERO					;并口1
VECTOR 0X28 ,ZERO					;实时时钟
VECTOR 0X29 ,ZERO					;重定向
VECTOR 0X2A ,ZERO					;保留
VECTOR 0x2B ,ZERO					;保留
VECTOR 0x2C ,ZERO					;ps/2 鼠标
VECTOR 0x2D ,ZERO					;fpu 浮点单元异常
VECTOR 0x2E ,ZERO					;硬盘
VECTOR 0x2F ,ZERO					;保留

global syscall_handler
syscall_handler:
    push 0      ;中断错误码0
    push ds
    push es
    push fs
    push gs
    pushad
    
    push 0x80			     ; 不管是否需要参数,都一律压入中断向量号,调试时很方便
    push edx			     ;第三个参数
    push ecx			     ;第二个参数
    push ebx			     ;第一个参数
    
    call [syscall_table + eax*4]
    add  esp,12
    
    mov [esp + 8*4],eax	     ;返回值给放到eax中 eax前面有 中断号+EDI+ESI+EBP+ESP+EBX+EDX+ECX 
    jmp intr_exit



