[bits 32]
section .text
global switch_to        ;供thread.c的schedule()调用
switch_to:
    push esi            ;这里是根据Intel386硬件体系的ABI规定，保存5个寄存器：其中esp改用手动处理，存储在cur->self_kstack中
    push edi
    push ebx
    push ebp
    mov eax, [esp+20]   ;上面的4个寄存器，加上主调函数里的call指令压入的eip寄存器，共5个，所以[esp+20]可以获得主调函数schedule()压入栈中的第一个参数cur
    mov [eax], esp      ;保存栈顶esp到cur->self_kstack。当前线程的上下文环境算是保存完毕了
    
    
;-----以上是备份当前线程的环境，下面是恢复下一个线程的环境-----
    
    
    mov eax, [esp+24]   ;获得第二个参数next
    mov esp, [eax]      ;将栈顶esp赋成next->self_kstack
    pop ebp             ;注意：这里的ebp（属于线程next）和上面的ebp（属于线程cur）不相同，因为esp已经变了，所以pop出来的值不是之前push的。下同
    pop ebx
    pop edi
    pop esi
    ret                 ;函数调用约定为：由主调函数清理主调函数压入栈中的实参


