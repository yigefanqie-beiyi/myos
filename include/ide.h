#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "bitmap.h"
#include "list.h"
#include "sync.h"
#include "super_block.h"

//分区结构
struct partition
{
    uint32_t start_lba;          //起�?扇区
    uint32_t sec_cnt;            //扇区�?
    struct disk* my_disk;        //分区所属硬�?
    struct list_elem part_tag;   //所在队列中的标�?
    char name[8];                //分区名字
    struct super_block* sb;	  //�?���? 超级�?
    struct bitmap block_bitmap;  //块位�?
    struct bitmap inode_bitmap;  //i结点位图
    struct list open_inodes;     //�?��区打开
};

struct partition_table_entry
{
    uint8_t bootable;		  //�?���?���?
    uint8_t start_head;	  //开始�?头号
    uint8_t start_sec;		  //开始扇区号
    uint8_t start_chs;		  //起�?柱面�?
    uint8_t fs_type;		  //分区类型
    uint8_t end_head;		  //结束磁头�?
    uint8_t end_sec;		  //结束扇区�?
    uint8_t end_chs;		  //结束柱面�?
    uint32_t start_lba;	  //�?��区起始的lba地址
    uint32_t sec_cnt;		  //�?��区数�?
} __attribute__ ((packed));

struct boot_sector
{
    uint8_t other[446];	  			//446 + 64 + 2 446�?��来占位置�?
    struct partition_table_entry partition_table[4];  //分区表中4�? 64字节
    uint16_t signature;				//最后的标识标志 魔数0x55 0xaa				
} __attribute__ ((packed));

//�?��
struct disk
{
    char name[8];		      //�?��盘的名称
    struct ide_channel* my_channel;    //这块�?��归属于哪个ide通道
    uint8_t dev_no;		      //0表示主盘 1表示从盘
    struct partition prim_parts[4];  //主分区顶多是4�?
    struct partition logic_parts[8]; //逻辑分区最多支�?8�?
};

// ata 通道结构
struct ide_channel
{
    char name[8];		      //ata通道名称
    uint16_t port_base;	      //�?��道的起始�?口号
    uint8_t  irq_no;		      //�?��道所用的�?���?
    struct lock lock;                //通道�? 一�?��盘一通道 不能同时
    bool expecting_intr;	      //期待�?���?��的bool
    struct semaphore disk_done;      //用于阻�? 唤醒驱动程序  和锁不一�? 把自己阻塞后 把cpu腾出�?
    struct disk devices[2];	      //一通道2�?�� 1�?1�?
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[2];
extern struct list partition_list;

void ide_init(void);
void select_disk(struct disk* hd);
void select_sector(struct disk* hd,uint32_t lba,uint8_t sec_cnt);
void cmd_out(struct ide_channel* channel,uint8_t cmd);
void read_from_sector(struct disk* hd,void* buf,uint8_t sec_cnt);
void write2sector(struct disk* hd,void* buf,uint8_t sec_cnt);
bool busy_wait(struct disk* hd);
void ide_read(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt);
void ide_write(struct disk* hd,uint32_t lba,void* buf,uint32_t sec_cnt);
void intr_hd_handler(uint8_t irq_no);
void swap_pairs_bytes(const char* dst,char* buf,uint32_t len);
void identify_disk(struct disk* fd);
void partition_scan(struct disk* hd,uint32_t ext_lba);
bool partition_info(struct list_elem* pelem,int arg);

#endif
