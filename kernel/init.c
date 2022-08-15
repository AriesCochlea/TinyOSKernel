#include "init.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"



void init_all(){ 
	//mem_init();    //根据检测到的可用内存，初始化内核物理内存池、用户物理内存池、内核虚拟内存池
	//thread_init(); //初始化线程总队列、就绪队列，并为主线程创建TCB
	idt_init();    //初始化中断描述符表IDT、注册中断处理函数、初始化可编程中断控制器8259A、最后加载IDT
	intr_enable(); //中断在loader.asm中关闭了，需要打开
}
