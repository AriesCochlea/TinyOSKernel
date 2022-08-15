#pragma once

//void print_init();

void put_c(char c);                //汇编版本的put_c
void put_char(char c);             //以下所有的打印函数都是基于put_char实现



void put_str(char* str);           //打印字符串

void put_uint(unsigned int num);   //打印无符号整数

void put_int(int num);             //打印有符号整数

void put_16uint(unsigned int num); //按16进制打印无符号整数
