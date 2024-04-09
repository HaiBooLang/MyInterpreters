#ifndef csalmon_chunk_h
#define csalmon_chunk_h

#include "common.h"
#include "memory.h"
#include "value.h"

// 在我们的字节码格式中，每个指令都有一个字节的操作码（通常简称为opcode）。这个数字控制我们要处理的指令类型——加、减、查找变量等。
typedef enum {
	OP_RETURN,
	// 当VM执行常量指令时，它会“加载”常量以供使用。我们的字节码像大多数其它字节码一样，允许指令有操作数。
	// 这些操作数以二进制数据的形式存储在指令流的操作码之后，让我们对指令的操作进行参数化。
	// 每个操作码会定义它有多少操作数以及各自的含义。
	OP_CONSTANT,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	// --------------------------------
	// 二元操作符
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	// --------------------------------
	// 一元操作符
	OP_NOT,
	OP_NEGATE,
} Opcode;

// 字节码是一系列指令。最终，我们会与指令一起存储一些其它数据，所以让我们继续创建一个结构体来保存所有这些数据。
// 由于我们在开始编译块之前不知道数组需要多大，所以它必须是动态的。动态数组是我最喜欢的数据结构之一。
// 动态数组提供了：缓存友好，密集存储、索引元素查找为常量时间复杂度、数组末尾追加元素为常量时间复杂度。
typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	int* lines;				// 该数组与字节码平级。数组中的每个数字都是字节码中对应字节所在的行号。。
	ValueArray constants;	// 保存字节码块中的常量值。
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);
// 我们定义一个便捷的方法来向字节码块中添加一个新常量。
int addConstant(Chunk* chunk, Value value);

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