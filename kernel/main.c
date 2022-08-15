#include "init.h"
#include "print.h"
#include "thread.h"  
#define NULL 0

//我们的入口函数的虚拟地址被固定为0xc0001500，故main函数前面不能有任何实质性的可执行代码
void thread1();
void thread2();
                         
                         
int main() {
    init_all(); 
    put_char('\b');      //用来测试空屏时左上角的'\b'
    put_str("Hee");
    put_char('\b');      //用来测试'\b'
    put_char('\0');      //用来测试空字符'\0'是否显示
    put_str("llo OS\n");
	put_str("version 1.0 : ");
	put_uint(2022);      //用来测试无符号整数
	put_int(-7);         //用来测试有符号整数
	put_int(-1);
	put_str("\n\n");
	

	
	//thread_start("thread1", 10, thread1, NULL);
	//thread_start("thread2", 10, thread2, NULL);
    while (1);
    return 0;
}
void thread1(){
	while(1)
		put_str("thread1\n");
}
void thread2(){
	while(1)
		put_str("thread2\n");
}
