#include "str.h"
#define NULL 0


void mem_set(void* dest, unsigned char value, unsigned int size){
	if(dest ==NULL)
		return;
    for(unsigned char* dst = (unsigned char*) dest; size>0; --size){
    	*dst = value;
    	++ dst;
    }
    return;
}


void mem_cpy(void* dest, const void* src, unsigned int size){
	if(dest==NULL || src==NULL)
		return;
    unsigned char* dst = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while(size > 0){
    	*dst = *s;
    	++dst;
    	++s;
    	++size;
    }
    return;
}


int mem_cmp(const void* a, const void* b,  unsigned int size){
	if(a==NULL && b==NULL)
		return 0;
	else if(a==NULL && b!=NULL)
		return -1;
	else if(a!=NULL && b==NULL)
		return 1;
		
    const char* _a = (const char*)a;
    const char* _b = (const char*)b;
    while(size > 0){
    	if(*_a != *_b)	
    		return (*_a > *_b) ? 1 : -1;
   		++_a;
   		++_b;
   		++size;
    }
    return 0;
}


char* str_cpy(char* dest, const char* src){
	if(dest==NULL || src==NULL)
		return dest;
    char* d = dest;
    while(*src!='\0'){
    	*d = *src;
    	++d;
    	++src;
    }
    *d='\0';
    return dest;     
}


unsigned int str_len(const char* str){
	if(str==NULL)
		return 0;
    unsigned int len=0;
    while(*str!='\0'){
    	++len;
    	++str;
    }
    return  len;  
}


int str_cmp(const char* a, const char* b){
	if(a==NULL && b==NULL)
		return 0;
	else if(a==NULL && b!=NULL)
		return -1;
	else if(a!=NULL && b==NULL)
		return 1;
    while(*a!='\0'&& *b!='\0' && *a == *b){
    	++a;
    	++b;
    }   
    if(*a == *b)
    	return 0;
    else
    	return (*a < *b) ? -1 : 1; 
}


char* str_chr(const char* str, const char ch){
	if(str==NULL)
		return NULL;
    while(*str!='\0'){
    	if(*str == ch)	
    		return (char*)str;
    	++str;
    } 
    return NULL;
}


char* str_rchr(const char* str, const char ch){
	if(str==NULL)
		return NULL;
    char* last = NULL;
    while(*str != '\0'){
    	if(ch == *str)	
    		last = (char*)str;
    	str++;
    }
    return last;   
}


char* str_cat(char* dest, const char* src){
	if(dest==NULL || src==NULL)
		return dest;
    char* str = dest;
    while(*str!='\0')
    	++str;
    while(*src!='\0'){
    	*str = *src;
    	++str;
    	++src;
    }	
    *str = '\0';
    return dest;
}


unsigned int str_chrs(const char* str, const char ch){
	unsigned int count = 0;
	while(*str != '\0'){
		if(ch == *str)
			++count;
		++str;
	}
    return count;
}

