#pragma once
#include "bitmap.h"
//前言：
//MBR被BIOS加载到物理地址0x7c00，本项目的loader被MBR加载到物理地址0x900（位于0x500～0x7c00这一片30KB可用区域，放不下我们的内核elf（70KB左右））
//本项目的内核elf被loader加载到物理地址0x70000（位于0x7e00～0x9fc00这一片608KB可用区域），两片可用区域之间隔着512字节的MBR
//本项目的内核映像的main入口物理地址为0x1500，内核映像可以向上覆盖掉MBR（只要不超过0x70000覆盖内核elf就行）
//页目录表和页表放在低端物理内存0～1MB以外，即从0x100000开始。而位图放在低端1MB内存以内，就不用额外考虑它所占用的内存，因为低端1MB内存肯定是被占用的
#define MEM_BITMAP_BASE 0Xc009a000   //内核位图起始虚拟地址，对应的物理地址为0x9a000，减去0x70000得到168KB，在这之间的区域完全足够放下我们的内核elf
                                     //内核初始栈顶虚拟地址为0xc009f000，物理地址0x9f000，是最靠近可用区域上限0x9fc00的，见boot/loader.asm第285行
                                     //将来会在0x9e000处安排内核主进程的PCB或TCB（本项目中PCB或TCB只占用一个自然页），0x9e000到0x9f000刚好是4KB
                                     //主进程（线程）的PCB放在物理低端1MB以内，而其他进程或线程的PCB或TCB创建后放在物理低端1MB以外的广阔空间
                                     //主进程（线程）的初始栈顶0x9f000（虚拟地址0xc009f000）是本项目在低端1MB所用到的最高物理地址
                           
                                     
#define K_HEAP_START    0xc0100000   //目前内核可用的堆空间的起始虚拟地址：0xc0100000～0xffbfffff ————从0xffc00000开始的空间都被第1023个PDE浪费了
                                     //目前用户可用的堆空间的虚拟地址为：    0x00100000～0xbfffffff ————这是对4GB内存而言
/*之前已完成的虚拟地址到物理地址的映射：详见loader.asm
0x00000000-0x000fffff -> 0x000000000000-0x0000000fffff //第0个PDE指向第0个页表，而第0个页表中（共1024个PTE）的256个PTE分配给了物理低端1MB
0xc0000000-0xc00fffff -> 0x000000000000-0x0000000fffff //第768个PDE指向第0个页表，而第0个页表中（共1024个PTE）的256个PTE分配给了物理低端1MB
0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff //第1023个PDE指向页目录表自身（0x100000），被当成页表，利用虚拟地址中间10位索引到第0个PTE
0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff //第1023个PDE指向页目录表自身，被当成页表，利用虚拟地址中间10位索引到第768～1022共255个PTE
0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff //第1023个PDE指向页目录表自身（0x100000），被当成页表，利用虚拟地址中间10位索引到第1023个PTE
*/



enum pool_flags{
    PF_KERNEL = 1, //内核内存池
    PF_USER = 2    //用户内存池
};
/*
struct bitmap{                      //#include "bitmap.h"
    unsigned char *bits;            //每个bit对应一个4KB自然页（按整数边界对齐）
    unsigned int btmp_bytes_len;
}; 
*/

struct virtual_addr{
    struct bitmap vaddr_bitmap;     //虚拟内存池用到的位图
    unsigned int vaddr_start;       //虚拟内存池的起始虚拟地址
};







     
void mem_init();                                           //初始化内存池：对kernel_vaddr, kernel_pool, user_pool这3个结构进行初始化
void* malloc_page(enum pool_flags pf, unsigned int pg_cnt);//分配pg_cnt个页（物理页可以不连续，虚拟页要连续），成功则返回起始虚拟地址，失败返回NULL
void* get_kernel_pages(unsigned int pg_cnt);               //malloc_page(PF_KERNEL, pg_cnt)并清零虚拟页（对应的物理页也会清零）


