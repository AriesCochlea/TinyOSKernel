#include "bitmap.h"
#include "str.h"

void bitmap_init(struct bitmap* btmp){
	mem_set(btmp->bits, 0, btmp->btmp_bytes_len);
}

//判断bit_idx位是否为1，若为1返回true，否则返回false
int bitmap_scan_test(struct bitmap* btmp, unsigned int bit_idx){
	unsigned int byte_idx = bit_idx/8;
    unsigned int byte_odd = bit_idx%8;
    if(btmp->bits[byte_idx] & (1 << byte_odd))
    	return 1;
    else 
    	return 0;	
}



//将位图第bit_idx位起连续cnt个位设置为value
void bitmap_set(struct bitmap* btmp, unsigned int bit_idx, unsigned int cnt, unsigned char value){
	unsigned int byte_idx;
    unsigned int byte_odd;
	for(unsigned int i=0; i<cnt; ++i){
		byte_idx = (bit_idx+i)/8;
    	byte_odd = (bit_idx+i)%8;
    	if(value == 1)
   			btmp->bits[byte_idx] |= (1 << byte_odd);
   		else if(value == 0)
   			btmp->bits[byte_idx] &= ~(1 << byte_odd);
	}
}









int  bitmap_scan(struct bitmap* btmp, unsigned int cnt){
	for(unsigned int i=0; i< btmp->btmp_bytes_len*8; ++i){
		unsigned int j;
		for(j=i; j<i+cnt && j<btmp->btmp_bytes_len*8; ++j){
			if(bitmap_scan_test(btmp,j)==1)
				break;
		}
		if(j==i+cnt){                 //申请连续的cnt个空闲位并将其置1
			bitmap_set(btmp,i,cnt,1); 
			return i;
		}
	}
	return -1;
}

