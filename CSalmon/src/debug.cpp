#include <stdio.h>

#include "debug.h"
#include "value.h"

// Ҫ�����һ���ֽ���飬�������ȴ�ӡһ��С���⣨�������Ǿ�֪�����ڿ��ĸ��ֽ���飩��Ȼ��ͨ���ֽ��뷴���ÿ��ָ�
void disassembleChunk(Chunk* chunk, const char* name) {
	printf("== %s ==\n", name);

	for (int offset = 0; offset < chunk->count;) {
		// �����ǵ��øú���ʱ���ڶԸ���ƫ������λ�÷����ָ��󣬻᷵����һ��ָ���ƫ������������Ϊ�����Ǻ���Ҳ�ῴ����ָ������в�ͬ�Ĵ�С��
		offset = disassembleInstruction(chunk, offset);
	}
}

static int simpleInstruction(const char* name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%-16s %4d '", name, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 2;
}

int disassembleInstruction(Chunk* chunk, int offset) {
	// ���ȣ������ӡ����ָ����ֽ�ƫ�����������ܸ������ǵ�ǰָ�����ֽ�����е�λ�á����������ֽ�����ʵ�ֿ���������תʱ���⽫��һ�����õ�·�ꡣ
	printf("%04d ", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	}
	else {
		printf("%4d ", chunk->lines[offset]);
	}

	// �������������ֽ����еĸ���ƫ��������ȡһ���ֽڡ���Ҳ�������ǵĲ����롣
	uint8_t instruction = chunk->code[offset];
	// ���Ǹ��ݸ�ֵ��switch������
	switch (instruction) {
		// ����ÿһ��ָ����Ƕ����ɸ�һ��С�Ĺ��ߺ�����չʾ����
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset);
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
		// ����������ֽڿ�������������һ��ָ����������Ǳ�������һ�����󡪡�����ҲҪ��ӡ������
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}