#pragma once
//注意：CPU进入中断后会自动把标志寄存器的IF位置0即屏蔽可屏蔽中断（也就是我们常说的关中断cli）



//初始化32位保护模式下的中断描述符表，本项目主要关注时钟中断（0x20）和键盘中断（0x21）：
void idt_init(); 



enum intr_status {
    INTR_OFF,
    INTR_ON
};


enum intr_status intr_get_status(); 
enum intr_status intr_set_status(enum intr_status);//返回旧的enum intr_status
enum intr_status intr_enable();                    //返回旧的enum intr_status
enum intr_status intr_disable();                   //返回旧的enum intr_status

