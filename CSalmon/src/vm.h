#ifndef csalmon_vm_h
#define csalmon_vm_h

#include "chunk.h"

// 虚拟机是我们解释器内部结构的一部分。你把一个代码块交给它，它就会运行这块代码。VM的代码和数据结构放在一个新的模块中。
typedef struct {
	Chunk* chunk;
	// 当虚拟机运行字节码时，它会记录它在哪里——即当前执行的指令所在的位置。
	// 我们没有在run()方法中使用局部变量来进行记录，因为最终其它函数也会访问该值。
	// “IP”这个名字很传统，而且与CS中的很多传统名称不同的是，它是有实际意义的：它是一个指令指针。
	// 几乎世界上所有的指令集，不管是真实的还是虚拟的，都有一个类似的寄存器或变量。
	uint8_t* ip;
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
InterpretResult interpret(Chunk* chunk);

#endif