/*键盘是个独立的设备，在它内部有个叫作键盘编码器的芯片（独立的处理器，有自己的寄存器和内存），通常是 Intel 8048 或兼容芯片，
      它的作用是：每当键盘上发生按键操作，它就向键盘控制器报告哪个键被按下，按键是否弹起。
  键盘控制器并不在键盘内部，它在主机内部的主板的南桥芯片中，通常是 Intel 8042 或兼容芯片，相当于是8048的代理
      它的作用是接收来自键盘编码器的按键信息，将其解码后保存，向中断代理（如Intel 8259A）发中断，之后处理器执行相应的中断处理程序读入8042保存的按键信息。
  8048要和8042达成协议：规定了键盘上的每个物理键对应的唯一数值，说白了就是对键盘上所有的按键进行编码，为每个按键分配唯一的数字。
      一个键的状态要么是按下，要么是弹起，因此一个键便有两个编码（每次按键至少产生两次键盘中断），按键被按下时的编码叫通码makecode，
      按键在被按住不松手时会持续产生相同的码，直到按键被松开时才终止，因此按键被松开弹起时产生的编码叫断码breakcode
      当键的状态改变后，键盘中的8048芯片把按键对应的扫描码（通码或断码）发送到主板上的8042芯片，由8042处理后保存在自己的寄存器中，
      然后向8259A发送中断信号，这样处理器便去执行键盘中断处理程序，将8042处理过的扫描码从它的寄存器中读取出来，继续进行下一步处理。
  键盘中断处理程序由程序员负责编写，我们只能得到键的扫描码，并不会得到键的ASCII 码，扫描码是硬件提供的编码集，ASCII 是软件中约定的编码集。
      但我们可以将得到的“硬件”扫描码转换成对应的“软件”ASCII码，键盘的中断处理程序便充当了字符处理程序。
      比如，可以在中断处理程序中将空格的扫描码0x39转换成ASCII码0x20，然后将ASCII码0x20交给我们的put_char函数，将ASCII码写入显存。
  根据不同的编码方案，键盘扫描码有三套，分别称为 scan code set 1 、 scan code set 2 、 scan code set 3 。
      所以需要8042这个“中间层”，不管键盘实际使用的是哪套扫描码，都被8042转换成scan code set 1 。
*/


#include "keyboard.h"
#include "print.h"
#include "ioPort.h"



#define KBD_BUF_PORT 0X60 //键盘buffer寄存器端口号为0x60。每次按键后必须要读取输出缓冲区寄存器，否则8042芯片不再继续响应键盘中断


#define esc '\033'	      //八进制表示字符，也可以用十六进制'\x1b'
#define delete '\0177'    //八进制表示字符，也可以用十六进制'\x7f'
#define enter '\r'
#define tab '\t'
#define backspace '\b'

#define char_invisible 0  //以下不可见字符均设置为0，因为它们只有扫描码，没有ASCII码
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible 
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

#define shift_l_make 0x2a  
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define caps_lock_make 0x3a
//举两个断码的例子，本项目不会用到：
//#define ctrl_r_break 0xe09d  //断码和通码的区别在于第八位，可以互推。而断码和通码总是成对存在（断码的前面必有对应的通码）
//#define alt_r_break 0xe0b8   //右边的相同的键的扫描码都是0xe0开头


char keymap[][2] = {           //用通码作为数组的索引下标
/* 0x00 */	{0, 	0}, 	 	
/* 0x01 */	{esc, 	esc}, 		
/* 0x02 */	{'1', 	'!'}, 		
/* 0x03 */	{'2', 	'@'}, 		
/* 0x04 */	{'3', 	'#'}, 		
/* 0x05 */	{'4', 	'$'}, 		
/* 0x06 */	{'5', 	'%'}, 		
/* 0x07 */	{'6', 	'^'}, 		
/* 0x08 */	{'7', 	'&'}, 		
/* 0x09 */	{'8', 	'*'}, 		
/* 0x0A */	{'9', 	'('}, 		
/* 0x0B */	{'0', 	')'}, 		
/* 0x0C */	{'-', 	'_'}, 		
/* 0x0D */	{'=', 	'+'}, 		
/* 0x0E */	{backspace,  backspace}, 	
/* 0x0F */	{tab, 	tab}, 		
/* 0x10 */	{'q', 	'Q'}, 		
/* 0x11 */	{'w', 	'W'}, 		
/* 0x12 */	{'e', 	'E'}, 		
/* 0x13 */	{'r', 	'R'}, 		
/* 0x14 */	{'t', 	'T'}, 		
/* 0x15 */	{'y', 	'Y'}, 		
/* 0x16 */	{'u', 	'U'}, 		
/* 0x17 */	{'i', 	'I'}, 		
/* 0x18 */	{'o', 	'O'}, 		
/* 0x19 */	{'p', 	'P'}, 		
/* 0x1A */	{'[', 	'{'}, 		
/* 0x1B */	{']', 	'}'}, 		
/* 0x1C */	{enter,   enter}, 
/* 0x1D */	{ctrl_l_char,  ctrl_l_char}, 
/* 0x1E */	{'a', 	'A'}, 		
/* 0x1F */	{'s', 	'S'}, 		
/* 0x20 */	{'d', 	'D'}, 		
/* 0x21 */	{'f', 	'F'}, 		
/* 0x22 */	{'g', 	'G'}, 		
/* 0x23 */	{'h', 	'H'}, 		
/* 0x24 */	{'j', 	'J'}, 		
/* 0x25 */	{'k', 	'K'}, 		
/* 0x26 */	{'l', 	'L'}, 		
/* 0x27 */	{';', 	':'}, 		
/* 0x28 */	{'\'', 	'"'}, 		
/* 0x29 */	{'`', 	'~'}, 		
/* 0x2A */	{shift_l_char,  shift_l_char}, 	
/* 0x2B */	{'\\', 	'|'}, 		
/* 0x2C */	{'z', 	'Z'}, 		
/* 0x2D */	{'x', 	'X'}, 		
/* 0x2E */	{'c', 	'C'}, 		
/* 0x2F */	{'v', 	'V'}, 		
/* 0x30 */	{'b', 	'B'}, 		
/* 0x31 */	{'n', 	'N'}, 		
/* 0x32 */	{'m', 	'M'}, 		
/* 0x33 */	{',', 	'<'}, 		
/* 0x34 */	{'.', 	'>'}, 		
/* 0x35 */	{'/', 	'?'}, 
/* 0x36	*/	{shift_r_char,  shift_r_char}, 	
/* 0x37 */	{'*', 	'*'},     	
/* 0x38 */	{alt_l_char,  alt_l_char}, 
/* 0x39 */	{' ', 	' '}, 		
/* 0x3A */	{caps_lock_char,  caps_lock_char}
//其他按键暂不处理
};




//以下变量用于记录控制键是是否按下并且尚未松开（长按）：caps_lock键长按等同于单按的状态
int ctrl_status = 0, shift_status = 0, alt_status = 0, caps_lock_status = 0;  
//扫描码是否以0xe0开头：
int ext_scancode = 0;                       


void intr_keyboard_handler(){
    int break_code;                             //此次键盘中断的扫描码是否是断码
    unsigned short scancode = inb(KBD_BUF_PORT);//读取此次键盘中断的扫描码
    if(scancode == 0xe0){	                    //只要发现扫描码前缀为0xe0，就表示此键的扫描码多于一个字节
    	ext_scancode = 1;
    	return;
    }
    if(ext_scancode == 1){                      //如果上次扫描码以0xe0开头，则将扫描码合并
    	scancode |= 0xe000;
    	ext_scancode = 0;
    }
    if((scancode & 0x0080) != 0)                //断码的第八位（第7位）为1
    	break_code = 1;
    else
    	break_code = 0;   
   
    
    if(break_code==1){                          //对caps_lock键而言，即使弹起也不将caps_lock_status置0，必须等到下次按下caps_lock键才置0
    	unsigned short make_code = (scancode &= 0xff7f); //抹去第八位得到通码
    	if(make_code == ctrl_l_make || make_code == ctrl_r_make)
    		ctrl_status = 0;
    	else if(make_code == shift_l_make || make_code == shift_r_make)
    		shift_status = 0;
    	else if(make_code == alt_l_make || make_code == alt_r_make)
    		alt_status = 0;
    	return;                                 //如果是断码就直接结束此次键盘中断
    }
    else if((scancode > 0x00 && scancode < 0x3b) || (scancode == alt_r_make) || (scancode == ctrl_r_make)){//alt_r_make和ctrl_r_make是0xe0开头
    	int shift = 0;                          //访问keymap数组第0列还是第1列
    	if((scancode < 0x0e) || (scancode == 0x1a) || (scancode == 0x1b) ||
    	  (scancode == 0x27) || (scancode == 0x28) || (scancode == 0x29) ||
    	  (scancode == 0x2b) || (scancode == 0x33) || (scancode == 0x34) || (scancode == 0x35)){
    	    if(shift_status==1)                              //上述这些键只受shift键影响，不受caps_lock键影响
    	    	shift = 1;
    	}
    	else{                                                //字母键a-z或A-Z
    	    if(shift_status==1 && caps_lock_status==1)       //shift和caps_lock同时长按，访问keymap数组第0列
    	    	shift = 0; 
    	    else if(shift_status==1 || caps_lock_status==1)  //shift和caps_lock只长按其中一个，访问keymap数组第1列
    	    	shift = 1; 
    	    else                                             //shift和caps_lock都没按，访问keymap数组第0列
    	    	shift = 0;
    	}//目前暂不实现alt键的功能
    	
    	unsigned char index = scancode & 0x00ff;//主要因为是有的码以0xe0开头
        char cur_char = keymap[index][shift];
        if(cur_char!=0){
        	put_char(cur_char);             //打印可见字符，添加到键盘输入缓冲区
        	//？？加入到键盘输入缓冲区，'\b'有点不对劲
	    	return;
	    }                                       //处理非可见字符：
		else if(scancode == ctrl_l_make || scancode == ctrl_r_make)    	
		    ctrl_status = 1;
		else if(scancode == shift_l_make || scancode == shift_r_make)
            shift_status = 1;
		else if(scancode == alt_l_make || scancode == alt_r_make)
		    alt_status = 1;
		else if(scancode == caps_lock_make){
			if(caps_lock_status == 0)
				caps_lock_status = 1;
			else
				caps_lock_status = 0;
		}
	
    }
}







