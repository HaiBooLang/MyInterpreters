#ifndef csalmon_debug_h
#define csalmon_debug_h

#include "chunk.h"

// 我们调用disassembleChunk()来反汇编整个字节码块中的所有指令。
void disassembleChunk(Chunk* chunk, const char* name);
// 这是用另一个函数实现的，该函数只反汇编一条指令。
int disassembleInstruction(Chunk* chunk, int offset);

#endif