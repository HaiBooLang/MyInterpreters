#ifndef csalmon_debug_h
#define csalmon_debug_h

#include "chunk.h"

// ���ǵ���disassembleChunk()������������ֽ�����е�����ָ�
void disassembleChunk(Chunk* chunk, const char* name);
// ��������һ������ʵ�ֵģ��ú���ֻ�����һ��ָ�
int disassembleInstruction(Chunk* chunk, int offset);

#endif