#ifndef csalmon_vm_h
#define csalmon_vm_h

#include "chunk.h"
#include "value.h"

// 给我们的虚拟机一个固定的栈大小，意味着某些指令系列可能会压入太多的值并耗尽栈空间——典型的“堆栈溢出”。
#define STACK_MAX 256

// 虚拟机是我们解释器内部结构的一部分。你把一个代码块交给它，它就会运行这块代码。VM的代码和数据结构放在一个新的模块中。
typedef struct {
	Chunk* chunk;
	// 当虚拟机运行字节码时，它会记录它在哪里——即当前执行的指令所在的位置。
	// 我们没有在run()方法中使用局部变量来进行记录，因为最终其它函数也会访问该值。
	// “IP”这个名字很传统，而且与CS中的很多传统名称不同的是，它是有实际意义的：它是一个指令指针。
	// 几乎世界上所有的指令集，不管是真实的还是虚拟的，都有一个类似的寄存器或变量。
	uint8_t* ip;
	// 在基于堆栈的虚拟机中执行指令是非常简单的。在后面的章节中，你还会发现，将源语言编译成基于栈的指令集是小菜一碟。
	// 但是，这种架构的速度快到足以在产生式语言的实现中使用。这感觉就像是在编程语言游戏中作弊。
	Value stack[STACK_MAX];
	Value* stackTop;
} VM;

// 当我们有一个报告静态错误的编译器和检测运行时错误的VM时，解释器会通过它来知道如何设置进程的退出代码。
typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

// VM会逐步获取到一大堆它需要跟踪的状态，所以我们现在定义一个结构，把这些状态都塞进去。
void initVM();
void freeVM();
// 我们已经得到了Lox源代码字符串，所以现在我们准备建立一个管道来扫描、编译和执行它。管道是由interpret()驱动的。
InterpretResult interpret(const char* source);

void push(Value value);
Value pop();

#endif