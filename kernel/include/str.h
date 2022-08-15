#pragma once

void mem_set(void* dest, unsigned char value, unsigned int size);
void mem_cpy(void* dest, const void* src, unsigned int size);
int mem_cmp(const void* a, const void* b,  unsigned int size);
char* str_cpy(char* dest, const char* src);
unsigned int str_len(const char* str);
int str_cmp(const char* a, const char* b);
char* str_chr(const char* str, const char ch);
char* str_rchr(const char* str, const char ch);
char* str_cat(char* dest, const char* src);
unsigned int str_chrs(const char* str, const char ch); //在字符串str中查找字符ch出现的次数


