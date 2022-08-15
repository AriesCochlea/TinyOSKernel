/*先使用bximage创建虚拟从盘hd80M.img用于实现硬盘驱动和文件系统并在bochsrc.txt中加入：  ata0-slave:  type=disk, path="./hd80M.img", mode=flat
ata0通道的两个硬盘（主和从）的中断信号挂在8259A从片的IRQ14上，两个硬盘共享同一个IRQ接口
在对硬盘发命令时，需要提前指定是对主盘还是从盘进行操作，这是在硬盘控制器的device寄存器中第4位的dev位指定的
*/


#pragma once
#include "bitmap.h"
#include "list.h"
#include "sync.h"

//16字节的存分区表项。最多有4个表项共64字节
struct partition_table_entry{
    unsigned char bootable;		     //是否是活动分区：取值只能为0x80或0
    unsigned char start_head;	     //起始磁头号
    unsigned char start_sec;		 //起始扇区号
    unsigned char start_chs;		 //起始柱面号
    unsigned char fs_type;		     //分区类型
    unsigned char end_head;		     //结束磁头号
    unsigned char end_sec;		     //结束扇区号
    unsigned char end_chs;		     //结束柱面号
    unsigned int start_lba;	         //本分区起始的LBA地址
    unsigned int sec_cnt;		     //本分区的扇区数目
} __attribute__ ((packed));          //__attribute__ ((packed))关键字可以让结构体按照紧凑排列的方式占用内存

struct boot_sector{                  //MBR：446 + 64 + 2
    unsigned char other[446];
    struct partition_table_entry partition_table[4];
    unsigned short signature;        //魔数0x55，0xaa				
} __attribute__ ((packed));



//分区
struct partition{
    char name[8];                    //分区名字，如sda1、sda2。注：sda是硬盘的名称，末尾带上数字则表示分区的名称
    struct disk* my_disk;            //分区所属的硬盘
    unsigned int start_lba;          //起始扇区的逻辑地址LBA
    unsigned int sec_cnt;            //分区的容量：扇区数
    struct list_elem part_tag;       //本分区的标记，用于将分区汇总到链表
    struct super_block* sb;	         //本分区的超级块的指针
    struct bitmap block_bitmap;      //块位图：为了简单，本项目的块以一个扇区即512字节为单位。
    struct bitmap inode_bitmap;      //i结点位图
    struct list open_inodes;         //本分区打开的i结点队列
};



//硬盘
struct disk{
    char name[8];		             //本硬盘的名称，如sda、sdb、sdc
    struct ide_channel* my_channel;  //这块硬盘归属于哪个ide通道（ata通道）————就是已经过时的PATA
    unsigned char dev_no;		     //0表示主盘，1表示从盘
    struct partition prim_parts[4];  //本硬盘的主分区最多只能是4个，为了向前兼容……
    struct partition logic_parts[8]; //本项目的逻辑分区最多支持8个
    ？为简单起见，本项目不支持逻辑分区
};

//ata通道
struct ide_channel{
    char name[8];		             //ata通道名称，如ata0或ide0
    struct disk devices[2];	         //一个通道连接两块硬盘，一主一从
    unsigned short port_base;	     //本通道的起始端口号
    unsigned char  irq_no;		     //本通道所用的中断号
    struct lock lock;                //通道锁：主从硬盘共用同一个通道，但一个通道只能有一个数据准备完毕的中断信号
    int expecting_intr;	             //本通道是否正在等待硬件的中断。驱动程序（中断处理程序）向硬盘发完命令后等待来自硬盘的中断
                                     //驱动程序中会通过此成员来判断此次的中断是否是因为之前的硬盘操作命令引起的，如果是，则进行下一步动作，如获取数据等
    struct semaphore disk_done;      //驱动程序向硬盘发送命令后，在等待硬盘工作期间可以通过此信号量阻塞自己，避免干等着浪费CPU
                                     //等硬盘工作完成后会主动发出中断，中断处理程序通过此信号量将硬盘驱动程序唤醒 ————因为我们的项目还未实现“条件变量”
};






//初始化硬盘
void ide_init();

//从硬盘hd的逻辑扇区地址lba开始读取sec_cnt个扇区到buf
void ide_read(struct disk* hd, unsigned int lba, void* buf, unsigned int sec_cnt);


void ide_write(struct disk* hd, unsigned int lba, void* buf, unsigned int sec_cnt);


void intr_hd_handler(unsigned int irq_no);





void partition_scan(struct disk* hd, unsigned int ext_lba);
int partition_info(struct list_elem* pelem, int arg);



