#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"


// 我们声明了一个全局VM对象。反正我们只需要一个虚拟机对象，这样可以让本书中的代码在页面上更轻便。
VM vm;

static void resetStack() {
	// 因为栈数组是直接在VM结构体中内联声明的，所以我们不需要为其分配空间。我们甚至不需要清除数组中不使用的单元——我们只有在值存入之后才会访问它们。
	// 我们需要的唯一的初始化操作就是将stackTop指向数组的起始位置，以表明栈是空的。
	vm.stackTop = vm.stack;
}

// 这本书不是C语言教程，所以我在这里略过了，但是基本上是...和va_list让我们可以向runtimeError()传递任意数量的参数。
// 它将这些参数转发给vfprintf()，这是printf()的一个变体，需要一个显式地va_list。
// 调用者可以向runtimeError()传入一个格式化字符串，后跟一些参数，就像他们直接调用printf()一样。然后runtimeError()格式化并打印这些参数。
static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	// 在显示了希望有帮助的错误信息之后，我们还会告诉用户，当错误发生时正在执行代码中的哪一行。
	// 因为我们在编译器中留下了标识，所以我们可以从编译到字节码块中的调试信息中查找行号。
	// 如果我们的编译器正确完成了它的工作，就能对应到字节码被编译出来的那一行源代码。
	// 我们使用当前字节码指令索引减1来查看字节码块的调试行数组。这是因为解释器在之前每条指令之前都会向前推进。
	// 所以，当我们调用 runtimeError()，失败的指令就是前一条。
	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);
	resetStack();
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

// 它从堆栈中返回一个Value，但是并不弹出它。distance参数是指要从堆栈顶部向下看多远：0是栈顶，1是下一个槽，以此类推。
static Value peek(int distance) {
	return vm.stackTop[-1 - distance];
}

// 对于一元取负，我们把对任何非数字的东西进行取负当作一个错误。
// 但是Lox，像大多数脚本语言一样，在涉及到!和其它期望出现布尔值的情况下，是比较宽容的。处理其它类型的规则被称为“falsiness”。
// Lox遵循Ruby的规定，nil和false是假的，其它的值都表现为true。
static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	ObjString* b = AS_STRING(pop());
	ObjString* a = AS_STRING(pop());

	// 首先，我们根据操作数的长度计算结果字符串的长度。
	// 我们为结果分配一个字符数组，然后将两个部分复制进去。与往常一样，我们要小心地确保这个字符串被终止了。
	int length = a->length + b->length;
	char* chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	// 最后，我们生成一个ObjString来包含这些字符。这次我们使用一个新函数takeString()。
	ObjString* result = takeString(chars, length);
	push(OBJ_VAL(result));
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
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
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
		case OP_NIL:		push(NIL_VAL); break;
		case OP_TRUE:		push(BOOL_VAL(true)); break;
		case OP_FALSE:		push(BOOL_VAL(false)); break;
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();
			// 你可以对任意一对对象执行==，即使这些对象是不同类型的。这有足够的复杂性，所以有必要把这个逻辑分流到一个单独的函数中。
			// 这个函数会一个C语言的bool值，所以我们可以安全地把结果包装在一个BOLL_VAL中。这个函数与Value有关，所以它位于“value”模块中。
			push(BOOL_VAL(valuesEqual(a, b)));
			break;
		}
		case OP_GREATER:  BINARY_OP(BOOL_VAL, > ); break;
		case OP_LESS:     BINARY_OP(BOOL_VAL, < ); break;
		case OP_ADD: {	// 这四条指令之间唯一的区别是，它们最终使用哪一个底层C运算符来组合两个操作数。
			// 如果两个操作数都是字符串，则连接。如果都是数字，则相加。任何其它操作数类型的组合都是一个运行时错误。
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a + b));
			}
			else {
				runtimeError("Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SUBTRACT:	BINARY_OP(NUMBER_VAL, -); break;
		case OP_MULTIPLY:	BINARY_OP(NUMBER_VAL, *); break;
		case OP_DIVIDE:		BINARY_OP(NUMBER_VAL, / ); break;
		case OP_NOT:		push(BOOL_VAL(isFalsey(pop()))); break;
		case OP_NEGATE:	// 该指令需要操作一个值，该值通过弹出栈获得。它对该值取负，然后把结果重新压入栈，以便后面的指令使用。
			// 首先，我们检查栈顶的Value是否是一个数字。如果不是，则报告运行时错误并停止解释器。
			if (!IS_NUMBER(peek(0))) {
				runtimeError("Operand must be a number.");
				return INTERPRET_RUNTIME_ERROR;
			}
			// 否则，我们就继续运行。只有在验证之后，我们才会拆装操作数，取负，将结果封装并压入栈。
			push(NUMBER_VAL(-AS_NUMBER(pop())));
			break;
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
	// 我们创建一个新的空字节码块，并将其传递给编译器。
	Chunk chunk;
	initChunk(&chunk);

	// 编译器会获取用户的程序，并将字节码填充到该块中。
	if (!compile(source, &chunk)) {
		// 如果遇到错误，compile()方法会返回false，我们就会丢弃不可用的字节码块。
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	// 否则，我们将完整的字节码块发送到虚拟机中去执行。
	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	InterpretResult result = run();

	// 当虚拟机完成后，我们会释放该字节码块，这样就完成了。
	freeChunk(&chunk);
	return result;
}