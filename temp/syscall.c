#include "syscall.h"


//本项目最多支持3个参数的系统调用。子功能号和返回值都放在eax中————参见interrupt.asm
//不需要写return retval;
#define _syscall0(NUMBER) ({ \
    int retval;		\
    asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER) : "memory"); \
    retval; \
    })

#define _syscall1(NUMBER, ARG1) ({ \
    int retval;		\
    asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER) , "b"(ARG1) : "memory"); \
    retval; \
    })
    
#define _syscall2(NUMBER, ARG1, ARG2) ({ \
    int retval;		\
    asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER) , "b"(ARG1) , "c"(ARG2): "memory"); \
    retval; \
    })

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({ \
    int retval;		\
    asm volatile ("int $0x80" : "=a"(retval) : "a"(NUMBER) , "b"(ARG1) , "c"(ARG2), "d"(ARG3): "memory"); \
    retval; \
    })
//上面的系统调用都是通过寄存器来传递参数的，若用栈传递参数的话，调用者（用户进程）首先得把参数压在3特权级的栈中，然后内核将其读出来再压入0特权级栈，
//这涉及到两种栈的读写，需要修改TSS中的SS0和esp0字段的值。





//目前int 0x80系统调用支持31个子功能
void* syscall_table[32];            //用于interrupt.asm中 call [syscall_table + eax*4]
                         
                         

void syscall_init(){
    syscall_table[SYS_GETPID] = sys_getpid;
}

unsigned int get_pid(){
	return _syscall0(SYS_GETPID);
}








//以下函数供上面的接口函数调用：


unsigned int sys_getpid(){
	unsigned int esp; 
   	asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程
	return cur->pid;
}

