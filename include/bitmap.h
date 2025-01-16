#ifndef LIB_KERNEL_BITMAP
#define LIB_KERNEL_BITMAP

#include "global.h"
#define BITMAP_MASK 1

struct bitmap{
    uint32_t btmp_bytes_len;        //���ֽ�Ϊ��λ��bitmap����
    uint8_t* bits;              //λͼ��ָ�룬��¼λͼ��λ��
};

/*��ʼ��λͼ��������ÿ��λ����Ϊ0*/
void bitmap_init(struct bitmap* btmp);
/*�ж�λͼ�еĵ�bit_idx��λ�Ƿ�Ϊ1�����򷵻�true*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
/*��λͼ����������cnt��λ���ɹ��򷵻��������ʼ�±꣬ʧ�ܷ���-1*/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
/*��λͼ�е�bit_idxλ����Ϊvalue*/
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif