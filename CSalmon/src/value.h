#ifndef csalmon_value_h
#define csalmon_value_h

#include "common.h"

// “Obj”这个名称本身指的是一个结构体，它包含所有对象类型共享的状态。它有点像对象的“基类”。
typedef struct Obj Obj;
// 字符串的有效载荷定义在一个单独的结构体中。同样，我们需要对其进行前置声明。
typedef struct ObjString ObjString;

// 现在，我们将从最简单、最经典的解决方案开始：带标签的联合体。
// 一个值包含两个部分：一个类型“标签”，和一个实际值的有效载荷。为了存储值的类型，我们要为虚拟机支持的每一种值定义一个枚举。
typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ,	// 每个状态位于堆上的Lox值都是一个Obj。
} ValueType;

// 这个类型定义抽象了Salmon值在C语言中的具体表示方式。这样，我们就可以直接改变表示方法，而不需要回去修改现有的传递值的代码。
// 对于像整数这种固定大小的值，许多指令集直接将值存储在操作码之后的代码流中。这些指令被称为即时指令，因为值的比特位紧跟在操作码之后。
// 对于字符串这种较大的或可变大小的常量来说，这并不适用。
// 在本地编译器的机器码中，这些较大的常量会存储在二进制可执行文件中的一个单独的“常量数据”区域。
// 然后，加载常量的指令会有一个地址和偏移量，指向该值在区域中存储的位置。
// 每个字节码块都会携带一个在程序中以字面量形式出现的值的列表。为简单起见，我们会把所有的常量都放进去，甚至包括简单的整数。
// 
// 有一个字段用作类型标签，然后是第二个字段，一个包含所有底层值的联合体。
// 大多数体系结构都喜欢将值与它们的字长对齐。
// 由于联合体字段中包含一个8字节的double值，所以编译器在类型字段后添加了4个字节的填充，以使该double值保持在最近的8字节边界上。
// 
// 对于小的、固定大小的类型（如数字），有效载荷直接存储在Value结构本身。
// 如果对象比较大，它的数据就驻留在堆中。那么Value的有效载荷就是指向那块内存的一个指针。
typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;
} Value;

// 其中每个宏都接收一个适当类型的C值，并生成一个Value，其具有正确类型标签并包含底层的值。这就把静态类型的值提升到了clox的动态类型的世界。
#define BOOL_VAL(value)   (Value{VAL_BOOL, {.boolean = value}})
#define NIL_VAL           (Value{VAL_NIL, {.number = 0}})
#define OBJ_VAL(object)   (Value{VAL_OBJ, {.obj = (Obj*)object}})
#define NUMBER_VAL(value) (Value{VAL_NUMBER, {.number = value}})
// 但是为了能对Value做任何操作，我们需要将其拆包并取出对应的C值。
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_OBJ(value)     ((value).as.obj)
#define AS_NUMBER(value)  ((value).as.number)
// 除非我们知道Value包含适当的类型，否则使用任何的AS_宏都是不安全的。为此，我们定义最后几个宏来检查Value的类型。
// 如果Value具有对应类型，这些宏会返回true。每当我们调用一个AS_宏时，我们都需要保证首先调用了这些宏。
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

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
bool valuesEqual(Value a, Value b);
void printValue(Value value);

#endif