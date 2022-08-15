/*
AT&T汇编的源操作数在左，目的操作数在右，寄存器前要加%，立即数前要加$（不加的话就被直接当成内存地址）
Intel汇编的操作数类型修饰符为byte,word,dword,qword，                                如：mov dword [0x1234],eax
AT&T汇编指令的最后一个字母表示操作数的大小，b表示8位，w表示16位，l表示32位，q表示64位，如：movl %eax, 0x1234
Intel汇编的远跳转、远调用、远返回是jmp far, call far, ret far
AT&T汇编的远跳转、远调用、远返回是ljmp, lcall, lret
*/

/*基本内联汇编：寄存器前缀是%
asm [volatile] ("code");若要引用C变量，只能将它定义为全局变量，如果定义为局部变量，链接时会找不到这个符号
asm写成__asm__也行，volatile是可选项，写成__volatile__也行，表示禁止编译器优化：对应的代码不会被优化，而是原样保留
"code"换行时需要加"\"，多条语句之间用';'或'\n'或'\r\n'分隔：
  "code1; \
   code2 \
  "
  
  
扩展内联汇编：寄存器前缀是%%，为了区别占位符和寄存器，所以要两个%
asm [volatile] ("code" : output : input : clobber/modify); //由“:”分成4部分，每一部分都能省略
     output和input中每个操作数的格式为：“[操作数修饰符]约束名”(C变量名)   ————[]表示可选，C变量也可以是立即数
     clobber/modify ：汇编代码执行后会破坏一些内存或寄存器，通过此项通知编译器，这样gcc就知道哪些寄存器或内存需要提前保护起来
                      在output和input中出现过的寄存器不需要出现在clobber/modify中
                      用双引号把寄存器名称引起来，多个寄存器之间用逗号','分隔，这里的寄存器不用再加两个’%’，只写名称（eax/ax/al均可）即可
                      如果内联汇编代码修改了标志寄存器中的标志位，需要在clobber/modify中用"cc"声明
                      如果修改了内存，需要在clobber/modify中"memory"声明。如果被修改的内存并未出现在output和input中，就需要用"memory"告诉gcc
                      另外一个用"memory"声明的原因就是清除寄存器缓存。编译中有时会把内存中的数据优化到寄存器，编译过程中编译器无法检测到内存的变化

操作数修饰符：一般情况下， input 中的 C 变量是只读的， output 中的 C 变量是只写的。
在output中有以下3种：＝：表示该C变量是只写的    ：int a=1,b=2,sum; asm("addl %k0, %k1":"=a"(sum):"a"(a),"b"(b));
                    ＋：表示该C变量是先读再写的：int a=1,b=2;   asm("addl %%ebx, %%eax":"+a"(a):"b"(b));
                    ＆：表示该C变量独占所约束（分配）的寄存器。有多个修饰符时，＆要与约束名挨着，不能分隔。
在 input 中： %：该输入寄存器可以和下一个输入寄存器互换，比如addl的两个操作数可以交换顺序。


寄存器约束：汇编代码中操作的并不是C变量本身，而是C变量通过值传递到寄存器的副本
a ：表示寄存器 eax/ax/al
b ：表示寄存器 ebx/bx/bl
c ：表示寄存器 ecx/cx/cl
d ：表示寄存器 edx/dx/dl
D ：表示寄存器 edi/di
S ：表示寄存器 esi/si
q ：表示任意这4个通用寄存器之－：eax/ebx/ecx/edx
r ：表示任意这6个通用寄存器之一：eax/ebx/ecx/edx/esi/edi
g ：表示可以存放到任意地点（寄存器和内存）。相当于除了同q一样外，还可以让gcc安排在内存中
A ：把eax和edx组合成64位整数
f ：表示浮点寄存器
t ：表示第1个浮点寄存器
u ：表示第2个浮点寄存器

内存约束：直接将位于input和output中的C变量的内存地址作为内联汇编代码的操作数，不需要寄存器做中转，直接进行内存读写
m ：直接访问变量的地址
o ：通过偏移量的形式访问变量

立即数约束：在传值的时候不通过内存和寄存器，直接作为立即数传给汇编代码
i ：表示操作数为整数立即数
F ：表示操作数为浮点数立即数
I ：表示操作数为 0～ 31 之间的立即数
J ：表示操作数为 0～63 之间的立即数
N ：表示操作数为 0～255 之间的立即数
O ：表示操作数为 0～ 32 之间的立即数
X ：表示操作数为任何类型立即数

通用约束：
0～ 9 ：此约束只用在input部分，表示可与第n个操作数（可以在input或output中）用相同的寄存器或内存，以下的例子中eax既可用于输入，又可用于输出
如：int var=1, sum=0; asm("addl %k1, %k0": "=a"(sum), "I"(2), "0"(var)); //将var复制到寄存器eax，将eax加上立即数2，再将eax的值复制到sum


序号占位符：对在output和input中的操作数，按照它们从左到右出现的次序从0开始编号，操作数用在assembly code中，引用它的格式是%0～%9
           占位符指代约束所对应的操作数，也就是在汇编代码中的操作数，与圆括号中的C变量无关
           
名称占位符：用名称代替序号，解决只有10个序号可用的限制        ————占位符指代约束所对应的操作数，也就是在汇编代码中的操作数，与圆括号中的C变量无关
在input和output里：[名称占位符]“操作数修饰符约束名”(C变量名)
在assembly code中使用：%[名称占位符]
           
机器模式：可以放在序号占位符（或名称占位符）的%和序号之间
b -- 0～7位：如%b0
h -- 8～15位：如"%h[hello]":[hello]"m"(var)
w -- 0～15位：如ax
k -- 0～31位：如eax
*/


#pragma once




/* 向端口port写入一个字节*/
static inline void outb(unsigned short port, unsigned char data) {
   asm volatile ( "outb %b0, %w1" : : "a" (data), "Nd" (port));    //outb %al,%dx  //al里存data，dx里存端口号port
}
/* 将从端口port读到的一个字节返回 */
static inline unsigned char inb(unsigned short port) {
   unsigned char data;
   asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
   return data;
}




/* 将addr处起始的word_cnt个字（不是字节）写入端口port */
static inline void outsw(unsigned short port, const void* addr, unsigned int word_cnt) {
   asm volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}
/* 将从端口port读到的word_cnt个字（不是字节）写入addr */
static inline void insw(unsigned short port, void* addr, unsigned int word_cnt) {
   asm volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}



