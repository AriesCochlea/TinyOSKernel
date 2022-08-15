//顺带一提：为了方便MBR找到活动分区上的内核加载器（操作系统引导记录OBR），内核加载器的入口地址也必须在固定的位置————各分区最开始的扇区，这是约定好的。
//在OBR扇区的前3个字节存放了跳转指令，这同样是约定，因此MBR找到活动分区后，就跳到活动分区的OBR引导扇区的起始处，该起始处的跳转指令直接跳到操作系统引导程序
//OBR中开头的跳转指令跳往的目标地址并不固定，这是由所创建的文件系统决定的，不管跳转目标地址是多少，总之那里通常是操作系统的内核加载器。
//对于FAT32文件系统来说，此跳转指令会跳转到本扇区（OBR扇区）偏移0x5A字节的操作系统引导程序处。OBR扇区也以魔数0x55和0xaa结束。
//其实要想把操作系统安装到文件系统上，必须在实现内核之前先实现文件系统模块，至少得完成写文件的功能，然后把操作系统通过写文件功能写到文件系统上。
//但咱们的操作系统安装到裸盘 hd60M.img 上，上面没有分区，更谈不上文件系统了。

#include "filesystem.h"
#include "ide.h"
#include "stdio-kernel.h"
#include "str.h"
#include "list.h"

struct partition* cur_part;//挂载分区时所用的全局变量

全局缓冲区，因为栈不够用

//在硬盘hd的part分区上创建文件系统
void partition_format(struct disk* hd, struct partition* part){
    unsigned int boot_sector_sects = 1;		//OBR块
    unsigned int super_block_sects = 1;		//超级块
    //有MAX_FILES_PER_PART个inode，那inode位图里就有MAX_FILES_PER_PART个bit，每BITS_PER_SECTOR个bit组成一个块，不足一个块的bit直接舍弃：
    unsigned int inode_bitmap_sects = MAX_FILES_PER_PART/BITS_PER_SECTOR;                   //inode位图占的块数
	unsigned int inode_table_sects = sizeof(struct inode)*MAX_FILES_PER_PART/SECTOR_SIZE;   //inode数组所占的块数
    unsigned int free_sects = part->sec_cnt - boot_sector_sects - super_block_sects - inode_bitmap_sects - inode_table_sects;
    unsigned int block_bitmap_sects = free_sects/BITS_PER_SECTOR;           //free_sects个空闲块所需的位图需要占用的块数
    unsigned int block_bitmap_bit_len = free_sects - block_bitmap_sects;	//空闲块位图最终有多少个bit。原本有free_sects个bit，但位图本身得占空间
    block_bitmap_sects = block_bitmap_bit_len/BITS_PER_SECTOR;              //空闲块位图最终占的块数
    
//初始化超级块
    struct super_block sb;	        		                                //超级块是512字节，栈还够用
    sb.magic              = 0x23333333;			？？
    sb.sec_cnt            = part->sec_cnt; 		
    sb.inode_cnt          = MAX_FILES_PER_PART;
    sb.part_lba_base      = part->start_lba;	
    sb.block_bitmap_lba   = part->start_lba + boot_sector_sects + super_block_sects;
    sb.block_bitmap_sects = block_bitmap_sects;
    sb.inode_bitmap_lba   = sb.block_bitmap_lba + block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;
    sb.inode_table_lba    = sb.inode_bitmap_lba + inode_bitmap_sects;
    sb.inode_table_sects  = inode_table_sects;
    sb.data_start_lba     = sb.inode_table_lba + inode_table_sects;
    sb.root_inode_no	  = 0;		                  //根目录inode起始编号为0 
    sb.dir_entry_size     = sizeof(struct dir_entry); //目录项大小
    
//把元信息挨个写进硬盘
    ide_write(hd, part->start_lba + boot_sector_sects, &sb, super_block_sects);
    
    
    
    unsigned int buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects) ? sb.block_bitmap_sects : sb.inode_bitmap_sects;
    buf_size = ((buf_size >= inode_table_sects) ? buf_size : inode_table_sects) * SECTOR_SIZE; ？？//找一个最大的数据缓冲区
    unsigned char* buf = (unsigned char*)sys_malloc(buf_size);
    

    buf[0] |= 0x01;//第0块预留给根目录，先在位图中占位
    unsigned int block_bitmap_last_byte = block_bitmap_bit_len / 8; 
    unsigned char block_bitmap_last_bit  = block_bitmap_bit_len % 8; 
    unsigned int last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE); //位图最后一个扇区中，不足一扇区的其余部分
    mem_set(&buf[block_bitmap_last_byte], 0xff, last_size);	                       //全部置1    

    unsigned char bit_idx = 0;
    while(bit_idx <= block_bitmap_last_bit)
    	buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);	                       //有效位全部置0
    
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
    
    
    //初始化inode位图
    mem_set(buf, 0, buf_size);
    buf[0] |= 0x01;	                                                //第一个inode用于根目录
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects); //第一个inode初始化在后面
    
    //初始化inode数组
    mem_set(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;	
    i->i_size = sb.dir_entry_size * 2;		        //. 和 .. 
    i->i_no   = 0;
    i->i_sectors[0]  = sb.data_start_lba;			//根目录
    
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);
    
    //把根目录写进sb.data_start_lba
    mem_set(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    
    mem_cpy(p_de->filename, ".", 1);
    p_de->i_no = 0;				
    p_de->f_type = FT_DIRECTORY;
    p_de++;							                //下一条目录项
    
    mem_cpy(p_de->filename, "..", 2);
    p_de->i_no = 0;					                //根目录的父目录就是它自己
    p_de->f_type = FT_DIRECTORY;
    
    ide_write(hd, sb.data_start_lba, buf, 1);	
    sys_free(buf);	
}


//挂载分区的实质是把该分区文件系统的元信息从硬盘读到内存中，硬盘资源的变化都用内存中元信息来跟踪，如有写操作，及时将内存中的元信息同步写入到硬盘以持久化
//在分区链表中找到名为part_name的分区，并将其指针赋值给全局变量cur_part
bool mount_partition(struct list_elem* pelem, int arg)//pelem 是分区partition中的part_tag的地址。arg是待比对的参数，此处是分区名
{
    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);//将pelem还原为分区part
    if(strcmp(part->name, part_name)==0)		   //找到名为part_name的分区
    {
    	cur_part = part;						   //全局变量
    	struct disk* hd = cur_part->my_disk;
    	
    	struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);//存储从硬盘上读取的超级块
    	if(sb_buf == NULL)
    	    PANIC("alloc memory failed!");
    	
    	mem_set(sb_buf, 0, SECTOR_SIZE);
    	ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);
    	
    	cur_part->sb = sb_buf;
    	
    	cur_part->block_bitmap.bits = (unsigned char*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
    	if(cur_part->block_bitmap.bits == NULL)
    	   PANIC("alloc memory failed!");
    	cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
    	ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);
    	
        cur_part->inode_bitmap.bits = (unsigned char*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if(cur_part->inode_bitmap.bits == NULL)
    	   PANIC("alloc memory failed!");
    	cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
    	ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);
    	
    	list_init(&cur_part->open_inodes);
    	printk("mount %s done!\n", part->name);
    	return true;	//停止循环
    	
    }
    return false;	//继续循环
}




//文件系统初始化
void filesys_init()
{
    unsigned char channel_no = 0, dev_no, part_idx = 0;
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    
    if(sb_buf == NULL)	PANIC("alloc memory failed!");
    printk("searching filesysteam......\n");
    while(channel_no < channel_cnt)
    {
    	dev_no = 0;
    	while(dev_no < 2)
    	{
    	    if(dev_no == 0)	//跳过hd60M.img主盘
    	    {
    	    	++dev_no;	
    	    	continue;
    	    }
    	    struct disk* hd = &channels[channel_no].devices[dev_no];		//得到硬盘指针
    	    struct partition* part = hd->prim_parts;				//先为主区创建文件系统
    	    while(part_idx < 12)		//4个主区 + 8个逻辑分区
    	    {
    	    	if(part_idx == 4)
    	    	    part = hd->logic_parts;	
    	    	if(part->sec_cnt != 0)		//分区存在 如果没有初始化 即所有成员都为0
    	    	{
    	    	    mem_set(sb_buf, 0, SECTOR_SIZE);
    	    	    ide_read(hd, part->start_lba +1, sb_buf, 1);	//读取超级块的扇区
    	    	    
    	    	    if(sb_buf->magic != 0x23333333)			//还没有创建文件系统
    	    	    {
    	    	    	hd->name, part->name);
    	    	    	partition_format(hd, part);
    	    	    }
    	    	    else
    	    	    	printk("%s has filesystem\n", part->name);
    	    	}
    	    	++part_idx;
    	    	++part;	    //下一个分区
    	    }
    	    ++dev_no;		//下一个磁盘
    	}
    	++channel_no;		//下一个通道
    }
    sys_free(sb_buf);
    char default_part[8] = "sdb1";	//默认分区的名称为sdb1
    list_traversal(&partition_list, mount_partition, (int)default_part);//功能相当于用 mount_partition(default_part）处理每一个分区
}

