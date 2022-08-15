;-----------------------------------------------
;先定义一些需要用到的宏：
;-----------------------------------------------
LOADER_BASE_ADDR     equ 0x900                     ;loader被MBR加载到内核的物理地址
PAGE_DIR_TABLE_POS   equ 0x100000                  ;页目录表（二级页表）的物理地址，这是实模式低端1MB（0～0xfffff）物理内存外的第一个字节的物理地址
KERNEL_START_SECTOR  equ 0x9                       ;内核从硬盘第9个扇区开始存放。注：第0个扇区放MBR，第2～5个扇区放loader。
KERNEL_BIN_BASE_ADDR equ 0x70000                   ;内核被loader加载到内存的物理地址，在实模式低端1MB（0～0xfffff）物理内存内的一片可用区域中
KERNEL_ENTRY_POINT   equ 0xc0001500                ;选择此地址为内核映像入口的虚拟地址，对应的物理地址为0x1500。位于0x7c00处的MBR已经无用，可以被覆盖




;段描述符的高32位取值情况：注：段描述符为8字节，共64位
DESC_G_4K     equ     100000000000000000000000b    ;第23位G为1，表示段界限的粒度为4KB，段界限最大为4G；为0表示段界限的粒度为1B，段界限最大为1M
DESC_D_32     equ     10000000000000000000000b     ;第22位D/B为1，指令中的有效地址和操作数都是32位，指令有效地址用32位EIP寄存器
DESC_L        equ     0000000000000000000000b      ;第21位L设置成0表示不设置成64位代码段，目前我们都是在32位模式下操作，故为0
DESC_AVL      equ     000000000000000000000b       ;第20位没有明确的用途，取值随意
DESC_LIMIT_CODE2  equ 11110000000000000000b        ;第16-19位为段界限的最后四位，全部初始化为1，由于采用32位平坦模型，段界限为(4GB/4KB)-1=0xFFFFF
DESC_LIMIT_DATA2  equ DESC_LIMIT_CODE2             ;数据段与代码段段界限相同
DESC_LIMIT_VIDEO2 equ 00000000000000000000b        ;显存段不采用平坦模型
DESC_P            equ 1000000000000000b            ;第15位P为1时表示段位于内存中，为0表示段不在内存中
DESC_DPL_0        equ 000000000000000b             ;0特权级
DESC_DPL_1        equ 010000000000000b             ;1特权级
DESC_DPL_2        equ 100000000000000b             ;2特权级
DESC_DPL_3        equ 110000000000000b             ;3特权级
DESC_S_CODE       equ 1000000000000b               ;第12位S为1表示非系统段
DESC_S_DATA       equ DESC_S_CODE                  ;第12位S为1表示非系统段
DESC_S_sys        equ 0000000000000b               ;第12位S为0表示系统段，非系统段分为代码段（第11～8位为XCRA）和数据段（第11～8位为XEWA）
DESC_TYPE_CODE    equ 100000000000b                ;X=1,C=0,R=0,A=0，表示代码段是可执行、非一致性、不可读、已访问位A清零     （书上的图有错）
DESC_TYPE_DATA    equ 001000000000b                ;X=0,E=0,W=1,A=0，表示数据段是不可执行、向上扩展、可读写、已访问位A清零 （书上的图有错）



;代码段描述符的高32位初始化
DESC_CODE_HIGH4  equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                      DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
                      DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00  ;0x00表示平坦模型段基址为零
;数据段描述符的高32位初始化
DESC_DATA_HIGH4  equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                      DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + \
                      DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00  ;0x00表示平坦模型段基址为零
;显存段描述符的高32位初始化
DESC_VIDEO_HIGH4 equ  (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                      DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + \
                      DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b  ;显存段为非平坦模型，段基址不为零，其中0x0b = (0xb8000 >>16)
                                                                                 ;书上写错了，不是0x00，而是0x0b              
;选择子的属性：16位的选择子的高13位是段描述符在全局描述符表GDT中的下标
;第0-1位RPL特权级比较是否允许访问，第2位TI为0表示GDT，为1表示LD。第3-15位为索引值，可以作为段描述符在全局描述符表GDT中的下标
RPL0    equ 00b
RPL1    equ 01b
RPL2    equ 10b
RPL3    equ 11b
TI_GDT  equ 000b
TI_LDT  equ 100b
SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0  ;代码段选择子
SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0  ;数据段选择子
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0 ;显存段选择子
;用选择子在GDT中找到段描述符后，将段描述符的段基址（平坦模型段基址为0）加上段内偏移地址（外来输入）就是要访问的地址





;-----------------------------------------------
;页目录表和页表的宏：
;PAGE_DIR_TABLE_POS   equ 0x100000   ;页目录表的物理地址，需要放入32位的页目录基址寄存器PDBR，又名cr3控制寄存器。
;                   PTE为页表项，PDE为页目录项，两者都是4字节即32位。不论是几级页表，每个页的大小一般都是4KB，本项目也是如此：
;                   将4G内存以4KB为单位分成最多1M个标准页，一级页表（简称页表）的每个PTE对应一个标准页，则共需1M个PTE即4B*1M=4MB内存来存放一级页表
;                   将4MB一级页表以4KB为单位分成1K个页，则二级页表（页目录表）的每个PDE对应一级页表的每个4K页，共需1K个PTE即4B*1K=4KB内存来存放页目录表
;                   每个页大小为4KB，即0x1000。4字节的PTE或PDE只有12～31位共20位表示该项对应的页的物理地址，因为地址都是4KB的整数倍，所以可以省下12位：
;-----------------------------------------------
PG_P    equ 1b     ;第0位P为1表示该项对应的页存在于物理内存中
PG_RW_R equ 00b    ;第1位RW为0表示可读不可写
PG_RW_W equ 10b    ;为1表示可读可写
PG_US_S equ 000b   ;第2位US为0表示特权级，特权级别为3（一般是用户进程）的程序不能访问该项对应的页
PG_US_U equ 100b   ;为1表示普通级，任意特权级别（0，1，2，3）的程序都能访问该项对应的页
                   ;本项目的init进程是用户级进程，为了项目实现上的方便，会在用户空间中会访问到内核，所以所有的PDE和PTE一律使用PG_US_U
                   ;或多或少脱离了保护模式的初衷









;-----------------------------------------------------------
;SECTION loader开始：
;-----------------------------------------------------------
section loader vstart=LOADER_BASE_ADDR



;-----------------------------------------------------------
; 全局描述符表GDT：段描述符数组，放在内存中，使用专用指令lgdt加载到48位的GDTR寄存器。注意：GDT的第0个段描述符不可用————乱七八糟的规则真多！
;-----------------------------------------------------------
GDT_BASE:        dd 0x00000000       ;GDT的起始地址，第一个描述符为空                 
                 dd 0x00000000
CODE_DESC:       dd 0x0000FFFF       ;代码段描述符，一个dd为4字节，段描述符为8字节，上面为低4字节。段基址全0，段界限全1 ————平坦模型，段基址为0
                 dd DESC_CODE_HIGH4
DATA_STACK_DESC: dd 0x0000FFFF       ;栈段和数据段共用一个描述符：段基址全0，段界限全1                                ————平坦模型，段基址为0
                 dd DESC_DATA_HIGH4
VIDEO_DESC:      dd 0x80000007       ;显存段描述符：采用非平坦模型，在段描述符里用：段基址（0xb8000）+ 偏移地址（(0～0x7)*4k）
                 dd DESC_VIDEO_HIGH4 ;显存的文本模式在实模式内存布局的0xb8000～0xbffff之间：0xb8000-(0x0b << 16)=0x8000 和 (0xbffff-0xb8000)/4k=0x7

GDT_LIMIT       equ $ - GDT_BASE - 1 
times 60 dq 0                        ;预留60个空的描述符


total_mem_bytes dd 0                 ;占4字节，保存检测到的可用内存大小（单位为字节），此处的内存地址是0x900 + 512 = 0x900 + 0x200 = 0xb00 
                                     ;在内核中实现内存分配系统时还会用到此地址。bochs下使用x 0xb00（虚拟地址）或xp 0xb00（物理地址）会得到该地址的内容
 
gdt_ptr dw GDT_LIMIT                 ;gdt_ptr是全局描述符表GDT的指针，前2字节是GDT界限，后4字节是GDT起始地址，共48位
        dd GDT_BASE                  ;共占6字节


ards_buf times 244 db 0              ;人工对齐，占244字节，用于ARDS结构体（每个占20字节）
ards_nr dw 0                         ;记录ARDS结构体的数量，占2字节





;-----------------------------------------------------------
; loader代码正式开始：
;-----------------------------------------------------------

loader_start:                       ;此处偏移量：0x200+4+6+244+2 = 0x200 + 256 = 0x200 + 0x100 = 0x300


;-----------------------------------------------------------
; 检测可用内存大小（本项目为32MB，即物理地址0xb00的值为0x02000000），提供3种方法：利用BIOS中断0x15子功能0xe820、0xe801或0x88                         
;-----------------------------------------------------------

    xor ebx, ebx                   ;ebx==0
    mov edx, 0x534d4150            ;只赋值一次，在循环体中不会改变
    mov di, ards_buf               ;di指向ARDS结构缓冲区
.e820_mem_get_loop:
    mov eax, 0x0000e820            ;每次循环后都会改变
    mov ecx, 20                    ;ARDS结构体占20字节
    int 0x15
	jc  .e820_failed_so_try_e801   ;标志寄存器的CF位==1则有错误发送，尝试0xe801子功能
    add di,cx					   ;把di的数值增加20，指向下一个ARDS结构体
    inc word [ards_nr]             ;记录ARDS结构体数量
    cmp ebx,0
    jne .e820_mem_get_loop 
    
    
    mov cx,[ards_nr]              
    mov ebx,ards_buf
    xor edx,edx
.find_max_mem_area:               ;在所有ARDS结构体找出最大的作为内存容量
    mov eax,[ebx]			
    add eax,[ebx+8]  
    add ebx,20                    ;指向下一个ARDS结构体
    cmp edx,eax
    jge .next_ards
    mov edx,eax
.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok
    
    
.e820_failed_so_try_e801:   
    mov ax,0xe801
    int 0x15
    jc .e801_failed_so_try_88  
    mov cx,0x400
    mul cx     
    shl edx,16 
    and eax,0x0000FFFF  
    or edx,eax     
    add edx,0x100000             
    mov esi,edx
    xor eax,eax
    mov ax,bx
    mov ecx,0x10000
    mul ecx
    add esi,eax
    mov edx,esi
    jmp .mem_get_ok
 
 
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
     
     
.mem_get_ok:
     mov [total_mem_bytes],edx






;------------------------------- 加载内核--------------------------------------------
packet:
packet_size: db 0x10                     ;此结构占16字节
reserved:    db 0x00                     ;保留项，固定为0
count:       dw 0x0020                   ;读取的扇区数：32，我们的内核不会超过256*512字节即128KB，每次读取16KB并复制到KERNEL_BIN_BASE_ADDR，共8次
bufferoff:   dw 0x1100                   ;缓冲区的偏移地址：0x1100 = 0x900 + 2KB = 0x900 + 0x800。我们放在0x900处的loader不会超过2KB
bufferseg:   dw 0                        ;缓冲区的段基址：0
startsector: dq KERNEL_START_SECTOR      ;要读取的起始扇区的LBA地址：0x09
;buffaddr:   dq 0                        ;64位平坦模型下的buffer address，在packet_size设置成16字节时是无效的               



	mov di, 0
	mov cx, 8
.load_kernel:
	mov ax, 0
	mov ds, ax
	mov ah, 0x42
	mov dl, 0x80
	mov si, packet
	int 0x13
	jc load_kernel_failed      
	call memcpy
	loop .load_kernel
	
	mov ax, 0
	mov ds, ax
	jmp set_p_mode
	
	
memcpy:
	push cx
	mov cx, 0x2000                       ;每次拷贝一个字，16KB需要拷贝8*1024次
	mov si, 0x1100
.memcpy_loop:
	mov bx, 0
	mov ds, bx                           ;ds==0
	mov word ax, [ds:si]
	mov bx, KERNEL_BIN_BASE_ADDR>>4
	mov ds, bx                           ;ds==0x7000
	mov word [ds:di], ax
	add si, 2
	add di, 2
	loop .memcpy_loop
	pop cx
	ret

    
load_kernel_failed:
	mov byte [gs:0x00],'L'               ;表示loader，如果内核加载失败，就会显示'L'
    mov byte [gs:0x01],0x07  
    jmp $
    
    
set_p_mode:                              ;保护模式下不能再使用实模式下的BIOS中断了，所以要先加载内核，再进入保护模式




;-----------------------------------------------------------
; 开启保护模式：
;-----------------------------------------------------------
    lgdt [gdt_ptr]                       ;加载GDT到48位的GDTR寄存器，16位实模式下只有低20位有效（表示段基址），其他位都是0
    
    in al, 0x92                          ;打开A20地址线————历史包袱！
    or al, 0x02
    out 0x92, al
	
	cli                                  ;保护模式下中断机制尚未建立, 关中断
   
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax                         ;cr0寄存器第0位PE置1，开启保护模式

    
    jmp SELECTOR_CODE:p_mode_start       ;使用远跳转指令刷新流水线，并纠正GDTR寄存器 ————CPU把第一个参数SELECTOR_CODE视为选择子。此时是16位保护模式
                                         
;-----------------------------------------------------------
; 正式进入32位的保护模式：
;-----------------------------------------------------------




[bits 32]
p_mode_start:                        ;cs寄存器不再存储代码段基址，而是代码段选择子SELECTOR_CODE
    mov ax, SELECTOR_DATA            ;选择子是16位，所以不用eax
    mov ds, ax                       ;ds寄存器不再存储数据段基址，而是数据段选择子SELECTOR_DATA
    mov es, ax                       ;es寄存器不再存储附加段基址，而是数据段选择子SELECTOR_DATA
    mov fs, ax                       
    mov ss, ax                       ;ss寄存器不再存储栈段基址，而是数据段选择子SELECTOR_DATA
    mov ax, SELECTOR_VIDEO
    mov gs, ax                       ;gs寄存器存储显存段选择子SELECTOR_VIDEO
    mov esp, LOADER_BASE_ADDR






;------------------------------- 启动分页机制 ---------------------------------------------------
    call setup_page                  ;建立页目录表和页表
    sgdt [gdt_ptr]                   ;保存GDTR寄存器的内容到gdt_ptr，等打开分页后再lgdt
                                     ;重新设置GDT，使虚拟地址指向内核的第一个页表
    mov ebx, [gdt_ptr + 2]                 ;获取GDT的基址
    or dword [ebx + 0x18 + 4], 0xc0000000  ;修改显存段描述符，放在内核空间，不能由用户直接操作显卡
    add dword [gdt_ptr + 2], 0xc0000000    ;将GDT的基址加0xc0000000使其成为内核所在的高地址
    
    add esp, 0xc0000000

                                   
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax                     ;此时仍是使用物理地址

                                   
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax                     ;打开分页，此后便是使用虚拟地址了

    lgdt [gdt_ptr]                   ;恢复GDT

	



;------------------------------- 初始化内核---------------------------------------------------
    
    jmp SELECTOR_CODE:enter_kernel ;强制刷新流水线，更新GDT

    enter_kernel:
        call kernel_init
        mov esp, 0xc009f000        ;人为选择的栈顶虚拟地址
        cli                        ;确保关中断，在设置好中断描述符表IDT之前都不要开中断         ————想在物理机上运行的话，必须这么做
        jmp KERNEL_ENTRY_POINT     ;跳到C语言的入口main函数0xc0001500，正式开始用C语言写内核

    
    
    
    






;------------------------------- 创建页目录表和页表 ------------------------------------------------ 
setup_page:

    mov ecx, 4096                          ;页目录表占据4KB空间，清零之
    mov esi, 0
.clear_page_dir:   
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir
    
;--------------------------------创建页目录表：创建255个（1022-768+1）PDE（第0和第1023个有特殊用途）指向与内核空间有关的255个页表--------------
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS            ;物理地址0x100000
    add eax, 0x1000                        ;页目录表本身占4KB即一个物理页，所以第一个页表的物理地址为0x101000。当然页目录表和页表不是必须要挨在一起
    or eax, PG_US_U | PG_RW_W | PG_P
    
    mov [PAGE_DIR_TABLE_POS], eax          ;设置第0个PDE，指向第一个页表（指向物理空间低端4MB，本项目内核只使用低端1MB）的地址即0x101000
                                           ;这是为了保证loader（从上文的jmp SELECTOR_CODE:enter_kernel这行代码开始）在分页机制下也能正确运行
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax  ;从第0xc00个开始的PDE用于内核空间（虚拟地址3GB～4GB之间）：(((3GB-0)/4KB)*4B/4KB)*4B = 3KB = 0xc00 B 
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax   ;最后一个（第1023个）PDE指向页目录表自己即0x100000，用于访问页目录表本身这个4KB页，为了以后能动态操作页表
;--------------------------------
;创建其他（第0xc00/4=768个除外，即从769开始）的内核页目录项：从第0个算起，1GB-4MB内核空间在第768～1022个PDE里，第1～767个PDE留给以后的用户空间
;虽然本项目内核用不到这么多空间，但以后要实现所有的用户进程独享4GB虚拟内存（即有独立的总共(4GB/4KB)*4B=4MB页表），其中高3GB～4GB都是内核的1G虚拟空间
;为子进程创建页表时，要把父进程页目录表中第768～1022个PDE复制到子进程页目录表中的相同位置（768～1022）。
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000                        ;第769个PDE指向第二个页表（每个页表4KB，总共有1K个页表即4MB）的地址……第1022个PDE指向第二百五十五个页表
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254                           ;1022 - 769 + 1 = 254
    mov esi, 769
.create_kernel_pde:
    mov [ebx + esi * 4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde            
   
;------------------------------- 创建页表：目前只创建256个PTE指向本项目的低端（本来有255*1K个PTE可以用于内核1GB-4MB空间）1MB物理空间 -----
;虽然内核的可执行程序被加载后不到1MB内存，但内核进程运行时需要的内存可能到达1GB，此处是用到多少就创建多少PTE，以后给进程分配内存时会创建更多的PTE
	mov ebx, 0x101000                      ;ebx初始化为0x101000即第一个页表的物理地址
    mov ecx, 256                           ;物理空间低端(1MB-0)/4KB=256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx + esi * 4], edx               ;每个PTE占4字节
    add edx, 4096                          ;每个PTE对应一个4KB物理页
    inc esi
    loop .create_pte
    

    
    ret    








;初始化内核，解析ELF文件格式
kernel_init:
	xor eax, eax                
	xor ebx, ebx                      
	xor ecx, ecx   
	xor edx, edx 

	mov dx, [KERNEL_BIN_BASE_ADDR + 42]   ; 偏移文件42字节处的属性是e_phentsize，表示program header的大小
	mov ebx, [KERNEL_BIN_BASE_ADDR + 28]  ; 偏移文件28字节处的属性是e_phoff，表示第1个program header在文件中的偏移量
	add ebx, KERNEL_BIN_BASE_ADDR         ; ebx指向第一个program header
	mov cx, [KERNEL_BIN_BASE_ADDR + 44]   ; 偏移文件44字节处是e_phnum，表示program header的数量

.each_segment:                            ; 拷贝每一个段（segment）
	cmp byte [ebx + 0], 0x0               
	je .PTNULL                            ; 若程序段类型等于0x0（空程序类型），说明该program header未使用
	
	mov eax,[ebx+8]
    cmp eax, KERNEL_ENTRY_POINT 
    jb .PTNULL                            ; 如果本段在内存中的起始虚拟地址小于KERNEL_ENTRY_POINT，则不使用       ————防止KERNEL_ENTRY_POINT处被覆盖
    
	                                      ; 为函数memcpy从右往左依次压入参数，函数原型为memcpy(dst, src, size)：
	push dword [ebx + 16]                 ; program header中偏移16字节处是p_filesz                             ————压入函数memcpy的第三个参数：size
	mov eax, [ebx + 4]                    ; program header中偏移4字节处是p_offset
	add eax, KERNEL_BIN_BASE_ADDR         ; 加上kernel.bin被加载到的物理地址，eax指向该段的物理地址
	push eax                              ;                                                                   ————压入函数memcpy的第二个参数：src
	push dword [ebx + 8]                  ; program header中偏移8字节处是p_vaddr，表示本段在内存中的起始虚拟地址 ————压入函数memcpy的第一个参数：dst
	
	call mem_cpy
	add esp, 12                           ; 清理栈中压入的三个参数
	
.PTNULL:
	add ebx, edx                          ; edx为program header大小，即e_phentsize，在此使ebx指向下一个program header
	loop .each_segment
	
	ret



; 逐字节拷贝：mem_cpy(dst, src, size) 
; -----------------------------------------------------------
; 输入：栈中三个参数（dst, src, size）
; 输出：无
; -----------------------------------------------------------
mem_cpy:
	cld                              ; 控制方向标志位DF，cld使DF置0，表示向高地址方向增加
	push ebp
	mov ebp, esp
	push ecx                         ; rep指令用到了ecx，但ecx在外层循环已经使用，所以先入栈备份
	mov edi, [ebp + 8]               ; 取参数dst
	mov esi, [ebp + 12]              ; 取参数src
	mov ecx, [ebp + 16]              ; 取参数size
	rep movsb                        ; 逐字节拷贝
	                                 ; 恢复环境：
	pop ecx
	pop ebp
	
	ret
