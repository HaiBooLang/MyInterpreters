#ifndef csalmon_value_h
#define csalmon_value_h

#include "common.h"

// 这个类型定义抽象了Salmon值在C语言中的具体表示方式。这样，我们就可以直接改变表示方法，而不需要回去修改现有的传递值的代码。
// 对于像整数这种固定大小的值，许多指令集直接将值存储在操作码之后的代码流中。这些指令被称为即时指令，因为值的比特位紧跟在操作码之后。
// 对于字符串这种较大的或可变大小的常量来说，这并不适用。
// 在本地编译器的机器码中，这些较大的常量会存储在二进制可执行文件中的一个单独的“常量数据”区域。
// 然后，加载常量的指令会有一个地址和偏移量，指向该值在区域中存储的位置。
// 每个字节码块都会携带一个在程序中以字面量形式出现的值的列表。为简单起见，我们会把所有的常量都放进去，甚至包括简单的整数。
typedef double Value;

// ------------------常量池------------------
// 常量池是一个值的数组。加载常量的指令根据数组中的索引查找该数组中的值。
// 与字节码数组一样，编译器也无法提前知道这个数组需要多大。因此，我们需要一个动态数组。
typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

// ------------------------------------
void printValue(Value value);

#endif