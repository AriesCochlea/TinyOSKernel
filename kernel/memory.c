/*32位虚拟地址的转换过程：
 高10位用于在页目录表中索引PDE：高10位乘以4（每个PDE占4字节）并加上页目录表的物理基址，在该PDE中获取其保存的页表的物理基址（20位左移12位即乘以4K得到32位）
 中10位用于在页表中索引PTE：中10位乘以4（每个PTE占4字节）并加上页表的物理基址，在该PTE中获取其保存的普通物理页的物理基址（20位左移12位即乘以4K得到32位）
 低12位是在物理页（4KB）内的偏移量，直接加上物理页的物理基址，便得到32位虚拟地址最终对应的32位物理地址
*/
#define PG_P_1 1     //此页已存在于内存
#define PG_P_0 0     //此页不存在于内存，在本项目中不使用
#define PG_RW_R 0    //读、执行
#define PG_RW_W 2    //读、写、执行
#define PG_US_S 0    //系统级
#define PG_US_U 4    //用户级
#define PG_SIZE 4096 //4KB页
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22) //用于获取虚拟地址addr的高10位
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12) //用于获取虚拟地址addr的中10位



#include "memory.h"
#include "str.h"
#define NULL 0

struct pool{
    struct bitmap pool_bitmap;      //物理内存池用到的位图
    unsigned int phy_addr_start;    //物理内存池的起始物理地址
};

struct pool  kernel_pool, user_pool;         //全局的内核物理内存池、用户物理内存池  ————只有两个
struct virtual_addr kernel_vaddr;            //全局的内核虚拟内存池                 ————只有一个，所有进程共享虚拟内核空间（便于在实现系统调用）
//struct virtual_addr user_vaddr;            //每个用户进程都有一个用户虚拟内存池    ————不在此处实现，而在PCB的task struct中实现 
                                             //所有用户进程共享一个内核虚拟内存池
//内核进程只有虚拟内核空间（当然不会跑到用户虚拟空间去运行），用户进程兼有虚拟用户空间和虚拟内核空间（因为中断和系统调用会陷入0特权级的内核态）。
//从严格意义上讲，Linux的“进程”只有3种：内核线程、用户进程、用户线程。所有内核线程共用一套页目录表和页表————物理地址0x100000（虚拟地址0xc0100000）。
//所有的内核“进程”的内核空间都一样（因为不需要访问用户态空间），而且进程切换的开销太大，所以不需要所谓的内核进程。我们平常见到的系统进程其实处于用户态。
//我们以后实现的进程特指用户进程，且其task struct中含有页目录表的指针和用户态虚拟内存池。





//all_mem是虚拟（物理）地址0xb00（见boot/loader.asm第111行）存储的检测到的可用内存32MB
void mem_pool_init(unsigned int all_mem){    
    //页目录表（占1个4KB页大小）和1GB-4MB内核空间需要的页表（占255个页大小）所占的物理内存大小
    //第0（不重复计算）和第768个页目录项都指向第0个页表，最后一个（不增加页表数）指向页目录表自身
    //第769～1022个页目录项共指向254个页表，故目前共需要1个页目录表和255个页表（都是4KB页）
    unsigned int page_table_size = PG_SIZE * 256;         //4KB*256=1MB
    unsigned int used_mem = page_table_size + 0x100000;   //0x200000
    unsigned int free_mem = all_mem - used_mem;           //以字节为单位
    unsigned short all_free_pages = free_mem / PG_SIZE;   //空余的页数 = 总空余内存/4KB页 ，不足一页的内存直接丢弃（按整数边界对齐）
    unsigned short kernel_free_pages = all_free_pages /2; //内核物理空间与用户物理空间平分剩余物理内存
    unsigned short user_free_pages = all_free_pages - kernel_free_pages; 
    
        
    kernel_pool.phy_addr_start = used_mem;
    user_pool.phy_addr_start = used_mem + kernel_free_pages * PG_SIZE;
    unsigned int kbm_length = kernel_free_pages / 8;      //每个bit对应一个4KB自然页，长度以字节为单位故除以8
    unsigned int ubm_length = user_free_pages / 8; 
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length; 
    kernel_pool.pool_bitmap.bits =(void*)MEM_BITMAP_BASE;//0Xc009a000，在低端1MB内存以内
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);                 //两个bitmap紧挨着，都在低端1MB内存以内
    bitmap_init(&kernel_pool.pool_bitmap);               //将位图初始化为全0，即全部空闲
    bitmap_init(&user_pool.pool_bitmap);                 //将位图初始化为全0，即全部空闲
    
    
    kernel_vaddr.vaddr_start = K_HEAP_START;             //0xc0100000 
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len =kbm_length;//虚拟内存也按1:1比例分配
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);//三个bitmap紧挨着，都在低端1MB内存以内
    bitmap_init(&kernel_vaddr.vaddr_bitmap);             //将位图初始化为全0，即全部空闲
}





//在pf表示的虚拟内存池中申请pg_cnt个连续的虚拟页，成功则返回虚拟页的起始地址，失败则返回NULL
void* vaddr_get(enum pool_flags pf, unsigned int pg_cnt){
    int vaddr_start = 0, bit_idx_start = -1;
    unsigned int cnt = 0;
    if(pf == PF_KERNEL){//内核虚拟内存池
    	bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
    	if(bit_idx_start == -1)	
    		return NULL;
    	vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else{/*             //用户虚拟内存池：TODO
    	*/}
    return (void*)vaddr_start;
}




//在m_pool指向的物理内存池中分配1个物理页，成功则返回页框的物理地址，失败返回NULL
void* palloc(struct pool* m_pool){
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if(bit_idx == -1)
    	return NULL;
    unsigned int page_phyaddr = bit_idx * PG_SIZE + m_pool->phy_addr_start;
    return (void*)page_phyaddr;
}




//最后一个页目录项里写的是页目录表自己的物理地址（详见boot/loader.asm第316行），可以通过此页目录项编辑页目录表和页表：
unsigned int* pde_ptr(unsigned int vaddr){//获取虚拟地址vaddr对应的pde指针，vaddr可以是己经分配、在页表中存在的，也可以是尚未分配、页表中不存在的
    unsigned int* pde = (unsigned int*) (0xfffff000 + PDE_IDX(vaddr) * 4);
    return pde;
}//0xfffff000 的高 10 位是 0x3ff，中间 10 位也是 0x3ff
unsigned int* pte_ptr(unsigned int vaddr){//获取虚拟地址vaddr对应的pte指针，vaddr可以是己经分配、在页表中存在的，也可以是尚未分配、页表中不存在的 
    unsigned int* pte = (unsigned int*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}//1023 换算成十六进制是 0x3ff，将其移到高 10 位后，变成 0xffc00000

//页表中添加虚拟地址到物理地址的映射
void page_table_add(void* _vaddr, void* _page_phyaddr){
    unsigned int vaddr = (unsigned int)_vaddr, page_phyaddr = (unsigned int)_page_phyaddr;
    unsigned int* pde = pde_ptr(vaddr);
    unsigned int* pte = pte_ptr(vaddr);
    
    if(*pde & 0x00000001) {                                             //页目录项PDE的第0位P为1，表示此PDE已存在于内存中
    	if(!(*pte & 0x00000001))                                        //页表项PTE的第0位P为0，表示此PTE不存在于内存中，此步是多余的，保险起见
    	    *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;         
    	else                                                           
    	    *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;
    } 
    else{                                                               //PDE的第0位P为0，表示此PDE不存在于内存中
    	unsigned int pde_phyaddr = (unsigned int)palloc(&kernel_pool);  //将分配的页表的起始地址放进此PDE里
    	*pde = pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1;                //将此PDE的第0位置1
    	mem_set((void*)((unsigned int)pte & 0xfffff000), 0, PG_SIZE); 
    	if(!(*pte & 0x00000001))                                        //页表项PTE的第0位P为0，表示此PTE不存在于内存中，此步是多余的，保险起见
    	    *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;           
    	else                                                          
    	    *pte = page_phyaddr | PG_US_U | PG_RW_W | PG_P_1;
    }
}















void mem_init(){
    unsigned int mem_bytes_total = *(unsigned int*)(0xb00); //虚拟地址0xb00（见boot/loader.asm）存储的是检测到的可用内存32MB
    mem_pool_init(mem_bytes_total);
}




//(1)通过vaddr_get在虚拟内存池中申请虚拟地址
//(2)通过palloc在物理内存池中申请物理页
//(3)通过page_table_add将以上两步得到的虚拟地址和物理地址在页表中完成映射
void* malloc_page(enum pool_flags pf, unsigned int pg_cnt){
    if(pg_cnt<=0||pg_cnt>=3840)//内核和用户空间各15MB：15MB/4KB = 3840 ————因为低端1MB和页表已经占了空间
    	return NULL;
    	
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if(vaddr_start == NULL)	
    	return NULL;
    
    
    unsigned int vaddr = (unsigned int)vaddr_start;
    unsigned int cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    while(cnt > 0){
    	void* page_phyaddr = palloc(mem_pool);       //物理页可以不连续
    	if(page_phyaddr == NULL)
    		return NULL;
    	page_table_add((void*)vaddr, page_phyaddr);
    	vaddr += PG_SIZE;                            //虚拟页要连续
    	--cnt;
    }
    return vaddr_start;
}




void* get_kernel_pages(unsigned int pg_cnt){
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if(vaddr != NULL)
    	mem_set(vaddr, 0, pg_cnt*PG_SIZE);
    return vaddr;
}

