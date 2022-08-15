/* 小“知识”：X86架构的中断栈独立于内核栈；而ARM架构没有独立的中断栈，中断时的上下文环境保护直接使用当前进程或线程的内核栈；ARM64开始支持独立的中断栈
            ————设计不同而已，花里胡哨全是八股…………
*/

//本文件虽然实现的是线程，但有一些内容也可以直接用于实现进程
#pragma once
#include "list.h"
typedef void thread_func(void*); //定义用于被当成线程执行的函数的类型
                          
enum task_status{                //进程或线程的状态
    TASK_RUNNING,  
    TASK_READY,  
    TASK_BLOCKED,
    TASK_WAITING, 
    TASK_HANGING,
    TASK_DIED  
};



//中断栈：1.用于中断发生时保护线程或进程的上下文环境（中断结束后，逐一出栈来恢复这些寄存器原来的值）
//       2.用于进程切换时保护进程的上下文环境 ————线程切换时不需要保存这么多东西，所以线程上下文环境保存在struct thread_stack中
//此结构在线程或进程的内核栈中位置不固定（因为中断的来临是随机的），但该线程第一次被调度上CPU运行时会将此结构放到TCB所在页的顶部，详见thread_start()函数
struct intr_stack{       
    unsigned int vec_no;   //interrupt.asm宏VECTOR中push %1压入的中断号
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp_dummy;//这里的esp取值不重要，下面的esp才是最终出栈时的有效esp
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int gs;
    unsigned int fs;
    unsigned int es;
    unsigned int ds;
    //以下为发生中断时cpu从低特权级进入高特权级时自动压入：
    unsigned int err_code;//错误码，不能由iretd自动pop，需要手动跳过
    //iretd指令会自动pop出eip、cs、eflags，根据特权级是否有变化，还会pop出ESP、SS
    void (*eip)();        //因为eip寄存器不能通过push和pop指令改变（只能通过jmp,call,ret改变），所以写成函数指针，下同
    unsigned int cs;
    unsigned int eflags;  //32位标志寄存器
    void* esp; 
    unsigned int ss;
};
//该结构对应interrput.asm里的intr%1entry函数和intr_exit函数，intr%1entry的压栈操作都是压入了此结构中，intr_exit中的出栈操作便是此结构的逆操作
//该结构在在thread.c里的唯一作用就是取sizeof(struct intr_stack)，详见thread_start()函数。其他时候都是intr%1entry函数和intr_exit函数直接起作用








//线程栈：用于该线程第一次被调度上CPU运行时“恢复”线程上下文环境：既然是第一次运行，那就需要手动给它初始化一个运行前的环境，详见thread_start()函数
//该线程第一次被调度上CPU运行时会将此结构放到上述的中断栈的下面；其他时候，线程栈在线程的内核栈中位置不固定（取决于线程切换时的栈顶esp指针）
struct thread_stack{
    //Intel386硬件体系的ABI：ebp、ebx、edi、esi、esp这5个寄存器归主调函数所用，其余的寄存器归被调函数所用
    //不管被调函数中是否使用了这5个寄存器，在被调函数执行完后，这5个寄存器的值必须和以前一样，所以被调函数要保存这些寄存器的值：
    unsigned int ebp;
    unsigned int ebx;
    unsigned int edi;
    unsigned int esi;
	//esp的值在本项目中手动处理，存储在task_struct->self_kstack中
	
	
    void (*eip) (thread_func* func, void* func_arg);//线程第一次执行时，此函数指针指向线程函数kernel_thread()，其他时候指向switch_to()的返回地址
    /*线程第一次执行时也是通过switch_to()的返回地址（在schedule()中），但switch_to()返回后栈顶esp已经发生了变化，即kernel_thread()是通过ret指令调用的！
      所以schedule()（在intr_timer_handler()中）不会返回，intr_timer_handler()（在intr%1entry中）也不会返回，最终导致intr%1entry也不会返回！
      故：若线程是第一次被调度上CPU执行，则switch_to()返回后会直接执行线程函数kernel_thread()，即通过switch_to()的ret指令调用kernel_thread()。
          若线程不是第一次执行，梳理一下流程：
            1.线程上次执行时，遇到导致线程切换的时钟中断，会将当前eip（就称为ip1吧）压栈，对应intr_stack中的void (*eip)()；接着执行intr%1entry；
              intr%1entry会压栈很多寄存器（对应intr_stack），然后call [idt_table + %1*4]（即call intr_timer_handler）将那时的eip（称为ip2）压栈；
              intr_timer_handler会调用schedule()并将那时的eip（称为ip3）压栈；schedule()会调用switch_to()并将那时的eip（称为ip4）压栈；
              其中ip4就是这里的void (*eip) (thread_func* func, void* func_arg);
              最后switch_to()将4个ABI规定的寄存器压栈，然后此线程的执行流“一去不复返”，对应的esp也换成了新线程的栈顶指针。
              但switch_to()会返回：switch_to()使用新esp并pop了4个寄存器，然后ret到（调用）新线程的void (*eip) (thread_func* func, void* func_arg);
                                               （递归性质的流程：若新线程是第一次执行，同前文，不赘述；若新线程不是第一次执行，接下来新线程的执行流程和下文的叙述完全相同，不赘述）
            2.线程这次执行时，内核栈里面从低到高的内容依次是：4个寄存器、ip4、……、ip3、……、ip2、很多寄存器、ip1
              需要将这些内容依次出栈：switch_to()清理栈帧并正常返回；schedule()清理栈帧并正常返回；intr_timer_handler清理栈帧并正常返回；
              最后intr%1entry函数jmp intr_exit来清理栈帧并iretd ————ip1成功出栈，CPU从ip1处继续执行线程函数kernel_thread()的函数体。
    */
                
                                                    
    //以下仅供该线程第一次被调度上cpu时使用：
    void *unused_retaddr;                           //占位，充当kernel_thread()函数的返回地址，因为将来不需要通过此地址“返回”
    thread_func* function;                          //线程所执行的函数kernel_thread()中调用的是function(func_arg)
    void* func_arg;				                    //function()所需的参数
};







//线程控制块TCB。进程控制块PCB：每个进程都有自己的PCB，所有的PCB放在一张链表（进程表）中由内核维护，故PCB又可称为进程表项
struct task_struct{
    unsigned int* self_kstack; //内核栈的栈顶指针。每个线程（或进程）都有独立的内核栈。线程（或进程）太多，光是内核栈都会占用大量的物理内存
    struct list_elem general_tag;  //当线程或进程被加入就绪队列、阻塞队列等时，把此值插到对应的队列
    struct list_elem all_list_tag; //全部线程或进程组成的队列，集中管理，相当于进程表
    char name[16];             //线程或进程的名字
    enum task_status status;   //线程或进程的状态
    unsigned int priority;     //线程或进程的优先级：优先级越高，每次任务被调度上处理器后执行的时间片就越长；调度线程时，本项目将priority赋值给ticks
    unsigned int ticks;		   //时间片：每次时钟中断都会将当前线程或进程的ticks减1，当减到0时就被换下CPU
    unsigned int elapsed_ticks;//已经运行的时钟滴嗒数：elapsed_ticks==priority-ticks
    unsigned int* pgdir;	   //进程页目录表的虚拟地址。每个进程都有自己的页表，线程没有（在线程的task_struct中将其置为NULL）
    unsigned int stack_magic;  //魔数，本项目的task_struct和内核栈处于同一个页，栈顶初始化为页的顶端并向下扩展，要防止压栈过程中把task_struct的信息覆盖
                               //intr_stack和thread_stack都位于线程的内核栈，且都位于task_struct所在页的顶部（高地址处）                          
}; 
//该页（在物理低端1MB以外）的内容从低地址到高地址依次是：TCB即task_struct、内核栈可用范围、thread_stack、intr_stack
//也可以把整个页都当成TCB，事实上本项目调度的单位就是整个页，以后的叙述中不再严格区分TCB（task_struct）和TCB所在的页。PCB也是如此，不再赘述。
//实际的操作系统的PCB或TCB都很大，以页为单位。本项目比较简单，TCB或PCB（含task_struct、内核栈、thread_stack、intr_stack）只占一个4KB页。









//初始化总队列、就绪队列，并为主线程main创建TCB
void thread_init();


//创建新线程（就是创建新线程的TCB）：线程名为name，线程优先级为prio，线程所执行的函数是function(func_arg)
struct task_struct* thread_start(char* name, unsigned int prio, thread_func function, void* func_arg);


//调度器：调度机制为Round-Robin Scheduling ，俗称RR，即轮询调度
void schedule();


//当前线程将自己阻塞（主动），并标志其状态为stat： TASK_BLOCKED,TASK_WAITING,TASK_HANGING三者之一
void thread_block(enum task_status status);

//将线程解除阻塞（被动唤醒）
void thread_unblock(struct task_struct* pthread);

