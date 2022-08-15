/*
CPU原生支持的多任务切换机制（用LDT和TSS）：CPU中有一个专门存储TSS结构体信息的TR寄存器始终指向当前正在运行的任务，任务切换的实质就是TR寄存器指向不同的TSS。
任务在被换下CPU时，由CPU自动地把当前任务的资源状态（寄存器、内存）保存到该任务对应的TSS结构体中，下次通过TSS选择子加载时，恢复这些资源并更新TR寄存器。
每个任务都拥有自己的TSS和LDT，TSS结构体由用户提供，由CPU自动维护。TSS和LDT都只能且必须在GDT中注册描述符。本项目不讨论LDT，但还是顺带提一下。
GDTR寄存器中存储的是GDT的起始地址及界限偏移；而TR、LDTR寄存器中存储的分别是TSS、LDT的选择子，用于在GDT中找到TSS描述符和LDT描述符，从而找到TSS和LDT。
TSS任务状态段描述符（系统段描述符，S位为0；放在GDT中），其段基址和段界限用于确定TSS结构体位置；任务门（放在IDT、GDT、LDT中）需要和TSS配合使用，不讨论。
TSS结构体中有三组栈：SS0和esp0，SS1和esp1，SS2和esp2。最低特权级是3，没有更低的特权级会跳入3特权级，故TSS中没有SS3和esp3。以下提到的TSS特指TSS结构体。
除了从中断和调用门（用得少）返回外，CPU不允许从高特权级转向低特权级。上面三组栈是CPU用来由低特权级往高特权级跳转时用的。
CPU并不会主动在TSS中更新相应特权级的栈指针，除非人为地在TSS中将栈指针改写，否则这三组栈指针将一成不变。
Linux只用到了0特权级和3特权级，用户进程处于3特权级，内核位于0特权级，因此对于Linux来说只需要在TSS中设置SS0和esp0的值就够了。
有一件工作必须且只能用TSS来完成，这就是CPU向更高特权级转移时所使用的栈地址，需要提前在TSS中写入。
Linux为每个核（CPU）创建一个TSS，单个CPU上的所有任务共享同一个TSS，各CPU的TR寄存器保存各CPU上的TSS，在用ltr指令加载TSS后，该TR寄存器永远指向同一个TSS，
之后再也不会重新加载TSS。在进程切换时，只需要把TSS中的SS0和esp0更新为新任务的内核栈的段地址及栈指针。
Linux在TSS中只初始化了SS0、esp0和I/O位图字段，除此之外TSS便没用了，就是个空架子，不再做保存任务状态之用。
*/

#include "tss.h"
#include "str.h"//mem_set()


struct TSS tss;    //全局tss结构体，将其基址加载到TR寄存器，同一个CPU下的所有进程都共享




struct gdt_desc make_gdt_desc(unsigned int* desc_addr, unsigned int limit, unsigned char attr_low, unsigned char attr_high){
    struct gdt_desc desc;
    unsigned int desc_base = (unsigned int) desc_addr;
    desc.limit_low_word =  limit & 0x0000ffff;
    desc.base_low_word = desc_base & 0x0000ffff;
    desc.base_mid_byte = (desc_base & 0x00ff0000) >> 16;
    desc.attr_low_byte = (unsigned char)attr_low;
    desc.limit_high_attr_high = ((limit & 0x000f0000) >> 16) + (unsigned char)attr_high;
    desc.base_high_byte = desc_base >> 24;
    return desc;
}



//在GDT中创建TSS并重新lgdt
void tss_init(){
    unsigned int tss_size = sizeof(struct TSS);
    mem_set(&tss, 0, tss_size);
    tss.ss0 = SELECTOR_K_STACK;
    tss.io_base = tss_size;//当IO位图的偏移地址大于等于TSS大小减1时，就表示没有IO位图
    
	//GDT基址是0xc0000900，把TSS描述符放到第五个（第4个）位置：0x900 + 4*8 = 0x900 + 0x20
    *((struct gdt_desc*)0xc0000920) = make_gdt_desc((unsigned int*)&tss, tss_size-1, TSS_ATTR_LOW, TSS_ATTR_HIGH);
    *((struct gdt_desc*)0xc0000928) = make_gdt_desc((unsigned int*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);//用户态代码段
    *((struct gdt_desc*)0xc0000930) = make_gdt_desc((unsigned int*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);//用户态数据段和栈段
    
    unsigned long long gdt_operand = (8*7 - 1) | (((unsigned long long)(unsigned int)0xc0000900) << 16);//GDT界限为8*7 - 1
    
    asm volatile("lgdt %0" :: "m"(gdt_operand)); //加载GDT基址到GDTR寄存器（48位）
    asm volatile("ltr %w0" :: "r"(SELECTOR_TSS));//加载TSS基址到TR寄存器
}






？？？
void updata_tss_esp(struct task_struct* pthread){
    tss.esp0 = (unsigned int*)((unsigned int)pthread + PG_SIZE);
}





