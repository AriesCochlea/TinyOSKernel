;中断分为外部中断（分为不可屏蔽中断和可屏蔽中断）和内部中断（分为软中断和异常）。不可屏蔽中断向量号固定为2，可屏蔽中断的每一种中断源都对应一个中断向量号
;异常由轻到重分为3种：Fault，Trap，Abort。某些异常会有单独的32位错误码。不可屏蔽中断和异常的中断向量号由CPU自动提供（0～31），但中断处理程序得自己编写
;可屏蔽中断的中断向量号由中断代理（如已经过时的Intel 8259A可编程中断控制器，虽然已经被淘汰，但接口没变）提供，软中断的中断向量号由软件提供
;本项目只实现3种中断和对应的中断处理程序：
;1.时钟中断，中断号0x20，用于实现进程调度
;2.键盘中断，中断号0x21，用于实现键盘驱动
;3.软件中断，中断号0x80，用于实现系统调用
;注：对显卡的操作在编程时通过直接写显存来实现



;实模式的中断向量表IVT：         固定在低端1KB物理内存0～0x3ff共1024字节，每个中断向量占4字节，一共可容纳256个。
;保护模式的中断描述符表IDT：每个描述符被称为“门”，共有4种门：任务门、中断门、陷阱门、调用门。现代操作系统很少使用任务门、调用门；本项目只使用中断门。
;                          起始地址不限制，每个门占8字节 ————和loader.asm里的全局描述符表GDT类似，也有对应的中断描述符表寄存器IDTR和专用指令lidt
;                                                           GDT第0个描述符不可用，但IDT没有此限制，第0个门是可以用的 ———— 0号中断表示除法错
;                          可容纳的最大门数是64KB/8B=8192个，但CPU只支持256个中断，其他的可以将中断门描述符高32位的第15位P置0表示中断处理程序不在内存中
;中断门描述符：占8字节共64位，会存储中断处理程序所在目标代码段的段描述符选择子和中断处理程序在目标代码段中的偏移量
;                          前者（选择子）用于在全局描述符表GDT中找到目标代码段的段描述符（获得目标代码段基址），加上后者（偏移量） ————IDT在GDT的“前面”
;标志寄存器eflags：进入中断时会把eflags的TF位和NT位置0。NT位和任务门有关，本项目不讨论。只讨论中断门和陷阱门对IF、TF位的影响。
;                          TF表示Trap Flag，也就是陷阱标志位，这用在调试环境中，当TF为0时表示禁止单步执行，即禁止中断处理程序单步执行
;                          IF位为0表示屏蔽外部中断里的可屏蔽中断（不可屏蔽中断、软中断、异常不受影响），即禁止中断嵌套
;                          中断门会使IF位自动置0；陷阱门主要用于调试程序，允许中断嵌套，不会使IF位自动置0  ————两者都禁止单步执行（TF==0）
;错误码：有些中断发生时，CPU会往栈中压入32位的错误码。中断返回时，CPU并不会主动跳过它的位置，必须手动将其跳过     
;8259A芯片：我们的个人电脑中只有两片，一共16个IRQ接口，而级联一个从片，要占用主片一个IRQ接口，所以在我们的个人电脑上可屏蔽中断只能支持15种中断源
;           虽然已经被淘汰，但接口没变



[bits 32]                    
%define ERROR_CODE nop		 ; 若在相关的异常中cpu已经自动压入了错误码，为保持栈中格式统一，这里不做操作
%define ZERO push 0		     ; 若在相关的异常中cpu没有压入错误码，为了统一栈中格式，就手动压入一个0



extern idt_table;


section .data
global intr_entry_table      ;全局的intr_entry_table数组：存储各个中断处理程序的入口地址
intr_entry_table:

%macro VECTOR 2              ;宏的名字是VECTOR，参数个数为2
section .text
intr%1entry:		         ; 每个中断处理程序都要压入中断向量号，所以一个中断类型对应一个中断处理程序

   %2				         ; %2表示第二个参数，此处是ERROR_CODE或ZERO，即nop 或 push 0

   push ds
   push es
   push fs
   push gs
   pushad

   ; 如果是从片上进入的中断,除了往从片上发送EOI外,还要往主片上发送EOI 
   mov al,0x20                   ; 中断结束命令EOI
   out 0xa0,al                   ; 向从片发送
   out 0x20,al                   ; 向主片发送

   push %1			             ; 不管idt_table中的目标函数是否需要参数，都一律压入中断向量号，调试时很方便
   
   call [idt_table + %1*4]       ; 调用interrupt.c中的idt_table的C版本中断处理函数
   jmp intr_exit
	
	
section .data
   dd    intr%1entry	         ; 存储各个中断处理程序的入口地址，形成intr_entry_table数组
   
%endmacro




section .text
global intr_exit           ; 全局的中断返回函数
intr_exit:	     
   add esp, 4			   ; 跳过4字节的中断号
   popad
   pop gs
   pop fs
   pop es
   pop ds
   add esp, 4			   ; 跳过4字节的错误码
   iretd



;-------------- 0x80号中断---------------

;extern syscall_table;

;section .text   
;global syscall_handler
;syscall_handler:
;    push 0
;    
;    push ds
;    push es
;    push fs
;    push gs
;    pushad
;    
;    push 0x80			     ;形同虚设，会被intr_exit自动跳过。这里是为了方便调试
;    push edx			     ;系统调用中的第三个参数
;    push ecx			     ;系统调用中的第二个参数
;    push ebx			     ;系统调用中的第一个参数
;  
;    call [syscall_table + eax*4] ;根据二进制编程接口ABI约定，eax存储系统调用子功能号，用它在数组外部的syscall_table中索引对应的C语言版子功能处理函数
;    add  esp, 12            ;跨过上面3个参数
;                            ;经过上面的call函数调用，eax的值已经变成了返回值（如果没有返回值也没关系，编译器会保证函数返回后eax的值不变）
;    mov [esp + 8*4], eax	 ;将返回值写到内核栈中的eax备份里。0x80+EDI+ESI+EBP+ESP+EBX+EDX+ECX+EAX：显然在EAX前面有8*4字节
;    jmp intr_exit





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
VECTOR 0X1E ,ERROR_CODE
VECTOR 0X1F ,ZERO
VECTOR 0X20 ,ZERO                   ;时钟中断
VECTOR 0X21 ,ZERO                   ;键盘中断
