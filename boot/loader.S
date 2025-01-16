%include "boot.inc"
SECTION loader vstart=LOADER_BASE_ADDR  ;0x900
LOADER_STACK_TOP equ LOADER_BASE_ADDR   ;设置程序的栈区，但其实用不到
jmp loader_start    ;跳转到代码区执行，下面的存放数据区来构建gdt,jmp指令占3个字节

;-----------------------------------数据存放-----------------------------------
;定义GDT的第一个段描述符，默认64位全0
GDT_BASE:   dd 0x00000000
            dd 0x00000000
;定义GDT的第二个段描述符，这里设置为代码段描述符
CODE_DESC:  dd 0x0000ffff
            dd DESC_CODE_HIGH4
;定义GDT的第三个段描述符，这里设置为代码段描述符
DATA_STACK_DESC:    dd 0x0000ffff
                    dd DESC_DATA_HIGH4
;定义GDT的第四个段描述符，这里设置为屏幕段描述符
;0xB8000 到0xBFFFF为文字模式显示内存，为了方便操作，这里不采用平坦模型，将段基址设为0xb8000，段大小为0x7fff
;粒度为4K，因此段界限等于0x7fff/4k=7
VIDEO_DESC: dd 0x80000007
            dd DESC_VIDEO_HIGH4
;设置GDT的大小和limit
GDT_SIZE    equ $ - GDT_BASE
GDT_LIMIT   equ GDT_SIZE - 1
;预留60个 四字型 描述符，方便以后扩展
times 59 dq 0
times 5 db 0        ;文件开头在内存的0x900开始，到这里一共占用了3+4*8+59*8+5=512个字节,即0x200个字节

total_mem_bytes dd 0    ;此时total_mem_bytes在内存中的位置是0x900+0x200=0xb00，total_mem_bytes以下的字节才可能可以用，以上的字节一定不能用

;定义段选择子
SELECTOR_CODE        equ (0X0001<<3) + TI_GDT + RPL0    
SELECTOR_DATA	  equ (0X0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO       equ (0X0003<<3) + TI_GDT + RPL0
;定义gdt指针，2字节gdt界限放在前面 4字节gdt地址放在后面，共48位
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE
;buf记录内存大小的缓冲区
ards_buf times 244 db 0
;nr记录20字节结构体ARDS的个数
ards_nr dw 0
;loadermsg 加载区显示字符
loadermsg db 'into loader'

;----------------------------------loader_start----------------------------
loader_start:
    ;进入保护模式之前，需要先检测内存
    ;先执行中断0x15子功能0xe820，遍历主机上的所有内存
    mov sp,LOADER_BASE_ADDR					;设置栈顶为LOADER_BASE_ADDR
    xor ebx, ebx    ;第一次调用时，ebx一定要置为0
    mov ax, 0
    mov es, ax      ;将段寄存器es置为0
    mov di, ards_buf    ;ed：di是ards结构的缓冲区
  .e820_mem_get_loop:
    mov eax, 0x0000e820   ;eax存放子功能号
    mov edx, 0x534d4150 ;edx固定签名  
    mov ecx, 20         ;ecx存放ards结构体的大小，默认是20字节
    int 0x15        ;调用0x15中断
    jc .e820_failed_so_try_e801     ;CF=1表示出错则尝试下一个检测内存的方法，即0xe801中断
    add di, cx      ;让es：di的di指针指向下一个新的ards结构位置
    inc word [ards_nr]      ;ards数量+1
    cmp ebx, 0      
    jnz .e820_mem_get_loop      ;如果ebx不为0，说明还有结构体没返回，则继续循环，如果为0，说明下一个结构体为0，即遍历完了
    ;做一个简单的查找，找到内存上限最大的那个ards，并把内存上限存到total_mem_bytes
    ;对BaseAddrLow + LengthLow 的值进行判断，该值表示对应ards对应的内存上限
    mov cx, [ards_nr]
    mov ebx,ards_buf
    xor edx,edx
  .find_max_mem_area:
    mov eax, [ebx]
    add eax, [ebx + 8]
    add ebx, 20
    cmp edx, eax
    jge .next_ards
    mov edx, eax
  .next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok
  
  ;如果0xe820不成功，则尝试0xe801
  .e820_failed_so_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_failed_so_try_88
    ; 1. 先算低于15MB的内存
    mov cx, 0x400
    mul cx          ;1024字节*ax,乘积的低位放在ax，高位放在dx
    shl edx, 16
    and eax, 0x0000FFFF
    or edx, eax
    add edx, 0x100000       ;加上1MB的内存空缺
    mov esi, edx    
    ; 2.接着算16MB以上的内存 以字节为单位
    xor eax, eax
    mov ax, bx
    mov ecx, 0x10000
    mul ecx
    mov edx, esi
    add edx, eax
    jmp .mem_get_ok

  ;如果0xe801失败了，则尝试0x88
  .e801_failed_so_try_88:
    mov ah,0x88
     int 0x15
     jc .error_hlt
     and eax,0x0000FFFF
     mov cx,0x400                                        
     mul cx
     shl edx,16
     or edx,eax 
     add edx,0x100000

  .error_hlt:
    jmp $
;将内存上限赋给total_mem_bytes
  .mem_get_ok:
    mov [total_mem_bytes], edx

;------------------------------------设置进入保护模式---------------------------------
; 1 打开A20 gate
; 2 加载gdt
; 3 将cr0 的 pe位置1

;打开A20门
in al,0x92                 
or al,0000_0010b
out 0x92,al

;加载gdt
lgdt [gdt_ptr]

;将cr0 的 pe位置1
mov eax,cr0                ;cr0寄存器第0位设置位1
or  eax,0x00000001              
mov cr0,eax

;----------------------------------------已经打开保护模式了-----------------------------------
jmp dword SELECTOR_CODE:p_mode_start                       ;刷新流水线

[bits 32]
p_mode_start: 
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax
;----------------------------------------加载kernel-----------------------------------
    ;把ELF格式的内核加载到低端1M的0x70000处
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx,200
    call read_disk_32
;----------------------------------------打开分页-----------------------------------
    ;创建页目录以及对应页表的映射
    call setup_page
    ;将当前段描述符地址以及偏移量写入内存gdt_ptr
    sgdt [gdt_ptr]
    ;gdt是六个字节大小，前两个字节是偏移量，后面四个字节是基址，将gdt基址放到ebx
    mov ebx, [gdt_ptr + 2]
    ;更改视频段的段基址，改到3GB以上    
    or dword [ebx + 0x18 + 4], 0xc0000000
    ;把gdt的基址也搬到3GB以上
    add dword [gdt_ptr + 2], 0xc0000000
    add esp, 0xc0000000
    ;将页表基址放到cr3，打开cr0，开启页表
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    lgdt [gdt_ptr]
    mov byte [gs:160], 'V'
    xchg bx, bx
    jmp SELECTOR_CODE:enter_kernel

;------------------------------- 跳转到内核区------------------------------------------------   
enter_kernel:
    call kernel_init					          ;根据我们的1M以下的内存分布区 综合考虑出的数据
    mov  esp,0xc009f000             ;作为main主线程的PCB的栈顶，PCB占一页，所以主线程PCB起始地址为0xc009e000
    jmp  KERNEL_ENTER_ADDR

;----------------------------------------创建页目录和页表-----------------------------------
setup_page:
    ;将页目录项清空
    mov ecx, 0x1000     ;循环4096次，因为页目录有1024项，每项占4字节
    mov esi, 0
  .clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir
    ;创建页目录项PDE
  .create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000     ;eax存放第一个页表的地址
    ;设定特殊的页目录项，0,768,1023
    or eax, PG_P | PG_RW_W | PG_US_U 
    mov [PAGE_DIR_TABLE_POS+0x0],eax       ;把页目录的第0项和第768项对应的页表设置为最低端的物理内存              
    mov [PAGE_DIR_TABLE_POS+0xc00],eax
    sub eax,0x1000      
    mov [PAGE_DIR_TABLE_POS+4092],eax       ;虚拟内存最后一个页目录项，指向页目录本身，为了可以通过虚拟地址访问页表本身
    ;创建内核对应页表的映射
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000         ;此时eax存放的是第一个页表的位置
    mov ecx, 256            ;由于内核应该是小于1M，一个页表项有4KB，所以暂时先设置256个页表项
    mov esi, 0
    mov ebx, PG_P | PG_RW_W | PG_US_U 
  .create_kernel_pte:
    mov [eax + esi*4], ebx      ;将页表对应的值设置为，也可以说映射到对应的内存起点
    inc esi
    add ebx,0x1000          ;页的大小为4KB，所以在映射下一个页时，应该加4096
    loop .create_kernel_pte         ;循环，直到把第一个页表的前256个页表项都映射到内存0-1M处
    ;创建页目录第768项之后的映射关系
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000     ;eax存了第二个页表地址
    or eax, PG_P | PG_RW_W | PG_US_U
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254        ;一共有1024个页目录项，最后一个特殊，则要循环1024-1-769=254
    mov esi, 769        ;从第769个目录项开始
  .create_kernel_pde:
    mov [ebx + esi*4], eax      ;从第769个目录项映射第2个页表，以此递推
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde

    ret

; eax=要读取的LBA起始扇区号， bx=读取扇区后要写入的内存地址， cx=读取的扇区数
read_disk_32:
    mov esi, eax    ;备份eax
    mov di, cx      ;备份cx
    ;读写硬盘
    ;第1步：设置要读取的扇区数量
    mov dx, 0x1f2
    mov al, cl      ;删了可以吗?
    out dx, al;
    mov eax, esi
    ;第2步，将要读的起始扇区的地址存入对应端口
        ;低8位，0x1f3
    mov cl, 0x08
    mov dx, 0x1f3
    out dx, al
        ;中8位，0x1f4
    mov dx, 0x1f4
    shr eax, cl
    out dx, al
        ;高8位，0x1f5
    mov dx, 0x1f5
    shr eax, cl
    out dx, al
        ;最高的4位对应device寄存器的低4位，采用主盘LBA模式的话，device高4位是1110
    shr eax, cl
    and al, 0x0f
    or al, 0xe0
    mov dx, 0x1f6
    out dx, al
    ;第3步，向0x1f7端口写入读命令，0x20
    mov dx, 0x1f7
    mov ax, 0x20
    out dx, al
    ;第4步，读取0x1f7端口，检测硬盘状态
  .noready:
    nop
    in al, dx
    and al, 0x88    ;第3位为1说明硬盘准备好数据传输，第7位为1说明硬盘繁忙
    cmp al, 0x08
    jne .noready
    ;第5步，从0x1f0端口即data寄存器读取数据
    mov ax, di  ;di是要读取的扇区数
    mov dx, 256
    mul dx
    mov cx, ax  ;将要读取的次数存到cx
    mov dx, 0x1f0
  .keep_read:
    in ax, dx
    mov [ebx], ax
    add ebx, 2
    loop .keep_read
    ret

;----------------------- 初始化内核 把缓冲区的内核代码放到0x1500区域 ------------------------------------------
;这个地方主要对elf文件头部分用的很多
;可以参照着书上给的格式 来比较对比
kernel_init:
    xor eax,eax   ;全部清零
    xor ebx,ebx
    xor ecx,ecx
    xor edx,edx
    
    ;这里稍微解释一下 因为0x70000 为64kb*7=448kb 而我们的内核映射区域是4MB 而在虚拟地址4MB以内的都可以当作1:1映射
    mov ebx,[KERNEL_BIN_BASE_ADDR+28]
    add ebx,KERNEL_BIN_BASE_ADDR                               ;ebx当前位置为程序段表
    mov dx,[KERNEL_BIN_BASE_ADDR+42]		         ;获取程序段表每个条目描述符字节大小
    mov cx,[KERNEL_BIN_BASE_ADDR+44]                         ;一共有几个段
    
     
 .get_each_segment:
    cmp dword [ebx+0],PT_NULL
    je .PTNULL                                                 ;空即跳转即可 不进行mem_cpy
    
    mov eax,[ebx+8]
    cmp eax,0xc0001500
    jb .PTNULL
    
        
    push dword [ebx+16]                                        ;ebx+16在存储的数是filesz  可以翻到Loader刚开始
                                                               
    mov eax,[ebx+4]                                            
    add eax,KERNEL_BIN_BASE_ADDR
    push eax                                                   ;p_offset 在文件中的偏移位置    源位置         
    push dword [ebx+8]                                         ;目标位置
     
    call mem_cpy
    add esp,12                                                 ;把三个参数把栈扔出去 等于恢复栈指针
    
 .PTNULL:
    add  ebx,edx                                               ;edx是一个描述符字节大小
    loop .get_each_segment                                     ;继续进行外层循环    
    ret
                                        
mem_cpy:
    cld                                                        ;向高地址自动加数字 cld std 向低地址自动移动
    push ebp                                                   ;保存ebp 因为访问的时候通过ebp 良好的编程习惯保存相关寄存器
    mov  ebp,esp 
    push ecx                                                   ;外层循环还要用 必须保存 外层eax存储着还有几个段
    
                                                               ;分析一下为什么是 8 因为进入的时候又重新push了ebp 所以相对应的都需要+4
                                                               ;并且进入函数时 还Push了函数返回地址 所以就那么多了
    mov edi,[ebp+8]                                            ;目的指针 edi存储的是目的位置 4+4
    mov esi,[ebp+12]                                           ;源指针   源位置             8+4
    mov ecx,[ebp+16]                                           ;与Movsb好兄弟 互相搭配      12+4
    
    
    rep movsb                                                  ;一个一个字节复制
       
    pop ecx 
    pop ebp
    ret