# TinyOSKernel

### 项目说明：

简易的学习型项目，给操作系统初学者提供一个可供实践的范例，以“精简”为原则，主要从软件角度来理解操作系统的运行原理。
本项目是参考《操作系统真象还原》（作者：郑刚）实现的32位操作系统。注意：原书代码是不能直接运行在裸机上的，需要做些调整。
笔者的生产环境：
          64位Intel机，4G内存；
          Ubuntu20.04；bochs2.6.8；
          gcc9.4.0； nasm2.14.02。

### 前置知识：
1. 操作系统、计算机组成原理和系统结构等理论知识；
2. 数据结构和算法，C语言，Intel汇编，内联AT&T汇编；
3. nasm汇编器，ld链接器，gcc编译器 ，make等工具。


### 主要功能：
1. 内核加载：依次实现MBR、内核加载器、实模式到保护模式的过渡、加载并解析ELF文件以进入C语言版内核。
2. 内存管理：使用内存分页机制实现虚拟地址到物理地址的映射，使用位图实现物理内存池和虚拟内存池来管理内存。
3. 中断控制：实现时钟中断用于实现任务调度，键盘中断用于实现键盘驱动，硬盘中断用于实现硬盘驱动，0x80号中断用于实现系统调用。
4. 任务调度：使用双向链表实现单核CPU下的内核线程的时间片轮转调度算法。
            用户进程：TODO，要先基于文件系统实现用户进程的加载。
5. 同步互斥：使用关中断实现原子操作，基于任务调度和阻塞队列实现信号量，基于二元信号量实现互斥锁。
5. 文件系统：基于硬盘驱动实现硬盘分区、超级块、inode、文件描述符、普通文件、目录文件；
            实现文件操作功能：路径解析、文件检索，文件或目录的创建、打开、关闭、读写、删除。
           （代码量大且繁琐，bug频发……写了一半就弃了……
             而且没写文件系统之前能正常运行的任务调度功能在删除文件系统后莫名其妙不能正常运行了；
             找了几天都没找到那个bug……论版本管理的重要性……所以下面只能简单演示一下时钟中断和键盘中断的运行效果……）————有点可惜              
6. 系统交互：TODO：使用0x80号中断实现系统调用；再基于文件系统和系统调用实现shell。
7. 进程通信：TODO：基于文件系统实现“管道”，基于内存管理实现“共享内存”。
8. 网络系统：TODO：知识水平还不够，可能得写网卡驱动，再从网络协议栈一层层写到手撕TCP，那就是另一个项目了……

——大概率是不会继续写了，因为转行来的本菜鸟不打好基础、获得业界技术经验而是去造小玩具轮子是不可取的……
  对于在校生来说，感觉是不错的操作系统初学者编程实践，但对我来说，除了一些过时的硬件“知识”，其他的部分对自己的提升并不大。
  越写到后面越感觉有点不对劲，还不如多学几个业界常用的POSIX API或掌握Linux上的常用工具或去看内核某部分源码（如果真的感兴趣）来得实在。
  时间真不能像在校生那样“挥霍”……接近一个月来大部分时间都在熬夜和bug打交道，搞得生活都不正常了……然而只是解决bug却没有实质性的收获……
  不过，后面的内容虽然没写完，但还是精读了几遍，该掌握的知识和原理全掌握了。
  
  


### 运行方法：

一、在bochs虚拟机上运行：略。自己看书去吧……

二、在物理机上运行：实验结束后，请按电源键强制关机，拔出U盘，改回曾经的BIOS或UEFI设置即可。

     1.准备一个不用的U盘（推荐至少2GB，注意备份数据。实验结束后，U盘可以重新格式化来恢复正常使用）；
     2.在Linux上确定你的U盘的设备名称：在插入U盘前后分别使用df命令获取磁盘文件系统的情况——
       插入U盘后，笔者这里多了一个/dev/sdb（这就代表我的U盘路径），注意最后的数字表示分区（笔者这里是sdb1），需要忽略；
       使用 sudo umount /dev/sdb 卸载该U盘上所有挂载到Linux的分区。
     3.修改usb.sh，将其中的设备路径改为你自己的U盘路径。这一步一定不能错！这里利用的是linux上的dd命令将二进制文件烧到U盘上。
     4.依次运行sh build.sh和sh usb.sh。此时U盘已经准备完毕。
     5.在电脑关机状态下打开BIOS（每种型号电脑打开方法不一样，请自行搜索），
       如果是UEFI（一般都会兼容传统的BIOS）则改为传统BIOS启动（具体改法因机而异，请自行搜索），选择USB HDD启动。
       
<div>
  <img src="temp/1.png" height="300"/>
</div>

### 运行效果：
键盘驱动和时钟中断
<div>
  <img src="temp/2.gif" height="300"/>
  <img src="temp/3.gif" height="300"/>
</div>



### 项目讲解：TODO：

——作为一个非常简单的半成品，感觉没啥可讲解的……不过有句话我还是得说一下：
```
  学习操作系统的目的
  1.并不是了解那些和寄存器、硬件等平台强相关的汇编代码部分，也不是PC机加电、从物理地址0xFFFF0运行BIOS（现在都是UEFI的时代了）、BIOS将MBR加载到物理地址0x7c00运行、
    MBR加载硬盘的活动分区（可引导分区）上的OBR、OBR将操作系统内核加载进内存运行，从实模式、保护模式到长模式的切换，打开A20地址线等一次性的任务——历史包袱而已，
    也不是实模式下的中断向量表到了保护模式中变成了中断描述符表、实模式的多段模式到保护模式里成了平坦模式、硬盘控制器分为PATA和SATA等这类“历史知识”…… 现在都直接利用GNU GRUB写系统了……
  2.而是关注操作系统重点概念（中断控制、内存管理、任务调度、同步互斥、进程通信、文件系统、系统调用等）的背后原理和联系，从软件角度来理解操作系统的运行机理。
  
```

