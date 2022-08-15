#include "sync.h"
#include "interrupt.h"

#define NULL 0


void semaphore_init(struct semaphore* sem, unsigned int value){
	sem->value = value;
	list_init(&(sem->waiters));
}

void semaphore_down(struct semaphore* sem){
	enum intr_status old = intr_disable(); //关中断来保证原子操作
	unsigned int esp; 
    asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程
	
	while(0 == sem->value){ //虽然写的是while，但不会在CPU上一直执行，只有被thread_unblock()加入到就绪队列并被调度上CPU才会执行while
		if(!elem_find(&(sem->waiters), &(cur->general_tag))){
			list_append(&(sem->waiters), &(cur->general_tag));        //加入信号量的等待队列
			thread_block(TASK_BLOCKED);                               //线程主动阻塞自己，并调度就绪队列里的其他线程上CPU运行
		}
	}
	--(sem->value);
	intr_set_status(old);
}

void semaphore_up(struct semaphore* sem){
	enum intr_status old = intr_disable(); //关中断来保证原子操作
	++(sem->value);
	if(!list_empty(&(sem->waiters))){
		thread_unblock( (struct task_struct*)((unsigned int)list_pop(&(sem->waiters)) & 0xfffff000) );
	}
	intr_set_status(old);
}






void lock_init(struct lock* lock){
    lock->holder = NULL;
	semaphore_init(&(lock->semaphore), 1);
	lock->holder_repeat_num = 0;
}

void lock_acquire(struct lock* lock){
	unsigned int esp; 
    asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程
    if(lock->holder != cur){                                          //如果锁已经被占用，则线程会将自己阻塞。
                                                                      //可以改成自旋锁：循环等待while(lock->holder!= NULL);，不阻塞
    	semaphore_down(&(lock->semaphore));		                      
    	lock->holder = cur;
    	lock->holder_repeat_num = 1;	
    }
    else                                                              //锁的持有者重复获取锁
    	++(lock->holder_repeat_num);
}

void lock_release(struct lock* lock){
	unsigned int esp; 
    asm ("mov %%esp,  %0" : "=g"(esp)); 
	struct task_struct* cur = (struct task_struct*)(esp & 0xfffff000);//获取当前正在运行的线程
    if(lock->holder != cur)
    	return;                                                       //释放锁的线程必须是其拥有者	
    if(lock->holder_repeat_num > 1){
    	--(lock->holder_repeat_num);		
    	return;
    }                                                                 //有几次lock_acquire就应该有几次lock_release
    lock->holder = NULL;                                              //释放锁的操作并不在关中断下进行，所以此句要放在semaphore_up()之前
    lock->holder_repeat_num = 0;
    semaphore_up(&(lock->semaphore));   
}

