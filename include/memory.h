#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "global.h"
#include "bitmap.h"
#include "sync.h"
#include "list.h"


/*虚拟地址池*/
struct virtual_addr{
    struct bitmap vaddr_bitmap;            //虚拟地址要用到的bitmap
    uint32_t vaddr_start;   //虚拟地址的起始地址
};

/*内存池标记，用于判断用哪个内存池*/
enum pool_flags
{
    PF_KERNEL = 1,  //内核内存池
    PF_USER = 2     //用户内存池
};

/*内存块*/
struct mem_block{
    struct list_elem free_elem;     //list_elem是链表元素
};

/*内存块描述符*/
struct mem_block_desc
{
    uint32_t block_size;            //内存块大小，例如16字节，64字节
    uint32_t block_per_arena;       //本arena中可容纳此mem_block的数量
    struct list free_list;	        //目前可用的mem_block链表
};

/*内存仓库*/
struct arena
{
    struct mem_block_desc* desc;        //此arena关联的mem_block_desc
    uint32_t cnt;           //large为true，cnt代表页框数，为false，代表空闲mem_block数量
    bool large; 
};

/*内存池结构*/
struct pool{
    struct bitmap pool_bitmap;      //内存池中用来管理内存的bitmap
    uint32_t phy_addr_start;    //本内存池所管理的物理内存的起始地址
    uint32_t pool_size;     //本内存池的字节容量
    struct lock lock;       //pool锁，申请内存时互斥
};

#define PG_P_1 1        //页表项或页目录项的存在属性位
#define PG_P_0 0        //存在属性位
#define PG_RW_R 0       //读/执行
#define PG_RW_W 2       //读/写/执行
#define PG_US_S 0       //系统级
#define PG_US_U 4       //用户级

#define DESC_CNT 7      //内存块描述符的个数

/*内存管理部分初始化入口*/
void mem_init();
/*返回虚拟地址vaddr对应的PTE指针，该指针也用虚拟地址表示*/
uint32_t* pte_ptr(uint32_t vaddr);
/*返回虚拟地址vaddr对应的PDE指针，该指针也用虚拟地址表示*/
uint32_t* pde_ptr(uint32_t vaddr);
/*在m_pool指向的物理内存池中分配1个物理页，成功则返回分配的物理页的物理地址*/
void* palloc(struct pool* m_pool);
/*添加虚拟地址_vaddr到物理地址_page_phyaddr的映射*/
void page_table_add(void* _vaddr,void* _page_phyaddr);
/*在pf所指向的虚拟内存池子中，申请分配pg_cnt个页，成功则返回申请到的首个虚拟地址*/
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
/*从内核物理内存池中申请pg_cnt页内存，成功则返回其虚拟地址*/
void* get_kernel_pages(uint32_t pg_cnt);
/*初始化内核和用户的内存池*/
static void mem_pool_init(uint32_t all_mem);
/*在用户空间中申请pg_cnt页的内存，并返回其虚拟地址*/
void* get_user_pages(uint32_t pg_cnt);
/*在用户空间或者内核空间分配一页内存，并将虚拟地址vaddr与其做映射*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
/* 得到虚拟地址映射的物理地址 */
uint32_t addr_v2p(uint32_t vaddr);
/*内存块初始化*/
void block_desc_init(struct mem_block_desc* desc_array);
//返回arena中第idx个内存块的地址 空间布局为 arena元信息+n个内存块
struct mem_block* arena2block(struct arena* a,uint32_t idx);
//返回内存块b所在的arena地址
struct arena* block2arena(struct mem_block* b);
//在堆中申请size字节内存
void* sys_malloc(uint32_t size);
/*将物理地址pg_phy_addr回收到物理内存池*/
void pfree(uint32_t pg_phy_addr);
/*去掉页表中虚拟地址vaddr的映射，只去掉vaddr对应的pte*/
void page_table_pte_remove(uint32_t vaddr);
/*在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址*/
void vaddr_remove(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt);
/*释放以虚拟地址vaddr为起始的cnt个物理页框*/
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt);
/*回收内存ptr*/
void sys_free(void* ptr);
//得到1页大小内存并复制到页表中 专门针对fork时虚拟地址位图无需操作
//因为位图我们后面会复制父进程的 所以当然不用继续对虚拟位图操作了
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr);


#endif