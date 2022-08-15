#include "process.h"
#include "print.h"
#include "str.h"
#include "tss.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"


#define EFLAGS_MBS	       (1 << 1)
#define EFLAGS_IF_1	       (1 << 9) //IF位为1，开中断
#define EFLAGS_IF_0	       0        //IF位为0，关中断
#define EFLAGS_IOPL_3	   (3 << 12)//IOPL3，用于测试用户程序在非系统调用下进行IO
#define EFLAGS_IOPL_0	   (0 << 12)//IPOL0
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))
#define default_prio       31
#define PG_SIZE            4096

#define USER_STACK3_VADDR  (0xc0000000 - 0x1000)

#define USER_VADDR_START   0x8048000




void start_process(void* filename){
    void* function = filename;
    unsigned int esp; 
    asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000); //获取当前正在运行的线程或进程
	
    struct intr_stack* proc_stack = (struct intr_stack*)((unsigned int)cur->self_kstack + sizeof(struct thread_stack)); 
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;                                                //用户态不能直接操作显存段，直接初始化为0
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;//用户态的数据段选择子
    proc_stack->eip = function;	
    proc_stack->cs = SELECTOR_U_CODE;								   //用户态的代码段选择子
    proc_stack->eflags = EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1;
    proc_stack->esp = (void*)((unsigned int)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);	
    proc_stack->ss = SELECTOR_U_DATA;                                  //用户态的栈段选择子
    asm volatile ("movl %0, %%esp;jmp intr_exit": : "g"(proc_stack) : "memory");
}



void page_dir_activate(struct task_struct* thread){
    unsigned int pagedir_phy_addr = 0x100000; 
    if(thread->pgdir != NULL){
    	pagedir_phy_addr = addr_v2p((unsigned int)thread->pgdir);
    }
    asm volatile ("movl %0, %%cr3" : : "r"(pagedir_phy_addr) : "memory");
}



void process_activate(struct task_struct* thread){
    page_dir_activate(thread);
    
    //tss切换需要
    if(thread->pgdir)
    	updata_tss_esp(thread);
}



unsigned int* create_page_dir(void){
    unsigned int* page_dir_vaddr = get_kernel_pages(1);				//得到内存
    if(page_dir_vaddr == NULL){
    	console_put_str("create_page_dir: get_kernel_page failed!\n");
    	return NULL;
    }
    
    mem_cpy((unsigned int*)((unsigned int)page_dir_vaddr + 0x300*4), (unsigned int*)(0xfffff000+0x300*4), 1024); // 256项
    
    unsigned int new_page_dir_phy_addr = addr_v2p((unsigned int)page_dir_vaddr);                    
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;                    //最后一项是页目录项自己的地址
    return page_dir_vaddr;									     
}



void create_user_vaddr_bitmap(struct task_struct* user_prog){
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;	 //位图开始管理的位置
    unsigned int bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/ PG_SIZE / 8, PG_SIZE); //向上取整取虚拟页数
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}



void process_execute(void* filename, char* name){
    struct task_struct* thread = get_kernel_pages(1);  //分配一页空间 得到pcb
    init_thread(thread, name, default_prio);		 //初始化pcb
    create_user_vaddr_bitmap(thread);			 //为虚拟地址位图初始化 分配空间
    thread_create(thread, start_process, filename);	 //创造线程 start_process 之后通过start_process intr_exit跳转到用户进程
    thread->pgdir = create_page_dir();		 //把页目录表的地址分配了 并且把内核的页目录都给复制过去 这样操作系统对每个进程都可见
    
    enum intr_status old_status = intr_disable();     
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);     //添加线程 start_process到就绪队列中
    
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);
}


