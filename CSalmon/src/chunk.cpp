#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
}

void writeChunk(Chunk* chunk, uint8_t byte) {
	// ������Ҫ���ĵ�һ�����ǲ鿴��ǰ�����Ƿ��Ѿ����������ֽڵ�������
	// ���û�У���ô����������Ҫ�����������ڳ��ռ䣨�����ǵ�һ��д��ʱ������ΪNULL����capacityΪ0��Ҳ���������������
	if (chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		// Ҫ�������飬��������Ҫ�����������Ȼ�������������䵽�ô�С��
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code,
			oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->count++;
}

void freeChunk(Chunk* chunk) {
	// �����ͷ����е��ڴ棬Ȼ�����initChunk()���ֶ����㣬ʹ�ֽ���鴦��һ��������ȷ�Ŀ�״̬��
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	initChunk(chunk);
}