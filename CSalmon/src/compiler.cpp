#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

// 像我们要构建的单遍编译器并不是对所有语言都有效。
// 因为编译器在生产代码时只能“管窥”用户的程序，所以语言必须设计成不需要太多外围的上下文环境就能理解一段语法。
// 幸运的是，微小的、动态类型的Lox非常适合这种情况。

// 我们维护一个这种结构体类型的单一全局变量，所以我们不需要在编译器中将状态从一个函数传递到另一个函数。
typedef struct {
	Token current;
	Token previous;
	bool hadError;
	// 我还要引入另一个用于错误处理的标志。我们想要避免错误的级联效应。
	// 如果用户在他们的代码中犯了一个错误，而解析器又不理解它在语法中的含义，我们不希望解析器在第一个错误之后，又抛出一大堆无意义的连带错误。
	// 我们在C语言中没有异常。相反，我们会做一些欺骗性行为。我们添加一个标志来跟踪当前是否在紧急模式中。
	bool panicMode;
} Parser;

Parser parser;

// 我们正在写入的字节码块被传递给compile()，但是它也需要进入emitByte()中。要做到这一点，我们依靠这个中间函数。
// 现在，chunk指针存储在一个模块级变量中，就像我们存储其它全局状态一样。
// 以后，当我们开始编译用户定义的函数时，“当前块”的概念会变得更加复杂。为了避免到时候需要回头修改大量代码，我把这个逻辑封装在currentChunk()函数中。
Chunk* compilingChunk;

static Chunk* currentChunk() {
	return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
	// 当出现错误时，我们为其赋值。
	// 之后，我们继续进行编译，就像错误从未发生过一样。字节码永远不会被执行，所以继续运行也是无害的。
	// 诀窍在于，虽然设置了紧急模式标志，但我们只是简单地屏蔽了检测到的其它错误。
	// 解析器很有可能会崩溃，但是用户不会知道，因为错误都会被吞掉。
	// 当解析器到达一个同步点时，紧急模式就结束了。对于Lox，我们选择了语句作为边界，所以当我们稍后将语句添加到编译器时，将会清除该标志。
	if (parser.panicMode) return;
	parser.panicMode = true;

	// 首先，我们打印出错误发生的位置。
	fprintf(stderr, "[line %d] Error", token->line);

	// 如果词素是人类可读的，我们就尽量显示词素。然后我们打印错误信息
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR) {
		// Nothing.
	}
	else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	// 然后我们打印错误信息。
	fprintf(stderr, ": %s\n", message);
	// 之后，我们设置这个hadError标志。该标志记录了编译过程中是否有任何错误发生。这个字段也存在于解析器结构体中。
	parser.hadError = true;
}

// 如果扫描器交给我们一个错误标识，我们必须明确地告诉用户。
static void errorAtCurrent(const char* message) {
	// 我们从当前标识中提取位置信息，以便告诉用户错误发生在哪里，并将其转发给errorAt()。
	errorAt(&parser.current, message);
}

// 更常见的情况是，我们会在刚刚消费的令牌的位置报告一个错误。
static void error(const char* message) {
	errorAt(&parser.previous, message);
}

// 就像在jlox中一样，该函数向前通过标识流。它会向扫描器请求下一个词法标识，并将其存储起来以供后面使用。
// 在此之前，它会获取旧的current标识，并将其存储在previous字段中。这在以后会派上用场，让我们可以在匹配到标识之后获得词素。
static void advance() {
	parser.previous = parser.current;

	// 读取下一个标识的代码被包在一个循环中。
	// 记住，clox的扫描器不会报告词法错误。相反地，它创建了一个特殊的错误标识，让解析器来报告这些错误。我们这里就是这样做的。
	// 我们不断地循环，读取标识并报告错误，直到遇到一个没有错误的标识或者到达标识流终点。这样一来，解析器的其它部分只能看到有效的标记。
	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

// 它类似于advance()，都是读取下一个标识。但它也会验证标识是否具有预期的类型。如果不是，则报告错误。
static void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

// 在我们解析并理解了用户的一段程序之后，下一步是将其转换为一系列字节码指令。
static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn() {
	emitByte(OP_RETURN);
}

static void endCompiler() {
	emitReturn();
}

// 我们将字节码块传入，而编译器会向其中写入代码，如何compile()返回编译是否成功。
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);

	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	// 对advance()的调用会在扫描器上“启动泵”。
	advance();
	// 然后我们解析一个表达式。我们还不打算处理语句，所以表达式是我们支持的唯一的语法子集。
	expression();
	// 在编译表达式之后，我们应该处于源代码的末尾，所以我们要检查EOF标识。
	consume(TOKEN_EOF, "Expect end of expression.");

	endCompiler();

	// 如果发生错误，compile()应该返回false。
	return !parser.hadError;
}