//硬盘读写的单位是扇区，但操作系统不会有了一扇区数据就去读写一次磁盘。Linux操作系统每次读写一个块（在Windows中被称为“簇”），即块是文件系统读写的单位。
//比如在Windows中格式化分区时，若选择文件系统类型为FAT32，可以选择多种不同大小的簇，有4KB、32KB等。
//一个文件不足一个块时，也要占满一个块，即使会浪费空间。一个文件超过一个块时，其组织方式因文件系统而异：
/*  FAT文件系统中的所有文件用链式结构存储，在每个块的最后存储下一个块的地址，优点是提升磁盘利用率，缺点是算法效率低；
    Unix的文件系统采用索引结构，避免了访问某一数据块需要从头把其前所有数据块再遍历一次的缺点，
    为每个文件建立了一个索引表，索引表就是块地址数组，每个元素就是块的地址，元素下标是文件块的索引。该索引结构称为inode，用来索引一个文件的所有块。*/
//在UINX文件系统中，一个文件必须对应一个inode，硬盘中有多少文件就有多少inode。索引结构的缺点是文件很大时块也比较多，索引表会占用不小空间。
/*  每个inode有15个项用于索引。前12个索引项是文件的前12个块的地址。若文件大于12个块，那就建立多级间接块索引表：
    一级间接块索引表（占用一个物理块，各表项都是块的地址，共可容纳256个块的地址）的地址存储到inode的第13个索引项中。一级间接块索引表可存储12+256个块。
    二级间接块索引表（占用一个物理块，各表项存储的是一级间接块索引表的地址）的地址存储到inode的第14个索引项中。二级间接块索引表可存储12+256+256*256个块。
    三级间接块索引表（占用一个物理块，各表项存储的是二级间接块索引表的地址）的地址存储到inode的第15个索引项中。
    使用三级间接块索引表，文件最大可达到12+256+256*256+256*256*256个块，如果块是4KB，则文件占用64GB。
    如果有超过64GB的超大文件出现，那就只能用mv命令将其切割成多个小文件了。用mkfs命令分区时需要先确定该分区的最大inode数量（即inode数组的长度）*/
//inode的数量等于文件的数量，为方便管理，分区中所有文件的inode放在inode_table中来维护，本质上就是inode数组，数组元素的下标便是文件inode的编号。
//目录和文件都用inode来表示，因此目录也是文件。目录中的目录项要么指向普通文件，要么指向目录。在某个目录下执行ls命令的时候，输出结果就是目录项的展现。
//目录项中包含文件名、inode编号和文件类型等，作用：一是文件类型用于标识此inode编号对应的inode的所指向的文件的数据块的内容是普通文件还是目录（多个目录项）
/*二是将文件名与inode编号做个绑定关联，这样用户便可以：1.在目录中找到文件名所在的目录项；
                                                    2.使用该目录项里的inode编号作为inode数组的索引下标，找到inode；
                                                    3.从该inode中获取文件数据块的地址，读取数据块。*/
//不管文件是普通文件，还是目录，它总会存在于某个目录中。所有的普通文件或目录都存在于根目录'/'之下，根目录是所有目录的父目录。





#pragma once
#include "list.h"
#include "ide.h"


//本项目文件系统布局：整个硬盘分为：主引导记录MBR、若干个分区。每个分区：操作系统引导块OBR、超级块、空闲块位图、inode位图、inode数组、根目录、空闲块区域
//本项目不实现扩展分区、子扩展分区、逻辑分区。为简单起见，本项目的硬盘上只有一个分区。
//本项目的一个块只占一个扇区



#define MAX_FILE_NAME_LEN	16    //最大文件名长度
#define MAX_FILES_PER_PART  4096  //每个分区最大支持文件数，即最大的inode数，也是inode数组的长度
#define BITS_PER_SECTOR 	4096  //每扇区的位数：512* 8，位图里面每个bit代表一个扇区（或块）
#define SECTOR_SIZE		    512   //每扇区的字节数：512
#define BLOCK_SIZE	SECTOR_SIZE   //块字节大小：本项目的一个块只占一个扇区

enum file_types {
    FT_UNKNOWN, 	              //未知文件类型
    FT_REGULAR, 	              //普通文件类型
    FT_DIRECTORY	              //目录文件类型
};




//超级块
struct super_block{
    unsigned int magic;			      //魔数用来标识文件系统类型
    unsigned int sec_cnt;			  //本分区总共的扇区数
    unsigned int inode_cnt;		      //本分区中的inode数量
    unsigned int part_lba_base;       //本分区的起始LBA位置
    unsigned int block_bitmap_lba;	  //空闲块位图的起始LBA地址
    unsigned int block_bitmap_sects;  //空闲块位图所占的扇区数    
    unsigned int inode_bitmap_lba;	  //inode位图的起始LBA地址
    unsigned int inode_bitmap_sects;  //inode位图所占的扇区数   
    unsigned int inode_table_lba;	  //inode数组的起始LBA地址
    unsigned int inode_table_sects;	  //inode数组所占的扇区数
    unsigned int data_start_lba;	  //空闲块起始的第一个LBA扇区号
    unsigned int root_inode_no;		  //根目录所在的inode编号
    unsigned int dir_entry_size;	  //目录项大小
    unsigned char pad[460];			  //13*4 + 460 = 512
}; __attribute__ ((packed));




//FCB文件控制块：inode。创建文件的本质是创建了文件的文件控制块，即目录项和inode
struct inode{
    unsigned int i_no;	     	  //inode编号
    unsigned int i_size;	      //文件大小，以字节为单位
    unsigned int i_open_cnts;     //记录此文件被打开的次数
    int write_deny;	     	      //写文件不能并行，多个写操作应该以串行的方式进行
    
    unsigned int i_sectors[13];   //i_sectors[0～11]是直接块，用来存储物理块的地址，i_sectors[12]存储一级间接块索引表的地址
                                  //本项目只实现一级间接块索引表，且一级间接块数量为128个，故本项目的文件的最大块数为12+128=140个，即单个文件最大为71KB
    struct list_elem inode_tag;   //用于加入己打开的inode队列：为了避免下次再打开该文件时还要从硬盘上重复载入inode
};



//目录结构：只在内存中，用过之后就释放了，不会回写到磁盘中
struct dir{			
    struct inode* inode;	     //指向已经打开的inode指针，该inode在已打开的inode队列中
    unsigned int dir_pos;		 //目录偏移位置，即游标
    unsigned char  dir_buf[512]; //目录的数据缓存，如读取目录时，用来存储返回的目录项
};
//目录项结构：只在内存中，用过之后就释放了，不会回写到磁盘中
struct dir_entry{		
    char filename[MAX_FILE_NAME_LEN];	//16个字的名字
    unsigned int i_no;			        //inode编号
    enum file_types f_type;		        //文件类型：目录或普通文件
};



//文件系统初始化
void filesys_init();




