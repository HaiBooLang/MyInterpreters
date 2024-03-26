#ifndef csalmon_chunk_h
#define csalmon_chunk_h

#include "common.h"
#include "memory.h"

// �����ǵ��ֽ����ʽ�У�ÿ��ָ���һ���ֽڵĲ����루ͨ�����Ϊopcode����������ֿ�������Ҫ�����ָ�����͡����ӡ��������ұ����ȡ�
typedef enum {
	OP_RETURN,
} Opcode;

// �ֽ�����һϵ��ָ����գ����ǻ���ָ��һ��洢һЩ�������ݣ����������Ǽ�������һ���ṹ��������������Щ���ݡ�
// ���������ڿ�ʼ�����֮ǰ��֪��������Ҫ��������������Ƕ�̬�ġ���̬����������ϲ�������ݽṹ֮һ��
// ��̬�����ṩ�ˣ������Ѻã��ܼ��洢������Ԫ�ز���Ϊ����ʱ�临�Ӷȡ�����ĩβ׷��Ԫ��Ϊ����ʱ�临�Ӷȡ�
typedef struct {
	int count;
	int capacity;
	uint8_t* code;
} Chunk;

void initChunk(Chunk* chunk);

void writeChunk(Chunk* chunk, uint8_t byte);

void freeChunk(Chunk* chunk);

//class Chunk {
//private:
//	size_t _count;
//	size_t _capacity;
//	std::unique_ptr<uint8_t> _code;
//
//public:
//	Chunk(size_t count,size_t capacity,uint8_t* code)
//		:_count(count),_capacity(capacity),_code(std::move(code)) {}
//};

#endif