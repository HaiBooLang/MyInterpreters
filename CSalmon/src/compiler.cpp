#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

// 我们将字节码块传入，而编译器会向其中写入代码，如何compile()返回编译是否成功。
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);

	// 对advance()的调用会在扫描器上“启动泵”。
	advance();
	// 然后我们解析一个表达式。我们还不打算处理语句，所以表达式是我们支持的唯一的语法子集。
	expression();
	// 在编译表达式之后，我们应该处于源代码的末尾，所以我们要检查EOF标识。
	consume(TOKEN_EOF, "Expect end of expression.");
}