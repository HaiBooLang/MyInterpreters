#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

// 我们声明了一个全局VM对象。反正我们只需要一个虚拟机对象，这样可以让本书中的代码在页面上更轻便。
VM vm;

static void resetStack() {
	// 因为栈数组是直接在VM结构体中内联声明的，所以我们不需要为其分配空间。我们甚至不需要清除数组中不使用的单元——我们只有在值存入之后才会访问它们。
	// 我们需要的唯一的初始化操作就是将stackTop指向数组的起始位置，以表明栈是空的。
	vm.stackTop = vm.stack;
}

void initVM() {
	resetStack();
}

void freeVM() {
}

void push(Value value) {
	// 记住，stackTop刚刚跳过上次使用的元素，即下一个可用的元素。
	*vm.stackTop = value;
	vm.stackTop++;
}

Value pop() {
	vm.stackTop--;
	return *vm.stackTop;
}

static InterpretResult run() {
	// 为了使作用域更明确，宏定义本身要被限制在该函数中。我们在开始时定义了它们，然后因为我们比较关心，在结束时取消它们的定义。
	// READ_BYTE这个宏会读取ip当前指向字节，然后推进指令指针。
#define READ_BYTE() (*vm.ip++)
	// READ_CONTANT()从字节码中读取下一个字节，将得到的数字作为索引，并在代码块的常量表中查找相应的Value。
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
	// 围绕这个核心算术表达式的是一些模板代码，用于从栈中获取数值，并将结果结果压入栈中。
	// 这个宏需要扩展为一系列语句。作为一个谨慎的宏作者，我们要确保当宏展开时，这些语句都在同一个作用域内。
	// 在宏中使用do while循环看起来很滑稽，但它提供了一种方法，可以在一个代码块中包含多个语句，并且允许在末尾使用分号。
#define BINARY_OP(op) \
    do { \
      double b = pop(); \
      double a = pop(); \
      push(a op b); \
    } while (false)

	// 我们有一个不断进行的外层循环。每次循环中，我们会读取并执行一条字节码指令。
	for (;;) {
		// 定义了这个标志之后，虚拟机在执行每条指令之前都会反汇编并将其打印出来。
		// 每当我们追踪执行情况时，我们也会在解释每条指令之前展示栈中的当前内容。
#ifdef DEBUG_TRACE_EXECUTION
		printf("          ");
		// 我们循环打印数组中的每个值，从第一个值开始（栈底），到栈顶结束。这样我们可以观察到每条指令对栈的影响。
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");

		// 由于 disassembleInstruction() 方法接收一个整数offset作为字节偏移量，而我们将当前指令引用存储为一个直接指针，
		// 所以我们首先要做一个小小的指针运算，将ip转换成从字节码开始的相对偏移量。
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction = 0;
		// 为了处理一条指令，我们首先需要弄清楚要处理的是哪种指令。READ_BYTE这个宏会读取ip当前指向字节，然后推进指令指针。
		// 任何指令的第一个字节都是操作码。给定一个操作码，我们需要找到实现该指令语义的正确的C代码。这个过程被称为解码或指令分派。
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			// 你就知道产生一个值实际上意味着什么：将它压入栈。
			Value constant = READ_CONSTANT();
			push(constant);
			break;
		}
		case OP_ADD:      BINARY_OP(+); break;	// 这四条指令之间唯一的区别是，它们最终使用哪一个底层C运算符来组合两个操作数。
		case OP_SUBTRACT: BINARY_OP(-); break;
		case OP_MULTIPLY: BINARY_OP(*); break;
		case OP_DIVIDE:   BINARY_OP(/); break;
		case OP_NEGATE:  push(-pop()); break;	// 该指令需要操作一个值，该值通过弹出栈获得。它对该值取负，然后把结果重新压入栈，以便后面的指令使用。
		case OP_RETURN: {
			printValue(pop());
			printf("\n");
			return INTERPRET_OK;
		}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
	compile(source);
	return INTERPRET_OK;
	//// 我们通过将ip指向块中的第一个字节码来对其初始化。
	//// 在虚拟机执行的整个过程中都是如此：IP总是指向下一条指令，而不是当前正在处理的指令。
	//vm.ip = vm.chunk->code;
	//return run();
}