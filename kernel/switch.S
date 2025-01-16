[bits 32]
section .text
global switch_to
switch_to:
    ;栈中此处是返回地址
    push esi
    push edi
    push ebx
    push ebp
    mov eax, [esp + 20] ;得到参数cur，参数cur是当前线程的PCB地址
    mov [eax], esp  ;把当前栈顶指针存放到cur中，即存放到当前线程PCB的第一个元素，内核栈selt_kstack中
    ;--------------以上是保存当前线程的上下文，下面是恢复下一个线程的上下文-----------------
    mov eax, [esp + 24] ;得到栈中的参数next，next是下一个线程的PCB地址
    mov esp, [eax]  ;PCB的第一个成员是当前线程的内核栈，此时esp将会指向下一个线程的内核栈
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret             ;将当前栈顶的值作为返回地址加载到处理器的eip寄存器中，从而使下一个线程的代码恢复执行

