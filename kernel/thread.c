#include "thread.h"
#include "memory.h"    //get_kernel_pages()
#include "interrupt.h"
#include "str.h"
#define NULL 0

#define PG_SIZE 4096

struct task_struct*  main_thread;          //主线程的task_struct
struct list    thread_ready_list;		   //就绪队列
struct list      thread_all_list;		   //所有线程（进程）队列


extern void switch_to(struct task_struct* cur, struct task_struct* next);



//为主线程创建TCB
void make_main_thread(){
    unsigned int esp;                                     //定义此变量前栈顶指针为0xc009f000，定义此变量后的栈顶指针变为0xc009f000-4
    asm ("mov %%esp,  %0" : "=g"(esp));                   //将寄存器%esp的值输出到C变量esp中
    main_thread = (struct task_struct*)(esp & 0xfffff000);//取当前栈顶指针（0xc009f000-4）的高20位作为主线程的PCB或TCB（占一页）基址0xc009e000
                                                          //直接写main_thread = (struct task_struct*)0xc009e000;也行
                                                          //主线程的TCB放在物理低端1MB以内，而其他线程的TCB创建后放在物理低端1MB以外的广阔空间
    	
    main_thread->status = TASK_RUNNING;
    str_cpy(main_thread->name,  "main");
    main_thread->priority = 10;
    main_thread->ticks = 10;
    main_thread->elapsed_ticks = 0;
    main_thread->pgdir=NULL;
    main_thread->self_kstack = (unsigned int*)((unsigned int)main_thread + PG_SIZE);//栈顶初始化为TCB或PCB所在页的顶部，在switch_to()函数中也会改变
    main_thread->stack_magic = 0x20220701; //魔数，无实际意义
    
    //主线程不需要在TCB或PCB所在页的顶部预留中断栈和内核栈的空间，而新创建的线程需要（见下面的thread_start()函数）；
    //其他时候，主线程和新线程发生中断或线程切换时，保存环境的地方取决于当时的栈顶esp地址
    
    if(!elem_find(&thread_all_list, &(main_thread->all_list_tag)))
    	list_append(&thread_all_list, &(main_thread->all_list_tag));
}


void thread_init(){
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    make_main_thread();
}







void kernel_thread(thread_func* function,  void* func_arg){
	intr_enable();     //第一次上CPU运行时是关中断的状态，所以要开中断，允许该线程被时钟中断调度
    function(func_arg);
}


struct task_struct* thread_start(char* name,  unsigned int prio,  thread_func function,  void* func_arg){
    struct task_struct* thread = (struct task_struct*) get_kernel_pages(1);
    //分配一个4KB自然页，且只取低端sizeof(struct task_struct)大小的空间当作task_struct
    //顶端分配给了该线程第一次被调度上cpu时使用的中断栈和内存栈，中间不足4KB的空间就是该线程的内核栈栈顶可以向下扩展的范围
    //TCB或PCB都位于内核空间，包括用户进程的TCB或pcb也是在内核空间。本项目的主线程的TCB或PCB在低端1MB以内，其他线程的TCB或PCB在低端1MB以外
	if(thread==NULL)
		return NULL;    
    thread->status = TASK_READY;
    str_cpy(thread->name,  name);
    thread->priority = prio;
    thread->ticks = prio;
    thread->elapsed_ticks = 0;
    thread->pgdir=NULL;
    thread->self_kstack = (unsigned int*)((unsigned int)thread + PG_SIZE); //栈顶初始化为TCB或PCB所在页的顶部
                                                                           //在switch_to()函数中也会改变，switchTo.asm文件第10行：mov [eax], esp 
    thread->stack_magic = 0x20220701;                   
    

    thread->self_kstack -= sizeof(struct intr_stack);   //减去中断栈的空间，且中断栈的内容全为0————调用get_kernel_pages()时会清零（因为线程首次运行）
    thread->self_kstack -= sizeof(struct thread_stack); //减去线程栈的空间，将thread->self_kstack指向thread_stack的基址
    /*以上两个子栈都属于线程的内核栈的一部分。只有thread_stack在该线程（肯定不是主线程）第一次上CPU运行时被用到，
      因为该线程第一次执行是通过时钟中断处理函数中的schedule()函数里的switch_to()函数的ret返回指令调用的。
      switch_to()里需要恢复5个寄存器的值，以下代码就是在提供可供“恢复”的寄存器值。switch_to()能成功返回，但调用它的中断处理函数不会返回，因为esp变了。
      所以目前的中断栈是形同虚设的。将来实现用户进程时，会将用户进程的初始信息放在中断栈中。
	*/
    struct thread_stack* kthread_stack = (struct thread_stack*)thread->self_kstack; 
    kthread_stack->func_arg = func_arg;                 //仅供该线程第一次被调度上cpu时使用
    kthread_stack->function = function;                 //仅供该线程第一次被调度上cpu时使用
    kthread_stack->eip = kernel_thread;                 //注意分析eip在代码段（而不是数据段和栈段）里的执行流
    kthread_stack->esi = 0; 
    kthread_stack->edi = 0;
    kthread_stack->ebx = 0;
    kthread_stack->ebp = 0;
    

    
    if(!elem_find(&thread_all_list, &(thread->all_list_tag)))
    	list_append(&thread_all_list, &(thread->all_list_tag));
    if(!elem_find(&thread_ready_list, &(thread->general_tag)))
    	list_append(&thread_ready_list, &(thread->general_tag)); 
    
    return thread;
}




void schedule(){  
    unsigned int esp; 
    asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程，即获取pcb页的指针
    //当前运行的进程一开始肯定是TASK_RUNNING，但开始执行schedule()时却不一定，见下面thread_block()中的cur->status = status; schedule();
    if(cur->status == TASK_RUNNING){ 
    	if(!elem_find(&thread_ready_list, &(cur->general_tag)))  
    		list_append(&thread_ready_list, &(cur->general_tag)); 
    	cur->ticks = cur->priority;
    	cur->status = TASK_READY;
    }
    else{
    //此线程需要某事件发生后才能加入就绪队列，加入就绪队列的代码写在处理该事件的函数（本项目是thread_unblock()）里，不写在这里
    }
    
    if(list_empty(&thread_ready_list))
    	return;
    unsigned int thread_tag =(unsigned int)list_pop(&thread_ready_list);
    struct task_struct* next = (struct task_struct*)(thread_tag & 0xfffff000);//通过tag求task_struct基址（被设计成4KB的整数倍），直接取高20位即可
    next->status = TASK_RUNNING;
    switch_to(cur, next);                 
}



void thread_block(enum task_status status){
	enum intr_status old = intr_disable();
	if(status==TASK_BLOCKED || status==TASK_WAITING || status==TASK_HANGING){
		unsigned int esp; 
    	asm ("mov %%esp,  %0" : "=g"(esp)); 
		struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程
		cur->status = status;
		schedule();                                                       //线程阻塞就是使用schedule()调度就绪队列里的其他线程上CPU运行
		                                                                  //当然，调用thread_block()前会将该线程加入到某事件的等待队列里
	}
	intr_set_status(old);
}



void thread_unblock(struct task_struct* pthread){
	enum intr_status old = intr_disable();
	if(pthread->status==TASK_BLOCKED || pthread->status==TASK_WAITING || pthread->status==TASK_HANGING){
		if(pthread->status != TASK_READY){
			pthread->status = TASK_READY;
			if(!elem_find(&thread_ready_list, &(pthread->general_tag)))
				list_push(&thread_ready_list, &(pthread->general_tag)); //放到就绪队列最前面，使其尽快得到调度
		}
	}
	intr_set_status(old);
}


