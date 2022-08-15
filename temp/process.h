#pragma once
#define NULL ((void*)0)


//目前我们尚未实现文件系统，前期我们用普通函数代替用户程序



extern void intr_exit();

void start_process(void* filename);                           //构建用户进程初始上下文信息：保存在中断栈struct intr_stack中

void page_dir_activate(struct task_struct* thread);

void process_activate(struct task_struct* thread);

unsigned int* create_page_dir();

void create_user_vaddr_bitmap(struct task_struct* user_prog);

void process_execute(void* filename, char* name);



