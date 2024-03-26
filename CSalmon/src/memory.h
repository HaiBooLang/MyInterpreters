#ifndef csalmon_memory_h
#define csalmon_memory_h

#include "common.h"

// ��������ݸ����ĵ�ǰ����������µ�������
// Ϊ�˻��������Ҫ�����ܣ���Ҫ�Ĳ��־��ǻ��ھ�������С������չ��������2��ϵ������������һ�����͵�ȡֵ��1.5������һ��������ѡ��
// ���ǻ��ᴦ��ǰ����Ϊ0�����������������£����ǵ�����ֱ������8�������Ǵ�1��ʼ��
// ��ͱ�����������ǳ�С��ʱ����ֶ�����ڴ沨�����������ڷǳ�С�Ŀ����˷Ѽ����ֽڡ�
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// һ������֪����������������Ϳ���ʹ��GROW_ARRAY()�������������鵽�ô�С��
// �������˶�reallocate()�����ĵ��ã������Ĺ���������������ɵġ�
// �걾�����ȡ����Ԫ�����͵Ĵ�С���������ɵ�void*ת������ȷ���͵�ָ�롣
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

// ��GROW_ARRAY()���ƣ����Ƕ�reallocate()���õİ�װ���������ͨ������0��Ϊ�µ��ڴ���С�����ͷ��ڴ档
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// ���reallocate()���������ǽ���clox���������ж�̬�ڴ�����Ψһ�������������ڴ棬�ͷ��ڴ��Լ��ı����з���Ĵ�С��
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif