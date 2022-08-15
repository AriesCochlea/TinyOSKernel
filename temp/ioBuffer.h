/*此处正是经典的“生产者-消费者”模型：
	1.从缓冲区取数据或往缓冲区增加数据时都需要加锁。（我们的互斥锁也是基于二元信号量实现的）
	2.缓冲区空时不能再取数据，需要等待（可以用条件变量（内置等待队列）实现，也可以用信号量实现，此处取后者，因为我们之前已经实现了信号量（内置等待队列））；
      缓冲区满时不能再加数据，需要等待，也用信号量实现。
*/

//在本项目中主要用于实现键盘输入缓冲区：内核级的缓冲区，不是用户级的，也不是每个线程在其内核栈（在本项目中才不到4KB）中都有一个，而是所有线程的共享内存
//生产者是键盘驱动，消费者是将来实现的shell


#pragma once
#include "thread.h"
#include "sync.h"

#define bufsize 64

struct ioqueue{
    struct lock lock;
    struct task_struct* consumer;
    //struct task_struct* producer;
    char buf[bufsize];
    uint32_t head;			//头部读入数据
    uint32_t tail;			//尾部拿数据
};

void init_ioqueue(struct ioqueue* ioq);
uint32_t next_pos(uint32_t pos);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
void ioq_wait(struct task_struct** waiter);
void wakeup(struct task_struct** waiter); 
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char chr);



//用循环队列实现的缓冲区：jdlkasj



