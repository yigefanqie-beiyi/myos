#ifndef LIB_KERNEL_BITMAP
#define LIB_KERNEL_BITMAP

#include "global.h"
#define BITMAP_MASK 1

struct bitmap{
    uint32_t btmp_bytes_len;        //以字节为单位的bitmap长度
    uint8_t* bits;              //位图的指针，记录位图的位置
};

/*初始化位图，将里面每个位都设为0*/
void bitmap_init(struct bitmap* btmp);
/*判断位图中的第bit_idx个位是否为1，是则返回true*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
/*在位图中申请连续cnt个位，成功则返回申请的起始下标，失败返回-1*/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
/*将位图中的bit_idx位设置为value*/
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif