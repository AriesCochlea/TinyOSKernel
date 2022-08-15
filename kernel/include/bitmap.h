#pragma once

struct bitmap{
    unsigned char *bits;          //机器的可用内存大小不能在编译期确定，所以不能用静态数组。C99的变长数组是分配在堆空间，而我们还没实现动态分配堆内存
    unsigned int btmp_bytes_len;
};


void bitmap_init(struct bitmap* btmp);                             //将位图初始化为全0，即全部空闲
int  bitmap_scan(struct bitmap* btmp, unsigned int cnt);           //在位图中申请连续的cnt个空闲位并将它们置1，若成功返回其起始下标，若失败返回-1

