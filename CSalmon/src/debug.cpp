#include <stdio.h>

#include "debug.h"
#include "value.h"

// 要反汇编一个字节码块，我们首先打印一个小标题（这样我们就知道正在看哪个字节码块），然后通过字节码反汇编每个指令。
void disassembleChunk(Chunk* chunk, const char* name) {
	printf("== %s ==\n", name);

	for (int offset = 0; offset < chunk->count;) {
		// 当我们调用该函数时，在对给定偏移量的位置反汇编指令后，会返回下一条指令的偏移量。这是因为，我们后面也会看到，指令可以有不同的大小。
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
	// 首先，它会打印给定指令的字节偏移量——这能告诉我们当前指令在字节码块中的位置。当我们在字节码中实现控制流和跳转时，这将是一个有用的路标。
	printf("%04d ", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	}
	else {
		printf("%4d ", chunk->lines[offset]);
	}

	// 接下来，它从字节码中的给定偏移量处读取一个字节。这也就是我们的操作码。
	uint8_t instruction = chunk->code[offset];
	// 我们根据该值做switch操作。
	switch (instruction) {
		// 对于每一种指令，我们都分派给一个小的工具函数来展示它。
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset);
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
		// 如果给定的字节看起来根本不像一条指令——这是我们编译器的一个错误——我们也要打印出来。
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}