#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // ��newSizeΪ0ʱ������ͨ������free()���Լ�������յ������
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    // ��������£�����������C��׼���realloc()������
    // �ú������Է����֧�����ǲ����е�����������������oldSizeΪ0ʱ��realloc() ��ͬ�ڵ���malloc()��
    // ��Ȥ������ǵ�oldSize��newSize����Ϊ0ʱ�����ǻ����realloc()Ҫ����֮ǰ����Ŀ�Ĵ�С��
    // ����µĴ�СС�����е��ڴ�飬����ֻ�Ǹ��¿�Ĵ�С�������ش����ָ�롣����¿��С�������ͻ᳢���������е��ڴ�顣
    // ���û�пռ�֧�ֿ��������realloc()�����һ�������С���µ��ڴ�飬���ƾɵ��ֽڣ��ͷž��ڴ�飬Ȼ�󷵻�һ��ָ�����ڴ���ָ�롣
    void* result = realloc(pointer, newSize);

    // ������ǵ�VM���ܵõ�������Ҫ���ڴ棬�Ǿ�������ʲô���õ�����.
    // ���������ٿ��Լ����һ�㣬��������ֹ���̣������Ƿ���һ��NULLָ�룬Ȼ���ó�������ƫ������
    if (result == NULL) exit(1);

    return result;
}