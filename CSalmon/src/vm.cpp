#include <stdio.h>
#include "common.h"
#include "debug.h"
#include "vm.h"

// 我们声明了一个全局VM对象。反正我们只需要一个虚拟机对象，这样可以让本书中的代码在页面上更轻便。
VM vm;

void initVM() {
}

void freeVM() {
}

static InterpretResult run() {
	// 为了使作用域更明确，宏定义本身要被限制在该函数中。我们在开始时定义了它们，然后因为我们比较关心，在结束时取消它们的定义。
	// READ_BYTE这个宏会读取ip当前指向字节，然后推进指令指针。
#define READ_BYTE() (*vm.ip++)
	// READ_CONTANT()从字节码中读取下一个字节，将得到的数字作为索引，并在代码块的常量表中查找相应的Value。
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

	// 我们有一个不断进行的外层循环。每次循环中，我们会读取并执行一条字节码指令。
	for (;;) {
		// 定义了这个标志之后，虚拟机在执行每条指令之前都会反汇编并将其打印出来。
		// 由于 disassembleInstruction() 方法接收一个整数offset作为字节偏移量，而我们将当前指令引用存储为一个直接指针，
		// 所以我们首先要做一个小小的指针运算，将ip转换成从字节码开始的相对偏移量。
#ifdef DEBUG_TRACE_EXECUTION
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction = 0;
		// 为了处理一条指令，我们首先需要弄清楚要处理的是哪种指令。READ_BYTE这个宏会读取ip当前指向字节，然后推进指令指针。
		// 任何指令的第一个字节都是操作码。给定一个操作码，我们需要找到实现该指令语义的正确的C代码。这个过程被称为解码或指令分派。
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			Value constant = READ_CONSTANT();
			printValue(constant);
			printf("\n");
			break;
		}
		case OP_RETURN: {
			return INTERPRET_OK;
		}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk) {
	vm.chunk = chunk;
	// 我们通过将ip指向块中的第一个字节码来对其初始化。
	// 在虚拟机执行的整个过程中都是如此：IP总是指向下一条指令，而不是当前正在处理的指令。
	vm.ip = vm.chunk->code;
	return run();
}