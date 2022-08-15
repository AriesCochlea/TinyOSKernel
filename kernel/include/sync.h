#pragma once
#include "list.h"
#include "thread.h"


//信号量
struct semaphore {
    unsigned int value;
    struct list waiters;            //信号量的等待队列，暂时无法获取信号量（value==0）的线程将被放在里面
};

void semaphore_init(struct semaphore* sem, unsigned int value);
void semaphore_down(struct semaphore* sem);
void semaphore_up(struct semaphore* sem);



//互斥锁
struct lock {
    struct task_struct* holder;     //持有锁的当前线程。因为释放锁的线程必须是其拥有者，而执行semaphore_down的线程和执行semaphore_up的线程可以不是同一个
    struct semaphore semaphore;     //用二元信号量实现互斥锁
    unsigned int holder_repeat_num; //应对锁的持有者多次获取锁或多次释放锁的情况，否则自己等待自己释放锁，会造成死锁
};

void lock_init(struct lock* lock);
void lock_acquire(struct lock* lock);
void lock_release(struct lock* lock);



