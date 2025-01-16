#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#include "global.h"
#include "bitmap.h"
#include "sync.h"
#include "list.h"


/*�����ַ��*/
struct virtual_addr{
    struct bitmap vaddr_bitmap;            //�����ַҪ�õ���bitmap
    uint32_t vaddr_start;   //�����ַ����ʼ��ַ
};

/*�ڴ�ر�ǣ������ж����ĸ��ڴ��*/
enum pool_flags
{
    PF_KERNEL = 1,  //�ں��ڴ��
    PF_USER = 2     //�û��ڴ��
};

/*�ڴ��*/
struct mem_block{
    struct list_elem free_elem;     //list_elem������Ԫ��
};

/*�ڴ��������*/
struct mem_block_desc
{
    uint32_t block_size;            //�ڴ���С������16�ֽڣ�64�ֽ�
    uint32_t block_per_arena;       //��arena�п����ɴ�mem_block������
    struct list free_list;	        //Ŀǰ���õ�mem_block����
};

/*�ڴ�ֿ�*/
struct arena
{
    struct mem_block_desc* desc;        //��arena������mem_block_desc
    uint32_t cnt;           //largeΪtrue��cnt����ҳ������Ϊfalse���������mem_block����
    bool large; 
};

/*�ڴ�ؽṹ*/
struct pool{
    struct bitmap pool_bitmap;      //�ڴ�������������ڴ��bitmap
    uint32_t phy_addr_start;    //���ڴ��������������ڴ����ʼ��ַ
    uint32_t pool_size;     //���ڴ�ص��ֽ�����
    struct lock lock;       //pool���������ڴ�ʱ����
};

#define PG_P_1 1        //ҳ�����ҳĿ¼��Ĵ�������λ
#define PG_P_0 0        //��������λ
#define PG_RW_R 0       //��/ִ��
#define PG_RW_W 2       //��/д/ִ��
#define PG_US_S 0       //ϵͳ��
#define PG_US_U 4       //�û���

#define DESC_CNT 7      //�ڴ���������ĸ���

/*�ڴ�����ֳ�ʼ�����*/
void mem_init();
/*���������ַvaddr��Ӧ��PTEָ�룬��ָ��Ҳ�������ַ��ʾ*/
uint32_t* pte_ptr(uint32_t vaddr);
/*���������ַvaddr��Ӧ��PDEָ�룬��ָ��Ҳ�������ַ��ʾ*/
uint32_t* pde_ptr(uint32_t vaddr);
/*��m_poolָ��������ڴ���з���1������ҳ���ɹ��򷵻ط��������ҳ�������ַ*/
void* palloc(struct pool* m_pool);
/*��������ַ_vaddr�������ַ_page_phyaddr��ӳ��*/
void page_table_add(void* _vaddr,void* _page_phyaddr);
/*��pf��ָ��������ڴ�����У��������pg_cnt��ҳ���ɹ��򷵻����뵽���׸������ַ*/
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
/*���ں������ڴ��������pg_cntҳ�ڴ棬�ɹ��򷵻��������ַ*/
void* get_kernel_pages(uint32_t pg_cnt);
/*��ʼ���ں˺��û����ڴ��*/
static void mem_pool_init(uint32_t all_mem);
/*���û��ռ�������pg_cntҳ���ڴ棬�������������ַ*/
void* get_user_pages(uint32_t pg_cnt);
/*���û��ռ�����ں˿ռ����һҳ�ڴ棬���������ַvaddr������ӳ��*/
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
/* �õ������ַӳ��������ַ */
uint32_t addr_v2p(uint32_t vaddr);
/*�ڴ���ʼ��*/
void block_desc_init(struct mem_block_desc* desc_array);
//����arena�е�idx���ڴ��ĵ�ַ �ռ䲼��Ϊ arenaԪ��Ϣ+n���ڴ��
struct mem_block* arena2block(struct arena* a,uint32_t idx);
//�����ڴ��b���ڵ�arena��ַ
struct arena* block2arena(struct mem_block* b);
//�ڶ�������size�ֽ��ڴ�
void* sys_malloc(uint32_t size);
/*�������ַpg_phy_addr���յ������ڴ��*/
void pfree(uint32_t pg_phy_addr);
/*ȥ��ҳ���������ַvaddr��ӳ�䣬ֻȥ��vaddr��Ӧ��pte*/
void page_table_pte_remove(uint32_t vaddr);
/*�������ַ�����ͷ���_vaddr��ʼ������pg_cnt������ҳ��ַ*/
void vaddr_remove(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt);
/*�ͷ��������ַvaddrΪ��ʼ��cnt������ҳ��*/
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt);
/*�����ڴ�ptr*/
void sys_free(void* ptr);
//�õ�1ҳ��С�ڴ沢���Ƶ�ҳ���� ר�����forkʱ�����ַλͼ�������
//��Ϊλͼ���Ǻ���Ḵ�Ƹ����̵� ���Ե�Ȼ���ü���������λͼ������
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf,uint32_t vaddr);


#endif