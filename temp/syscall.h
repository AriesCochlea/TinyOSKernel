#pragma once


enum SYSCALL_NR{           //在这里添加新的子功能号
    SYS_GETPID
};



void syscall_init();
unsigned int get_pid();
