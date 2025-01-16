#include "bitmap.h"            //函数定义
#include "global.h"            //global.h 不清楚
#include "string.h"            //memset函数要用
#include "interrupt.h"         
#include "print.h"             
#include "debug.h"

/*初始化位图，将里面每个位都设为0*/
void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/*判断位图中的第bit_idx个位是否为1，是则返回true*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_index = bit_idx % 8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_index));
}

/*在位图中申请连续cnt个位，成功则返回申请的起始下标，失败返回-1*/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt){
    uint32_t idx_byte = 0;      //idx_byte存储有空闲位的字节所在数组的下标
    /*在bitmap中逐个字节比较，找到空闲位所在字节*/
    while( (0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len) ){
        idx_byte++;
    }
    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if(idx_byte == btmp->btmp_bytes_len){
        return -1;
    }
    /*在空闲位所在字节逐位比较，找到空闲位*/
    uint32_t idx_bit = 0; 
    while( (uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte] ){
        idx_bit++;
    }
    int bit_idx_start = idx_byte * 8 + idx_bit;     //空闲位的下标
    /*如果申请位只需要1个，则直接返回下标即可*/
    if(cnt == 1) return bit_idx_start;
    /*如果申请位需要连续多个，则进行逻辑判断*/
    uint32_t bit_left = (btmp->btmp_bytes_len * 8) - bit_idx_start;     //记录bitmap还剩下多少个位
    uint32_t next_bit = bit_idx_start + 1;      //记录下一个位的位置
    bit_idx_start = -1;     //先置为-1，如果找不到连续cnt个位空闲，则直接返回-1
    uint32_t count = 1;     //记录已经找到的连续空闲位的数量
    while(bit_left-- > 0){
        if(!(bitmap_scan_test(btmp, next_bit))){
            count++;
        }
        else{
            count = 0;
        }
        if(count == cnt){
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

/*将位图中的bit_idx位设置为value*/
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value){
    ASSERT(value == 0 || value == 1);
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_index = bit_idx % 8;
    if(value){
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_index);
    }
    else{
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_index);
    }
}