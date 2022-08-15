#include "print.h"
#include "interrupt.h"
/*#include "sync.h"

struct lock locker;

void print_init(){
	lock_init(&locker);
}
*/



void put_char(char c){
	//lock_acquire(&locker);
    enum intr_status old_status = intr_disable();
	put_c(c);
    intr_set_status(old_status); 
	//lock_release(&locker);
}





void put_str(char* str){
	//lock_acquire(&locker);
    enum intr_status old_status = intr_disable();
	for(unsigned int i = 0; str[i]!='\0'; ++i)
		put_char(str[i]);
    intr_set_status(old_status); 
	//lock_release(&locker);
}



void put_uint(unsigned int num){ //打印无符号整数————按十进制打印
	//lock_acquire(&locker);
    enum intr_status old_status = intr_disable();
	char c[10]={0,0,0,0,0,0,0,0,0,0};
	int i = 9;
	unsigned int temp = num;
	while(i>=0){
		c[i] = temp%10 + '0';    //num为0时也能打印
		temp = (temp - temp%10)/10 ;
		if(temp == 0)
			break;
		--i;
	}
	for(int j=0; j<10; ++j){
		if(c[j]==0)
			continue;
		put_char(c[j]);
	}
    intr_set_status(old_status); 
	//lock_release(&locker);
}


void put_int(int num){          //有符号整数：借用无符号整数进行打印
	//lock_acquire(&locker);
    enum intr_status old_status = intr_disable();
	if(num < 0){
		num = -num;
		put_char('-');
		put_uint ( num );
	}
	else
		put_uint ( num );
    intr_set_status(old_status); 
	//lock_release(&locker);
} 






void put_16uint(unsigned int num){
	//lock_acquire(&locker);
    enum intr_status old_status = intr_disable();
	char c[10]={0,0,0,0,0,0,0,0,0,0};
	int i = 9;
	unsigned int temp = num;
	while(i>=0){
		c[i] = temp%16 + '0';    //num为0时也能打印
		temp = (temp - temp%16)/16 ;
		if(temp == 0)
			break;
		--i;
	}
	for(int j=0; j<10; ++j){
		if(c[j]==0)
			continue;
		put_char(c[j]);
	}
    intr_set_status(old_status); 
	//lock_release(&locker);
}
