%include "boot.inc"
SECTION MBR vstart=0x7c00 ;告诉编译器起始地址应该编译在0x7c00处，让将来某个加载器把这个程序放到内存0x7c00处加载
    ;初始化寄存器
    xchg bx, bx
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax

    mov sp,0x7c00
    mov ax,0xb800 ; ax为文本信号起始区
    mov ds,ax     ; ds = ax = 0xb800 寄存器寻址默认段寄存器为ds

    ;设置屏幕模式为文本模式，清除屏幕
    mov ax, 3
    int 0x10 

    ;直接操作显存部分,输出hello,立即数寻址默认段寄存器为ds，
    mov byte [0x00],'B' ;低位字节储存ascii字符 小端储存内存顺序相反
    mov byte [0x01],0x07    ;背景储存在第二个字节
    
    mov byte [0x02],'o' 
    mov byte [0x03],0x07
    
    mov byte [0x04],'o' 
    mov byte [0x05],0x07
    
    mov byte [0x06],'t' 
    mov byte [0x07],0x07
    
    mov byte [0x08],'i' 
    mov byte [0x09],0x07
    
    mov byte [0x0A],'n' 
    mov byte [0x0B],0x07
    
    mov byte [0x0C],'g' 
    mov byte [0x0D],0x07
    
    mov byte [0x0E],'!' 
    mov byte [0x0F],0x07

    mov ax, 0
    mov ds, ax      ;让段寄存器回到0!!
    
    mov  eax, LOADER_START_SECTOR     ;起始扇区号，2
    mov bx, LOADER_BASE_ADDR        ;要写入的内存地址，0x900
    mov cx, 4       ;读取的扇区数量
    call read_disk

    xchg bx, bx
    jmp LOADER_BASE_ADDR;       系统跳转到内存0x900处进行loader

; eax=要读取的LBA起始扇区号， bx=读取扇区后要写入的内存地址， cx=读取的扇区数
read_disk:
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
    mov dx, 0x1f3
    out dx, al
        ;中8位，0x1f4
    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al
        ;高8位，0x1f5
    shr eax, cl
    mov dx, 0x1f5
    out dx, al
        ;最高的4位对应device寄存器的低4位，采用主盘LBA模式的话，device高4位是1110
    shr eax, cl
    and al, 0x0f
    or al, 0xe0
    mov dx, 0x1f6
    out dx, al
    ;第3步，向0x1f7端口写入读命令，0x20
    mov al, 0x20
    mov dx, 0x1f7
    out dx, al
    ;第4步，读取0x1f7端口，检测硬盘状态
  .noready:
    nop
    in al, dx
    and al, 0x88    ;第3位为1说明硬盘准备好数据传输，第7位为1说明硬盘繁忙
    cmp al, 0x08
    jnz .noready
    ;第5步，从0x1f0端口即data寄存器读取数据
    mov ax, di  ;di是要读取的扇区数
    mov dx, 256
    mul dx
    mov cx, ax  ;将要读取的次数存到cx
    mov dx, 0x1f0
  .keep_read:
    in ax, dx
    mov [bx], ax
    add bx, 2
    loop .keep_read
    ret

times 510 - ($ - $$) db 0 
db 0x55,0xaa
