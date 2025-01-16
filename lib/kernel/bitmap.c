#include "bitmap.h"            //��������
#include "global.h"            //global.h �����
#include "string.h"            //memset����Ҫ��
#include "interrupt.h"         
#include "print.h"             
#include "debug.h"

/*��ʼ��λͼ��������ÿ��λ����Ϊ0*/
void bitmap_init(struct bitmap* btmp){
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/*�ж�λͼ�еĵ�bit_idx��λ�Ƿ�Ϊ1�����򷵻�true*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_index = bit_idx % 8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_index));
}

/*��λͼ����������cnt��λ���ɹ��򷵻��������ʼ�±꣬ʧ�ܷ���-1*/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt){
    uint32_t idx_byte = 0;      //idx_byte�洢�п���λ���ֽ�����������±�
    /*��bitmap������ֽڱȽϣ��ҵ�����λ�����ֽ�*/
    while( (0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len) ){
        idx_byte++;
    }
    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if(idx_byte == btmp->btmp_bytes_len){
        return -1;
    }
    /*�ڿ���λ�����ֽ���λ�Ƚϣ��ҵ�����λ*/
    uint32_t idx_bit = 0; 
    while( (uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte] ){
        idx_bit++;
    }
    int bit_idx_start = idx_byte * 8 + idx_bit;     //����λ���±�
    /*�������λֻ��Ҫ1������ֱ�ӷ����±꼴��*/
    if(cnt == 1) return bit_idx_start;
    /*�������λ��Ҫ���������������߼��ж�*/
    uint32_t bit_left = (btmp->btmp_bytes_len * 8) - bit_idx_start;     //��¼bitmap��ʣ�¶��ٸ�λ
    uint32_t next_bit = bit_idx_start + 1;      //��¼��һ��λ��λ��
    bit_idx_start = -1;     //����Ϊ-1������Ҳ�������cnt��λ���У���ֱ�ӷ���-1
    uint32_t count = 1;     //��¼�Ѿ��ҵ�����������λ������
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

/*��λͼ�е�bit_idxλ����Ϊvalue*/
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