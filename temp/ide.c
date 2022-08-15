//硬盘驱动程序

#include "ide.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "sync.h"
#include "ioPort.h"
#include "timer.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"


//定义硬盘各寄存器的端口号
#define reg_data(channel) 	  (channel->port_base + 0)
#define reg_error(channel) 	  (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel)	  (channel->port_base + 3)
#define reg_lba_m(channel)	  (channel->port_base + 4)
#define reg_lba_h(channel)	  (channel->port_base + 5)
#define reg_dev(channel)	  (channel->port_base + 6)
#define reg_status(channel)	  (channel->port_base + 7)
#define reg_cmd(channel)	  (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	  reg_alt_status(channel)
//reg_alt_status寄存器的一些关键位
#define BIT_STAT_BSY	  	  0X80		//硬盘忙
#define BIT_STAT_DRDY	  	  0X40		//驱动器准备好了
#define BIT_STAT_DRQ 	  	  0x8		//数据传输准备好了
//device寄存器的一些关键位
#define BIT_DEV_MBS		      0Xa0      //第7位和第5位固定为1		
#define BIT_DEV_LBA		      0X40
#define BIT_DEV_DEV		      0X10
//一些硬盘操作的指令
#define CMD_IDENTIFY		  0Xec		//identify指令
#define CMD_READ_SECTOR	      0X20		//读扇区指令
#define CMD_WRITE_SECTOR	  0X30		//写扇区指令

#define max_lba		  ((80*1024*1024/512) - 1) //定义可读写的最大扇区数，本项目只支持80MB硬盘


unsigned char channel_cnt;		        //全局变量：通道数 = 硬盘数/2 + 硬盘数%2
struct ide_channel channels[2];         //两个ide通道。个人PC一般都是两个ide通道的主板，但多个ide通道的服务器确实很普遍
unsigned int ext_lba_base = 0;	        //记录总拓展分区的起始LBA地址，partition_scan()时以此为标记
unsigned char p_no = 0, l_no = 0;	    //记录硬盘的主分区下标和逻辑分区下标
struct list partition_list;	            //所有分区放到一张链表中管理





void ide_init(){
    //物理地址0x475处存储着主机上安装的硬盘的数量，它是由BIOS检测并写入的
    unsigned char hd_cnt = *((unsigned char*)(0x475));                  //获取硬盘数量
    if(hd_cnt <= 0)
    	return;
    channel_cnt = hd_cnt/2 + hd_cnt % 2;                                //全局变量
    if(channel_cnt <= 0|| channel_cnt > 2)
    	return;
    	
//开始初始化ide通道：    
    struct ide_channel* channel;
    unsigned char channel_no = 0, dev_no = 0;
    list_init(&partition_list);
    while(channel_no < channel_cnt){
    	channel = &(channels[channel_no]);
    	switch(channel_no){
    	    case 0:
    		channel->port_base = 0x1f0;		//ide0的起始端口号为0x1f0
    		channel->irq_no    = 0x2e;		//从片8259A上倒数第二的中断引脚用于ideO通道的中断
    		break;
    	    case 1:
    		channel->port_base = 0x170;		//ide1的起始端口号为0x170
    		channel->irq_no    = 0x2f;      //从片8259A上倒数第一的中断引脚用于ide1通道的中断
    		break;
    	}
    	channel->expecting_intr = 0;	   //未向硬盘写入指令时不期待硬盘的中断
    	lock_init(&(channel->lock));
    	semaphore_init(&(channel->disk_done), 0);

//开始初始化硬盘：  	
    	while(dev_no < 2){                 //分别获取两个硬盘的参数和分区信息
    	    struct disk* hd = &(channel->devices[dev_no]);
    	    hd->my_channel = channel;
    	    hd->dev_no = dev_no;
    	    sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
    	    identify_disk(hd);
    	    if(dev_no != 0)                //内核本身的裸硬盘（ hd60M.img ）不处理
    	    	partition_scan(hd, 0);     //扫描该硬盘上的分区
    	    p_no = 0, l_no = 0;
    	    dev_no++;
    	}
    	dev_no = 0;                        //将硬盘驱动号置0，为下一个 channel 的两个硬盘初始化
    	channel_no++;                      //下一个 channel 
    }
    list_traversal(&partition_list, partition_info, (int)NULL);//打印所有分区信息
}









//选择要读写的硬盘是主盘还是从盘
void select_disk(struct disk* hd){
    unsigned char reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if(hd->dev_no == 1) //主盘0，从盘1
    	reg_device |= BIT_DEV_DEV;
    outb(reg_dev(hd->my_channel), reg_device);
}
//向硬盘hd提供要读写的起始扇区地址lba和要读写的扇区数sec_cnt
void select_sector(struct disk* hd, unsigned int lba, unsigned char sec_cnt){
	if(lba > amx_lba)
		return;
    struct ide_channel* channel = hd->my_channel;
    outb(reg_sect_cnt(channel), sec_cnt);
    outb(reg_lba_l(channel), lba);
    outb(reg_lba_m(channel), lba>>8);
    outb(reg_lba_h(channel), lba>>16);
    //因为LBA地址的第24～ 27位要存储在device寄存器的0～ 3位，无法单独写入这四位
    //所以在此处把device寄存器再重新写入一次，保留原来信息的同时，又补充了LBA的第24～27位
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}
//向通道channel发命令cmd：读、写、identify
void cmd_out(struct ide_channel* channel, unsigned char cmd){
    channel->expecting_intr = 1; //在intr_hd_handler()中重置为0
    outb(reg_cmd(channel), cmd);
}
//从硬盘读sec_cnt个扇区数据到buf
void read_from_sector(struct disk* hd, void* buf, unsigned char sec_cnt){
    unsigned int size_in_byte;
    if(sec_cnt == 0)             //unsigned char型的变量若为256则会将最高位的1丢掉变为0
    	size_in_byte = 256*512;  //如果读写的扇区数超过256，则要分多次读写，需要多次加锁、解锁，会多次发出中断
    else
    	size_in_byte = sec_cnt*512;
    insw(reg_data(hd->my_channel), buf, size_in_byte/2);
}
//将buf中的sec_cnt个扇区数据写入硬盘
void write_to_sector(struct disk* hd, void* buf, unsigned char sec_cnt){
    unsigned int size_in_byte;
    if(sec_cnt == 0)
    	size_in_byte = 256*512;
    else
    	size_in_byte = sec_cnt * 512;
    outsw(reg_data(hd->my_channel), buf, size_in_byte/2);
}
//最多等待31秒：因为ata手册中要求所有的操作都应该在31秒内完成
int busy_wait(struct disk* hd){
    struct ide_channel* channel = hd->my_channel;
    unsigned short time_limit = 31 * 1000;
    while(time_limit -= 10 >= 0){
    	if(!(inb(reg_status(channel)) & BIT_STAT_BSY)){  //判断status寄存器的BSY位是否为1，如果是，则表示硬盘繁忙
    	    if(inb(reg_status(channel)) & BIT_STAT_DRQ)  //如果硬盘不忙，则再次读取status寄存器，判断其DRQ位的值，DRQ位为1表示硬盘己经准备好数据了
    	    	return 1;
		}
    	else
    		mtime_sleep(10);                             //硬盘繁忙时调用mtime_sleep()休眠10毫秒
    	??
    }
    return 0;
}









//从硬盘lba地址开始读取sec_cnt个扇区到buf
void ide_read(struct disk* hd, unsigned int lba, void* buf, unsigned int sec_cnt){
    if(lba > max_lba || sec_cnt <= 0)
    	return;
    lock_acquire(&(hd->my_channel->lock));//保证一次只操作同一通道上的一块硬盘
//确定hd是主盘还是从盘   
    select_disk(hd);                      
    
    unsigned int secs_op;                 //每次操作的扇区数
    unsigned int secs_done = 0;           //已完成的扇区数
    
    while(secs_done < sec_cnt){
        if(secs_done + 256 <= sec_cnt)  
            secs_op = 256;                //尽量一次操作256个扇区
        else
        	secs_op = sec_cnt - secs_done;//最后余下的不足256个扇区一次性完成
//向硬盘hd提供每次要读写的起始扇区地址和要读写的扇区数
        select_sector(hd, lba + secs_done, secs_op);
        cmd_out(hd->my_channel, CMD_READ_SECTOR);    //硬盘开始读扇区了

        semaphore_down(&(hd->my_channel->disk_done));//在硬盘开始工作时阻塞当前线程，硬盘完成读操作会主动发中断，然后在intr_hd_handler()中唤醒线程

        if(busy_wait(hd)==0){                        //线程被唤醒后检测硬盘是否可读
        	put_str("Panic: read disk failed!\n");
        	lock_release(&(hd->my_channel->lock));
		    return;
		}
        
      	read_from_sector(hd, (void*)((unsigned int)buf +secs_done * 512), secs_op);
      	secs_done += secs_op;
    }
    
    lock_release(&(hd->my_channel->lock));   
}


//从buf写sec_cnt个扇区数据到硬盘lba地址处
void ide_write(struct disk* hd, unsigned int lba, void* buf, unsigned int sec_cnt){
    if(lba > max_lba || sec_cnt <= 0)
    	return;
    lock_acquire(&(hd->my_channel->lock));
    
    select_disk(hd);
    unsigned int secs_op;
    unsigned int secs_done = 0;
    while(secs_done < sec_cnt){
        if(secs_done + 256 <= sec_cnt)
            secs_op = 256;
        else
        	secs_op = sec_cnt - secs_done;
        
    	select_sector(hd, lba + secs_done, secs_op);
    	cmd_out(hd->my_channel, CMD_WRITE_SECTOR);
    	
        if(busy_wait(hd)==0){
        	put_str("Panic: write disk failed!\n");
        	lock_release(&(hd->my_channel->lock));
		    return;
		}
    	
    	write_to_sector(hd, (void*)((unsigned int)buf + secs_done * 512), secs_op);
    	semaphore_down(&(hd->my_channel->disk_done)); //和ide_read阻塞线程的时机有所不同
    	secs_done += secs_op;                         //线程被intr_hd_handler()唤醒后才能确定已经写好了
    }
    
    lock_release(&(hd->my_channel->lock)); 
}


//硬盘任务结束时的中断程序
void intr_hd_handler(unsigned int irq_no){
    if(irq_no != 0x2e && irq_no != 0x2f)
    	return;
    unsigned int ch_no = irq_no - 0x2e;
    struct ide_channel* channel = &channels[ch_no];
    if(channel->irq_no != irq_no)
    	return;
    if(channel->expecting_intr == 1){//是在cmd_out()中置为1的
		channel->expecting_intr = 0; 
		semaphore_up(&(channel->disk_done));
		inb(reg_status(channel));    //需要读取status寄存器使硬盘控制器认为此次的中断已被处理完成，否则硬盘不会再产生新的中断，导致硬盘无法执行新的读写
    }   
}








//将dst中len个相邻字节交换位置后存入buf，因为读入的时候字节顺序是反的
void swap_pairs_bytes(const char* dst, char* buf, unsigned int len){
    unsigned char idx;
    for(idx = 0; idx < len; idx += 2){ //将每个字的2个字节互换
    	buf[idx+1] = *dst++;
    	buf[idx]   = *dst++;
    }
    buf[idx] = '\0';
}
//向硬盘发送identify命令以获得硬盘参数信息，identify命令返回的结果都是以字为单位，并不是宇节
void identify_disk(struct disk* hd){             //此函数会放在ide_init()中，所以不需要加锁
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);       //硬盘开始工作
    semaphore_down(&(hd->my_channel->disk_done));//等待线程被intr_hd_handler()唤醒 
    if(busy_wait(hd)==0){
    	print("Panic: identify disk failed!\n");
    	return;
    }
    read_from_sector(hd, id_info, 1);            //读一个扇区
    
    char buf[64] = {0};                          //identify命令会返回20字节的硬盘序列号字符串、40字节的硬盘型号字符串、4字节的用户可用扇区数
    unsigned char sn_start = 10 * 2, sn_len = 20, md_start = 27*2, md_len = 40;//序列号起始字节地址、序列号字偏移量、型号起始字节地址、型号字偏移量
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);？？
    swap_pairs_bytes(&id_info[md_start], buf, md_len);？？
    unsigned int sectors = *(unsigned int*)&id_info[60*2];
} 




void partition_scan(struct disk* hd, unsigned int ext_lba){
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);
    unsigned char part_idx = 0;
    struct partition_table_entry* p = bs->partition_table;       //p为分区表开始的位置
    while(part_idx++ < 4){
    	if(p->fs_type == 0x5){                                   //拓展分区
    	    if(ext_lba_base != 0){
    	    	partition_scan(hd, p->start_lba + ext_lba_base); //继续递归转到下一个逻辑分区再次得到表
    	    }
    	    else{                                                //第一次读取引导块
    	    	ext_lba_base = p->start_lba;                     //记录下扩展分区的起始 lba 地址，后面所有的扩展分区地址都相对于此
    	    	partition_scan(hd, ext_lba_base);                //递归
    	    }
    	}
    	else if(p->fs_type != 0){
    	    if(ext_lba == 0){                                    //主分区
    	    	hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
    	    	hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
    	    	hd->prim_parts[p_no].my_disk = hd;
    	    	list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
    	    	sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no+1);
    	    	p_no++;
    	    	ASSERT(p_no<4);	//0 1 2 3 最多四个
    	    }
    	    else{                                               //其他分区
    	    
    	    	hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
    	    	hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
    	    	hd->logic_parts[l_no].my_disk = hd;
    	    	list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
    	    	sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no+5); //从5开始
    	    	l_no++;
    	    	if(l_no >= 8)
    	    		return; //只支持8个
    	    }
    	}
    	++p;
    }
    sys_free(bs);
}
//打印分区信息
int partition_info(struct list_elem* pelem, int arg){
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("    %s  start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba, part->sec_cnt);
    return false; //list_pop完
}



