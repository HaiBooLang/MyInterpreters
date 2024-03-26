#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
}

void writeChunk(Chunk* chunk, uint8_t byte) {
	// 我们需要做的第一件事是查看当前数组是否已经有容纳新字节的容量。
	// 如果没有，那么我们首先需要扩充数组以腾出空间（当我们第一个写入时，数组为NULL并且capacity为0，也会遇到这种情况）
	if (chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		// 要扩充数组，首先我们要算出新容量，然后将数组容量扩充到该大小。
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code,
			oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->count++;
}

void freeChunk(Chunk* chunk) {
	// 我们释放所有的内存，然后调用initChunk()将字段清零，使字节码块处于一个定义明确的空状态。
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	initChunk(chunk);
}