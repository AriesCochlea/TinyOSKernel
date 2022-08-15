#!/bin/sh



echo "Compiling..."                 #mbr和loader不用链接，详情请见源代码
nasm -o mbr.bin     boot/mbr.asm
nasm -o loader.bin  boot/loader.asm

#汇编，不链接。nasm默认格式bin是纯二进制可执行文件，加载到内存后能直接被CPU执行；elf和pe格式的二进制可执行文件包含了格式头，需要解析格式头找到程序入口地址
nasm -f elf -o putchar.o   kernel/lib/putchar.asm
nasm -f elf -o intrrpt.o   kernel/lib/interrupt.asm
nasm -f elf -o switchTo.o  kernel/lib/switchTo.asm


# 编译、汇编，不链接，-m32指生成32位目标文件：
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o main.o        kernel/main.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o init.o        kernel/init.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o sync.o        kernel/sync.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o thread.o      kernel/thread.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o memory.o      kernel/memory.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o list.o        kernel/list.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o print.o       kernel/print.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o interrupt.o   kernel/interrupt.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o keyboard.o    kernel/keyboard.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o str.o         kernel/lib/str.c
gcc -m32 -c -fno-stack-protector -I ./kernel/include/ -o bitmap.o      kernel/lib/bitmap.c


# -m elf_i386按32位进行链接，-Ttext 0xc0001500表示内核映像入口的虚拟地址，-e main表示入口符号：
#ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel.bin   main.o init.o  thread.o memory.o list.o print.o interrupt.o keyboard.o\
#                                                             str.o bitmap.o putchar.o intrrpt.o switchTo.o
ld -m elf_i386 -Ttext 0xc0001500 -e main -o kernel.bin   main.o init.o  print.o interrupt.o keyboard.o\
                                                                putchar.o intrrpt.o


rm -f *.o



