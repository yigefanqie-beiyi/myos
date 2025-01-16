TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0    ;设置video段的段选择子

[bits 32]
section .data
put_int_buffer  dq  0;  定义8个字节的缓冲区用于数字到字符的转换

section .text
;--------------------------------put_char--------------------------
global put_char
put_char:
    pushad      ;备份32位的寄存器，包括eax，ecx，edx，ebx，esp，ebp，esi，edi
    mov ax, SELECTOR_VIDEO
    mov gs, ax      ;让段寄存器gs存放video段的段选择子
    ;获取光标位置
    ;获取光标高8位位置
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al  ;向CRT的0x03d4地址寄存器写入0x0e
    mov dx, 0x03d5
    in al, dx   ;再从CRT的0x03d5数据寄存器中读取光标高8位存入al
    mov ah, al  ;将光标高8位存入ah
    ;获取光标低8位位置
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al  ;向CRT的0x03d4地址寄存器写入0x0f
    mov dx, 0x03d5
    in al, dx   ;再从CRT的0x03d5数据寄存器中读取光标低8位存入al
    ;将光标存入bx
    mov bx, ax
    ;在栈中获取要打印的字符，调用put_char会将打印的字符压入栈中，再压函数返回地址（4字节），上面又进行了pushad压了32字节
    mov ecx, [esp + 36]     ;因此esp + 36指向要打印的字符（1个字节），其实就存在cl里面
    ;判断回车换行与退格
    cmp cl, 0x0d    
    jz .is_carriage_return  ;如果是回车，则跳转
    cmp cl, 0x0a
    jz .is_line_feed        ;如果是换行，则跳转
    cmp cl, 0x08
    jz .is_backspace        ;如果是退格，则跳转
    jmp .put_other          ;如果都不是，则跳转

  .is_backspace:        
    ;实现退格
    dec bx      ;bx存储了光标的位置，让光标位置自减
    shl bx, 1   ;让bx左移1位，乘2，即光标在内存的位置
    mov byte [gs:bx], 0x20      ;让该处的字符显示为空格
    inc bx      ;让bx++，此时bx指向该处字符的属性
    mov byte [gs:bx], 0x07      ;让该处的字符属性为默认黑底白字的0x07
    shr bx, 1   ;让bx右移1位，除以2，即光标的位置，此时光标位置被更新为退格后的位置，且该位置显示空格
    jmp set_cursor     ;更新光标位置

  .put_other:
    ;打印可见字符
    shl bx, 1       ;bx乘2变为光标在内存的位置
    mov [gs:bx], cl
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1       ;让光标恢复之前位置
    inc bx          ;光标指向下一个位置
    cmp bx, 2000
    jl set_cursor  ;若光标位置<80*25=2000，则说明屏幕未满，可以直接跳转到更新光标位置，如果屏幕满了，则滚屏

  .is_carriage_return:
  .is_line_feed:
    ;回车换行处理
    xor dx, dx      ;清空dx，dx存放被除数
    mov ax, bx
    mov si, 80
    div si          ;光标位置除以80取余存在dx
    sub bx, dx      ;光标位置回到行头
  .is_carriage_return_end:
    add bx, 80      ;光标位置跳转到下一行
    cmp bx, 2000    ;比较是否屏幕满了
  .is_line_feed_end:
    jl set_cursor  ;没满则更新光标位置，满了则滚屏

  .roll_screen:
    ;滚屏
    cld     ;清除方向位
    mov ecx, 960        ;将前24行的数据往上搬运一行，(1920*2)/4=960
    mov esi, 0xc00b80a0 ;第1行地址
    mov edi, 0xc00b8000 ;第0行地址
    rep movsd   ;将数据从源地址（由DS:ESI寄存器指定）复制到目标地址（由ES:EDI寄存器指定），执行复制操作后，ESI和EDI寄存器的值会根据方向标志位（DF）的值自动增加
    ;将最后一行填充为空白
    mov ebx, 3840
    mov ecx, 80
  .cls:
    mov word [gs:ebx], 0x0720;  0720是黑底白字的空格
    add ebx, 2
    loop .cls
    mov bx, 1920

global set_cursor
set_cursor:
    ;将光标值设置为bx
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al
  .put_char_done:
    popad
    ret

;-----------------------------------------put_str-----------------------------
global put_str   
put_str:
    push eax
    push ebx
    push ebp
    xor  eax,eax
    mov  ebp,esp                 ;一般用ebp来寻值 esp上面有个4字节的返回地址 就没有了
    mov  ebx,[ebp+16]            ;指针4字节+12字节寄存器
 .put_char_loop:    
    mov byte al,[ebx]
    cmp  al,0
    je  .str_end
    push ax
    call put_char
    add  esp,2
    inc  ebx
    jmp .put_char_loop
 .str_end:
    pop ebp
    pop ebx
    pop eax
    ret

global put_int
put_int:
    pushad
    mov ebp,esp
    mov eax,[esp+36]        ;32字节+4返回地址
    mov edx,eax
    mov edi,7               ;put_int_buffer偏移量
    mov ecx,8               ;循环八次
    mov ebx,put_int_buffer
 .16based_4bits:
    and edx,0xF
    cmp edx,0x9             
    jg  .is_A2F             ;进入A～F ASCII码处理
    add edx,'0'             ;得0~9的ascii
    jmp .store    
 .is_A2F:
    sub edx,10
    add edx,'A'             ;减去10等于A～F的字符序 + 'A'得ascii   
 .store:
    mov [ebx+edi],dl        ;偏移量 倒序储存在buf
    dec edi                 ;-1
    shr eax,4
    mov edx,eax
    loop .16based_4bits    ;8个16进制ascii入账 且还是大端序 妙
 .ready_to_print:
    inc edi                 ;减去8次成-1了 先循环+1次
 .skip_prefix_0:            ;处理前缀0
    cmp edi,8               ;edi偏移量 8的时候表示第九个字符
    je  .full0              ;全是0 
 .go_on_skip:   
    mov cl,[put_int_buffer+edi]
    inc edi
    cmp cl,'0'
    je  .skip_prefix_0      ;跳转回去看看是不是最后一个数字了
    dec edi                 ;不是的0的话 就到下面打印字符时间
    jmp .put_each_num
 .full0:
    mov  cl,'0'
 .put_each_num:
    mov  cl,[put_int_buffer+edi]
    push ecx
    call put_char
    add  esp,4
    inc  edi
    cmp  edi,8
    jge .end
    jmp .put_each_num
 .end:
    popad 
    ret

global cls_screen: 
cls_screen:                               ; 连着set_cursor
   pushad                                ; 用户态不允许访问显存 只能由中断后进入内核态 故此为系统调用
   mov ax,SELECTOR_VIDEO                 ; ax做中转
   mov gs,ax                             ; 先放到gs里面 进入中断后再
   
   mov ebx,0
   mov ecx,80*25                         ;清屏幕
   
 .cls:
   mov word [gs:ebx],0x0720		   ;黑底白字空格键
   add ebx,2                              ;一次两字节
   loop .cls        
   mov ebx,0
   jmp set_cursor