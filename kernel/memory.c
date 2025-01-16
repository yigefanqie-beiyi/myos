#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "sync.h"
#include "interrupt.h"

#define PG_SIZE 4096    //页的大小为4096字节
#define MEM_BITMAP_BASE 0xc009a000      //主线程的PCB在0xc009e000，bitmap设定为支持管理512M内存，所以占4页，减去4000
#define K_HEAP_START 0xc0100000     //之前把第一个页表的前256项映射为了低端1M的内存，所以把堆设置在1M以上保持紧密

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)   //获取addr的高10位,即PDE索引部分
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)   //获取addr的中10位，即PTE索引部分


struct mem_block_desc k_block_descs[DESC_CNT];  //内核内存块描述符数组
struct pool kernel_pool, user_pool; //定义了内核物理内存池和用户物理内存池
struct virtual_addr kernel_vaddr;   //内核的虚拟内存池

/*在pf标识的内存池中申请pg_cnt个虚拟页，成功则返回虚拟页的地址，失败返回NULL*/
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
    int vaddr_start = 0,bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL)     //内核池
    {
    	bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt); //在bitmap中申请连续位，申请成功则返回初始位的下标
    	if(bit_idx_start == -1)	return NULL;
    	while(cnt < pg_cnt)
    	bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),1);   //从bit_idx_start开始连续pg_cnt个位都设为1
    	vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;   //申请到的虚拟地址起始页是虚拟地址开始位置+bitmap申请到的起始位*4K
    }
    else        //用户池，已实现
    {
        struct task_struct* cur = running_thread();     //每个进程都有自己的虚拟池
    	bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
    	if(bit_idx_start == -1)	return NULL;
    	while(cnt < pg_cnt)
    	bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),1);
    	vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    	ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}

/*返回虚拟地址vaddr对应的PTE指针，该指针也用虚拟地址表示*/
uint32_t* pte_ptr(uint32_t vaddr)
{
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/*返回虚拟地址vaddr对应的PDE指针，该指针也用虚拟地址表示*/
uint32_t* pde_ptr(uint32_t vaddr)
{
    uint32_t* pde = (uint32_t*) ((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/*在m_pool指向的物理内存池中分配1个物理页，成功则返回分配的物理页的物理地址*/
void* palloc(struct pool* m_pool)
{
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
    if(bit_idx == -1)	return NULL;
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/*添加虚拟地址_vaddr到物理地址_page_phyaddr的映射*/
void page_table_add(void* _vaddr,void* _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr,page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    
    if(*pde & 0x00000001)   //如果虚拟地址vaddr对应的pde存在位为1
    {
    	ASSERT(!(*pte & 0x00000001));
    	if(!(*pte & 0x00000001))        //如果虚拟地址vaddr对应的pte不存在，则将该pte的存在位设为1，并将对物理地址的映射写入
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	else
    	{
    	    PANIC("pte repeat");
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	}
    } 
    else        //如果虚拟地址vaddr对应的pde存在位为0，即不存在
    {
    	uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);      //调用palloc分配一个新的物理页，该物理页用来当做新的页表，存在pde里
    	*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      
    	memset((void*)((int)pte & 0xfffff000),0,PG_SIZE);
    	ASSERT(!(*pte & 0x00000001));
    	*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
    return;
}

/*在pf所指向的虚拟内存池子中，申请分配pg_cnt个页，成功则返回申请到的首个虚拟地址*/
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);    /*内核和用户平分30M内存，所以申请的页数不能超过15M对应3840页*/
    
    void* vaddr_start = vaddr_get(pf,pg_cnt);   /*在虚拟内存池中申请虚拟地址*/
    if(vaddr_start == NULL)	return NULL;
    
    
    uint32_t vaddr = (uint32_t)vaddr_start,cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool; /*判断该虚拟内存池子属于内核还是用户*/
    
    while(cnt-- > 0)
    {
    	void* page_phyaddr = palloc(mem_pool);      /*申请物理页，返回物理地址*/
    	if(page_phyaddr == NULL)	return NULL;
    	page_table_add((void*)vaddr,page_phyaddr);  /*完成 申请的虚拟地址和申请的物理地址的映射*/
    	vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/*从内核物理内存池中申请pg_cnt页内存，成功则返回其虚拟地址*/
void* get_kernel_pages(uint32_t pg_cnt)
{
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE); /*如果分配成功，则将分配到的物理页全清零*/
    return vaddr;
}

/*在用户空间中申请pg_cnt页的内存，并返回其虚拟地址*/
void* get_user_pages(uint32_t pg_cnt){
    lock_acquire(&user_pool.lock);	//用户进程可能会产生冲突 大部分时间都在用户进程 内核进程可以理解基本不会冲突
    void* vaddr = malloc_page(PF_USER,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/*在用户空间或者内核空间分配一页内存，并将虚拟地址vaddr与其做映射*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    
    //虚拟地址位图置1
    if(cur->pgdir != NULL && pf == PF_USER)
    {
    	bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else if(cur->pgdir == NULL && pf == PF_KERNEL) 
    {
    	bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else
    	PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    	
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL)
    	return NULL;
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

/* 得到虚拟地址映射的物理地址 */
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/*内存块初始化*/
void block_desc_init(struct mem_block_desc* desc_array)
{
    uint16_t desc_idx,block_size = 16;
    for(desc_idx = 0;desc_idx < DESC_CNT;desc_idx++)
    {
    	desc_array[desc_idx].block_size = block_size;
    	desc_array[desc_idx].block_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
    	list_init(&desc_array[desc_idx].free_list);
    	block_size *= 2;
    }   
}

//返回arena中第idx个内存块的地址 空间布局为 arena元信息+n个内存块
struct mem_block* arena2block(struct arena* a,uint32_t idx)
{
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

//返回内存块b所在的arena地址
struct arena* block2arena(struct mem_block* b)
{
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

//在堆中申请size字节内存
void* sys_malloc(uint32_t size)
{
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    
    if(cur_thread->pgdir == NULL)
    {
    	PF = PF_KERNEL;
    	pool_size = kernel_pool.pool_size;
    	mem_pool = &kernel_pool;
    	descs = k_block_descs;
    }
    else
    {
    	PF = PF_USER;
    	pool_size = user_pool.pool_size;
    	mem_pool = &user_pool;
    	descs = cur_thread->u_block_desc;
    }
    //size 超出空间大小或者为 0 负数
    if(!(size > 0 && size < pool_size))
    {
    	return NULL;
    }
    
    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);
    
    //超过页框直接分配页框
    if(size > 1024)
    {
    	uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena),PG_SIZE);
    	a = malloc_page(PF,page_cnt);
    	if(a != NULL)
    	{
    	    memset(a,0,page_cnt * PG_SIZE);
	    a->desc = NULL;
	    a->cnt  = page_cnt;
	    a->large = true;    
	    lock_release(&mem_pool->lock);
	    return (void*)(a+1); //返回地址 struct arena + sizeof(struct arena)
	}
	else
	{
	    lock_release(&mem_pool->lock);
	    return NULL;
	}
    }
    else
    {
    	uint8_t desc_idx;
    	for(desc_idx = 0;desc_idx < DESC_CNT;desc_idx++)
    	{
    	    if(size <= descs[desc_idx].block_size)
    	    {
    	    	break;
    	    }
    	}
    	
    	//已经空了
    	if(list_empty(&descs[desc_idx].free_list))
    	{
    	    a = malloc_page(PF,1);
    	    if(a == NULL)
    	    {
    	    	lock_release(&mem_pool->lock);
    	    	return NULL;
    	    }
    	    memset(a,0,PG_SIZE);
    	    
    	    a->desc = &descs[desc_idx];
    	    a->large = false;
    	    a->cnt = descs[desc_idx].block_per_arena;
    	    uint32_t block_idx;
    	    
    	    enum intr_status old_status = intr_disable();
    	    
    	    for(block_idx = 0;block_idx < descs[desc_idx].block_per_arena;++block_idx)
    	    {
    	    	b = arena2block(a,block_idx);
    	    	ASSERT(!elem_find(&a->desc->free_list,&b->free_elem));
    	    	list_append(&a->desc->free_list,&b->free_elem);
    	    }
    	    intr_set_status(old_status);
    	}
    	
    	b = (struct mem_block*)list_pop(&(descs[desc_idx].free_list));
    	memset(b,0,descs[desc_idx].block_size);
    	
    	a = block2arena(b);
    	--a->cnt;
    	lock_release(&mem_pool->lock);
    	return (void*)b;
    }
}

/*将物理地址pg_phy_addr回收到物理内存池*/
void pfree(uint32_t pg_phy_addr)
{
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr >= user_pool.phy_addr_start)
    {
    	mem_pool = &user_pool;
    	bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else
    {
    	mem_pool = &kernel_pool;
    	bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);	//全部置0
}

/*去掉页表中虚拟地址vaddr的映射，只去掉vaddr对应的pte*/
void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    *pte = ~PG_P_1;
    asm volatile("invlpg %0"::"m"(vaddr):"memory");	//更新tlb
}

/*在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址*/
void vaddr_remove(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0,vaddr = (uint32_t)_vaddr,cnt = 0;
    
    if(pf == PF_KERNEL)
    {
    	bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    	while(cnt < pg_cnt)
    	{
    	    bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),0);
    	}
    }
    else
    {
    	struct task_struct* cur_thread = running_thread();
    	bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
    	while(cnt < pg_cnt)
    	{
    	    bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),0);
    	}
    }
}

/*释放以虚拟地址vaddr为起始的cnt个物理页框*/
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt)
{
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr,page_cnt = 0;
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
    pg_phy_addr = addr_v2p(vaddr);
    
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);
    
    if(pg_phy_addr >= user_pool.phy_addr_start)
    {
    	vaddr -= PG_SIZE;
    	while(page_cnt < pg_cnt)
    	{
    	    vaddr += PG_SIZE;
    	    pg_phy_addr = addr_v2p(vaddr);
    	    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);
    	    pfree(pg_phy_addr);
    	    page_table_pte_remove(vaddr);
    	    page_cnt++;
    	}
    	vaddr_remove(pf,_vaddr,pg_cnt);
   }
   else
   {
   	vaddr -= PG_SIZE;
   	while(page_cnt < pg_cnt)
   	{
   	    vaddr += PG_SIZE;
   	    pg_phy_addr = addr_v2p(vaddr);
   	    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= kernel_pool.phy_addr_start && pg_phy_addr < user_pool.phy_addr_start);
   	    pfree(pg_phy_addr);
   	    page_table_pte_remove(vaddr);
   	    page_cnt++;
   	}
   	vaddr_remove(pf,_vaddr,pg_cnt);
   }
}

/*回收内存ptr*/
void sys_free(void* ptr)
{
    ASSERT(ptr != NULL);
    if(ptr != NULL)
    {
    	enum pool_flags PF;
    	struct pool* mem_pool;
    	
    	if(running_thread()->pgdir == NULL)
    	{
    	    ASSERT((uint32_t)ptr >= K_HEAP_START);
    	    PF = PF_KERNEL;
    	    mem_pool = &kernel_pool;
    	}
    	else
    	{
    	    PF = PF_USER;
    	    mem_pool = &user_pool;
    	}
    	
    	lock_acquire(&mem_pool->lock);
    	struct mem_block* b = ptr;
    	struct arena* a = block2arena(b);
    	
    	ASSERT(a->large == 0 || a->large == 1);
    	if(a->desc == NULL && a->large == true)
    	{
    	    mfree_page(PF,a,a->cnt);
    	}
    	else
    	{
    	    list_append(&a->desc->free_list,&b->free_elem);
    	    if(++a->cnt == a->desc->block_per_arena)
    	    {
    	    	uint32_t block_idx;
    	    	for(block_idx = 0;block_idx < a->desc->block_per_arena;block_idx++)
    	    	{
    	    	    struct mem_block* b = arena2block(a,block_idx);
    	    	    ASSERT(elem_find(&a->desc->free_list,&b->free_elem));
    	    	    list_remove(&b->free_elem);
    	    	}
    	    	mfree_page(PF,a,1);
    	    }
    	}
    	lock_release(&mem_pool->lock);
    }
}


/*初始化内核和用户的内存池*/
static void mem_pool_init(uint32_t all_mem){
    put_str("mem_pool_init start!\n");
    uint32_t page_table_size = PG_SIZE * 256;       //页目录表和页表占用的大小，页目录表占1个页框，第0和768个页目录项映射的第一个页表占一个页框，769到1022页目录项映射了254个页框，总共256个页框
    uint32_t used_mem = page_table_size + 0x100000; //记录当前已使用的内存，低端1M加上页目录和页表占用的大小,目前2M
    uint32_t free_mem = all_mem - used_mem;     //空闲内存大小，总共可用内存大小减去已使用内存
    uint16_t all_free_pages = free_mem / PG_SIZE;   //空余的页数 = 总空余内存 / 一页的大小

    uint16_t kernel_free_pages = all_free_pages /2; //内核和用户平分内存，各15M
    uint16_t user_free_pages = all_free_pages - kernel_free_pages; //万一是奇数 就会少1 减去即可

    uint32_t kbm_length = kernel_free_pages / 8;    //内核内存池的bitmap长度
    uint32_t ubm_length = user_free_pages / 8;      //用户内存池的bitmap长度

    uint32_t kp_start = used_mem;               //内核物理内存池的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;     //用户物理内存池的起始地址

    kernel_pool.phy_addr_start = kp_start;      //初始化内核内存池和用户内存池所管理的物理内存的起始地址
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;        //初始化内核内存池和用户内存池所管理的物理内存容量
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;        //初始化内核内存池和用户内存池对应bitmap的长度
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length; 

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;      //初始化内核内存池和用户内存池对应bitmap在内存中的位置
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    put_str("kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_char(0x0d);
    put_str("kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_char(0x0d);
    put_str("user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_char(0x0d);
    put_str("user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_char(0x0d);

    bitmap_init(&kernel_pool.pool_bitmap);      //初始化内核内存池和用户内存池对应bitmap的数值，初始化为全0
    bitmap_init(&user_pool.pool_bitmap);

    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);    //初始化内核虚拟内存池，将其安排在内核物理内存池和用户物理内存池之后
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;      //初始化内核虚拟内存池bitmap长度，长度设为与内核物理内存池一样

    kernel_vaddr.vaddr_start = K_HEAP_START;        //初始化虚拟内存池的起始地址
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    put_str("mem_pool_init done\n");
    return;
}

/*内存管理部分初始化入口*/
void mem_init()
{
    put_str("mem_init start!\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); //我们把总内存的值放在了0xb00 
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done!\n");
    return;
}

//得到1页大小内存并复制到页表中 专门针对fork时虚拟地址位图无需操作
//因为位图我们后面会复制父进程的 所以当然不用继续对虚拟位图操作了
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    //分配失败
    if(page_phyaddr == NULL)
    {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}
