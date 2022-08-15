#pragma once
#include "thread.h"
//配合loader.asm进行参考



//--------------   GDT描述符属性  ------------
#define DESC_G_4K	1
#define DESC_D_32	1
#define DESC_L		0
#define DESC_AVL	0
#define DESC_P		1
#define DESC_DPL_0	0
#define DESC_DPL_1	1
#define DESC_DPL_2	2
#define DESC_DPL_3	3
#define DESC_S_CODE	1
#define DESC_S_DATA	1
#define DESC_S_SYS	0
#define DESC_TYPE_CODE	8
#define DESC_TYPE_DATA  2
#define DESC_TYPE_TSS   9
//--------------   TSS描述符属性  ------------
#define	     TSS_DESC_D  0 
#define 	 TSS_ATTR_HIGH             ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0X0)
#define 	 TSS_ATTR_LOW              ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)
//--------------   用户态的代码段和数据段的描述符属性  ------------
#define      GDT_ATTR_HIGH		       ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))
#define      GDT_CODE_ATTR_LOW_DPL3    ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)
#define      GDT_DATA_ATTR_LOW_DPL3    ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA)



//--------------   选择子的属性  ------------
#define	 RPL0  0
#define	 RPL1  1
#define	 RPL2  2
#define	 RPL3  3
#define TI_GDT 0
#define TI_LDT 1
//系统段，0特权级：
#define SELECTOR_K_CODE	   ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA	   ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK   SELECTOR_K_DATA 
#define SELECTOR_K_GS	   ((3 << 3) + (TI_GDT << 2) + RPL0)
//TSS选择子：
#define SELECTOR_TSS       ((4 << 3) + (TI_GDT << 2) + RPL0)
//用户段，3特权级：
#define SELECTOR_U_CODE  	   ((5 << 3) + (TI_GDT << 2) + RPL3) //用户态代码段选择子
#define SELECTOR_U_DATA	       ((6 << 3) + (TI_GDT << 2) + RPL3) //用户态数据段选择子
#define SELECTOR_U_STACK	   SELECTOR_U_DATA                   //用户态栈段选择子







//GDT描述符
struct gdt_desc{
    unsigned int   limit_low_word;
    unsigned int   base_low_word;
    unsigned char  base_mid_byte;
    unsigned char  attr_low_byte;
    unsigned char  limit_high_attr_high;
    unsigned char  base_high_byte;
};
//TSS结构体
struct TSS{
    unsigned int backlink;
    unsigned int* esp0;
    unsigned int ss0;
    unsigned int* esp1;
    unsigned int ss1;
    unsigned int* esp2;
    unsigned int ss2;
    unsigned int cr3;
    unsigned int (*eip) ();
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned int trace;
    unsigned int io_base;
};



void tss_init();


void updata_tss_esp(struct task_struct* pthread);



