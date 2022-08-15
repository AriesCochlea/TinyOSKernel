;在在文本模式80*25下的显存可以显示80*25=2000个字符，每个字符占2宇节，低宇节是字符的 ASCII 码，高字节是前景色和背景色属性
;在默认的80*25模式下，每行80个字符共25行，屏幕上可以容纳2000个字符。第O行的所有字符坐标是0～24，以此类推，最后一行的所有字符坐标是1975～ 1999 
;由于一个字符占用2宇节，所以光标坐标乘以2后才是字符在显存中的地址
;光标的坐标位置是存放在光标坐标寄存器中的，当我们在屏幕上写入一个字符时，光标的坐标并不会自动加1，需要人为修改


;选择子的属性：
TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0   ;显存段选择子


[bits 32]


section .data  

section .text

global put_c

;在汇编代码中导出符号供外部引用是用的关键字 global ，引用外部文件的符号是用的关键宇extern 
;在C代码中只要将符号定义为全局便可以被外部引用，引用外部符号时用 extern 声明即可



; 字符打印函数
put_c:
    pushad                     ;push所有以e开头的32位通用寄存器：EAX->ECX->EDX->EBX->ESP->EBP->ESI->EDI
    mov ax, SELECTOR_VIDEO
    mov gs, ax
                               ;获取当前光标位置：
    mov dx, 0x03d4             
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5             ;通过读写数据端口寄存器0x03d5来获得或设置光标位置
    in al, dx                  ;对于in指令，如果源操作数是8位寄存器，目的操作数必须是al；如果源操作数是16位寄存器，目的操作数必须是ax
    mov ah, al                 ;得到光标位置的高8位

    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx                  ;得到光标位置的低8位

    mov bx, ax                 ;bx存储光标的16位坐标值
    mov ecx, [esp + 36]        ;pushad压入了4*8 = 32字节，加上主调函数4字节的返回地址，又因为栈是往低地址方向扩展，所以是加36。
                               ;调用put_c函数前先往栈中压入参数，然后call指令会压入主调函数4字节的返回地址，而后被调函数又pushad了32字节
                               ;这个过程是基于call和ret指令的原理，和高级语言调用函数的压栈过程是不同的，有细节上的差异。
                   
    cmp cl, 0x0                ;空字符'\0'
    je .set_cursor         
    cmp cl, 0x8                ;退格0x8
    jz .is_backspace                     
    cmp cl, 0xd                ;回车0xd
    jz .is_carriage_return     
    cmp cl, 0xa                ;换行0xa
    jz .is_line_feed          
    
    jmp .put_other



.put_other:
    shl bx, 1
    mov [gs:bx], cl
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1
    inc bx
    cmp bx, 2000
    jl .set_cursor             ;小于2000（不超过一页）就跳转



.is_backspace:
	cmp bx,0                   ;如果bx == 0就不能退格了
    je  .set_cursor              
    dec bx                     ;光标位置退1
    shl bx, 1                  ;光标坐标乘以2就是字符位置

    mov byte [gs:bx], 0x00     ;'\0' 
    inc bx
    mov byte [gs:bx], 0x07     ;前景色和背景色：黑屏白字
    shr bx, 1                  ;除以2（取整）得到光标坐标
    jmp .set_cursor            ;将光标位置设为bx的值



.is_carriage_return:           ;本项目中'\n'和'\r'的效果都是换行，若出现"\r\n"则换行两次
.is_line_feed:
    xor dx, dx                 ;dx被除数高16位清零
    mov ax, bx                 ;ax是被除数低16位，也清零
    mov si, 80                 ;每行80个字符（0～79）
    div si                     ;ax÷80=bx÷80，商放在ax，余数放在dx
    sub bx, dx                 ;先回车：bx指向该行行首
.is_carriage_return_end:
    add bx, 80                 ;再换行：bx指向下一行行首
    cmp bx, 2000
.is_line_feed_end:
    jl .set_cursor             ;'\n'或'\r'时超过2000就直接就直接执行下面的.roll_screeen





.roll_screeen:                 ;滚屏（翻页）
    cld                        ;将标志寄存器Flag的方向标志位DF置0即向高地址处扩展
    mov ecx, 960               ;搬运(2000-80)*2=3840字节，每次搬4字节，需要搬960次
    mov esi, 0xc00b80a0        ;初始值为第1行行首
    mov edi, 0xc00b8000        ;初始值为第0行行首
    rep movsd                  ;movsd将数据从esi指向的内存位置复制到edi指向的内存位置，并使esi和edi自动增加（增加的方向由方向标志位DF确定）

    mov ebx, 3840              ;最后一行
    mov ecx, 80
.cls:
    mov word [gs:ebx], 0x0700  ;全改为黑底白字的'\0'
    add ebx, 2
    loop .cls
    mov bx, 1920               ;将光标重置为最后一行的行首



.set_cursor:
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh                 ;光标位置的高8位设成bx的高8位
    out dx, al

    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl                 ;光标位置的低8位设成bx的低8位
    out dx, al
.put_char_done:
    popad                      ;pop所有以e开头的32位通用寄存器：EAX->ECX->EDX->EBX->ESP->EBP->ESI->EDI
    ret
