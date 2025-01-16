#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "sync.h"
#include "interrupt.h"

#define PG_SIZE 4096    //ҳ�Ĵ�СΪ4096�ֽ�
#define MEM_BITMAP_BASE 0xc009a000      //���̵߳�PCB��0xc009e000��bitmap�趨Ϊ֧�ֹ���512M�ڴ棬����ռ4ҳ����ȥ4000
#define K_HEAP_START 0xc0100000     //֮ǰ�ѵ�һ��ҳ���ǰ256��ӳ��Ϊ�˵Ͷ�1M���ڴ棬���԰Ѷ�������1M���ϱ��ֽ���

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)   //��ȡaddr�ĸ�10λ,��PDE��������
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)   //��ȡaddr����10λ����PTE��������


struct mem_block_desc k_block_descs[DESC_CNT];  //�ں��ڴ������������
struct pool kernel_pool, user_pool; //�������ں������ڴ�غ��û������ڴ��
struct virtual_addr kernel_vaddr;   //�ں˵������ڴ��

/*��pf��ʶ���ڴ��������pg_cnt������ҳ���ɹ��򷵻�����ҳ�ĵ�ַ��ʧ�ܷ���NULL*/
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
    int vaddr_start = 0,bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL)     //�ں˳�
    {
    	bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt); //��bitmap����������λ������ɹ��򷵻س�ʼλ���±�
    	if(bit_idx_start == -1)	return NULL;
    	while(cnt < pg_cnt)
    	bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),1);   //��bit_idx_start��ʼ����pg_cnt��λ����Ϊ1
    	vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;   //���뵽�������ַ��ʼҳ�������ַ��ʼλ��+bitmap���뵽����ʼλ*4K
    }
    else        //�û��أ���ʵ��
    {
        struct task_struct* cur = running_thread();     //ÿ�����̶����Լ��������
    	bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
    	if(bit_idx_start == -1)	return NULL;
    	while(cnt < pg_cnt)
    	bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + (cnt++),1);
    	vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    	ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}

/*���������ַvaddr��Ӧ��PTEָ�룬��ָ��Ҳ�������ַ��ʾ*/
uint32_t* pte_ptr(uint32_t vaddr)
{
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

/*���������ַvaddr��Ӧ��PDEָ�룬��ָ��Ҳ�������ַ��ʾ*/
uint32_t* pde_ptr(uint32_t vaddr)
{
    uint32_t* pde = (uint32_t*) ((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

/*��m_poolָ��������ڴ���з���1������ҳ���ɹ��򷵻ط��������ҳ�������ַ*/
void* palloc(struct pool* m_pool)
{
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
    if(bit_idx == -1)	return NULL;
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

/*��������ַ_vaddr�������ַ_page_phyaddr��ӳ��*/
void page_table_add(void* _vaddr,void* _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr,page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    
    if(*pde & 0x00000001)   //��������ַvaddr��Ӧ��pde����λΪ1
    {
    	ASSERT(!(*pte & 0x00000001));
    	if(!(*pte & 0x00000001))        //��������ַvaddr��Ӧ��pte�����ڣ��򽫸�pte�Ĵ���λ��Ϊ1�������������ַ��ӳ��д��
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	else
    	{
    	    PANIC("pte repeat");
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	}
    } 
    else        //��������ַvaddr��Ӧ��pde����λΪ0����������
    {
    	uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);      //����palloc����һ���µ�����ҳ��������ҳ���������µ�ҳ������pde��
    	*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      
    	memset((void*)((int)pte & 0xfffff000),0,PG_SIZE);
    	ASSERT(!(*pte & 0x00000001));
    	*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
    return;
}

/*��pf��ָ��������ڴ�����У��������pg_cnt��ҳ���ɹ��򷵻����뵽���׸������ַ*/
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);    /*�ں˺��û�ƽ��30M�ڴ棬���������ҳ�����ܳ���15M��Ӧ3840ҳ*/
    
    void* vaddr_start = vaddr_get(pf,pg_cnt);   /*�������ڴ�������������ַ*/
    if(vaddr_start == NULL)	return NULL;
    
    
    uint32_t vaddr = (uint32_t)vaddr_start,cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool; /*�жϸ������ڴ���������ں˻����û�*/
    
    while(cnt-- > 0)
    {
    	void* page_phyaddr = palloc(mem_pool);      /*��������ҳ�����������ַ*/
    	if(page_phyaddr == NULL)	return NULL;
    	page_table_add((void*)vaddr,page_phyaddr);  /*��� ����������ַ������������ַ��ӳ��*/
    	vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/*���ں������ڴ��������pg_cntҳ�ڴ棬�ɹ��򷵻��������ַ*/
void* get_kernel_pages(uint32_t pg_cnt)
{
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE); /*�������ɹ����򽫷��䵽������ҳȫ����*/
    return vaddr;
}

/*���û��ռ�������pg_cntҳ���ڴ棬�������������ַ*/
void* get_user_pages(uint32_t pg_cnt){
    lock_acquire(&user_pool.lock);	//�û����̿��ܻ������ͻ �󲿷�ʱ�䶼���û����� �ں˽��̿��������������ͻ
    void* vaddr = malloc_page(PF_USER,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/*���û��ռ�����ں˿ռ����һҳ�ڴ棬���������ַvaddr������ӳ��*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    
    //�����ַλͼ��1
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

/* �õ������ַӳ��������ַ */
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/*�ڴ���ʼ��*/
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

//����arena�е�idx���ڴ��ĵ�ַ �ռ䲼��Ϊ arenaԪ��Ϣ+n���ڴ��
struct mem_block* arena2block(struct arena* a,uint32_t idx)
{
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

//�����ڴ��b���ڵ�arena��ַ
struct arena* block2arena(struct mem_block* b)
{
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

//�ڶ�������size�ֽ��ڴ�
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
    //size �����ռ��С����Ϊ 0 ����
    if(!(size > 0 && size < pool_size))
    {
    	return NULL;
    }
    
    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);
    
    //����ҳ��ֱ�ӷ���ҳ��
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
	    return (void*)(a+1); //���ص�ַ struct arena + sizeof(struct arena)
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
    	
    	//�Ѿ�����
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

/*�������ַpg_phy_addr���յ������ڴ��*/
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
    bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);	//ȫ����0
}

/*ȥ��ҳ���������ַvaddr��ӳ�䣬ֻȥ��vaddr��Ӧ��pte*/
void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    *pte = ~PG_P_1;
    asm volatile("invlpg %0"::"m"(vaddr):"memory");	//����tlb
}

/*�������ַ�����ͷ���_vaddr��ʼ������pg_cnt������ҳ��ַ*/
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

/*�ͷ��������ַvaddrΪ��ʼ��cnt������ҳ��*/
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

/*�����ڴ�ptr*/
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


/*��ʼ���ں˺��û����ڴ��*/
static void mem_pool_init(uint32_t all_mem){
    put_str("mem_pool_init start!\n");
    uint32_t page_table_size = PG_SIZE * 256;       //ҳĿ¼���ҳ��ռ�õĴ�С��ҳĿ¼��ռ1��ҳ�򣬵�0��768��ҳĿ¼��ӳ��ĵ�һ��ҳ��ռһ��ҳ��769��1022ҳĿ¼��ӳ����254��ҳ���ܹ�256��ҳ��
    uint32_t used_mem = page_table_size + 0x100000; //��¼��ǰ��ʹ�õ��ڴ棬�Ͷ�1M����ҳĿ¼��ҳ��ռ�õĴ�С,Ŀǰ2M
    uint32_t free_mem = all_mem - used_mem;     //�����ڴ��С���ܹ������ڴ��С��ȥ��ʹ���ڴ�
    uint16_t all_free_pages = free_mem / PG_SIZE;   //�����ҳ�� = �ܿ����ڴ� / һҳ�Ĵ�С

    uint16_t kernel_free_pages = all_free_pages /2; //�ں˺��û�ƽ���ڴ棬��15M
    uint16_t user_free_pages = all_free_pages - kernel_free_pages; //��һ������ �ͻ���1 ��ȥ����

    uint32_t kbm_length = kernel_free_pages / 8;    //�ں��ڴ�ص�bitmap����
    uint32_t ubm_length = user_free_pages / 8;      //�û��ڴ�ص�bitmap����

    uint32_t kp_start = used_mem;               //�ں������ڴ�ص���ʼ��ַ
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;     //�û������ڴ�ص���ʼ��ַ

    kernel_pool.phy_addr_start = kp_start;      //��ʼ���ں��ڴ�غ��û��ڴ��������������ڴ����ʼ��ַ
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;        //��ʼ���ں��ڴ�غ��û��ڴ��������������ڴ�����
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;        //��ʼ���ں��ڴ�غ��û��ڴ�ض�Ӧbitmap�ĳ���
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length; 

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;      //��ʼ���ں��ڴ�غ��û��ڴ�ض�Ӧbitmap���ڴ��е�λ��
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

    bitmap_init(&kernel_pool.pool_bitmap);      //��ʼ���ں��ڴ�غ��û��ڴ�ض�Ӧbitmap����ֵ����ʼ��Ϊȫ0
    bitmap_init(&user_pool.pool_bitmap);

    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);    //��ʼ���ں������ڴ�أ����䰲�����ں������ڴ�غ��û������ڴ��֮��
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;      //��ʼ���ں������ڴ��bitmap���ȣ�������Ϊ���ں������ڴ��һ��

    kernel_vaddr.vaddr_start = K_HEAP_START;        //��ʼ�������ڴ�ص���ʼ��ַ
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    put_str("mem_pool_init done\n");
    return;
}

/*�ڴ�����ֳ�ʼ�����*/
void mem_init()
{
    put_str("mem_init start!\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); //���ǰ����ڴ��ֵ������0xb00 
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done!\n");
    return;
}

//�õ�1ҳ��С�ڴ沢���Ƶ�ҳ���� ר�����forkʱ�����ַλͼ�������
//��Ϊλͼ���Ǻ���Ḵ�Ƹ����̵� ���Ե�Ȼ���ü���������λͼ������
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    //����ʧ��
    if(page_phyaddr == NULL)
    {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}
