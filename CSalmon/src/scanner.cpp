#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

// 当我们的扫描器一点点处理用户的源代码时，它会跟踪自己已经走了多远。
// 就像我们在虚拟机中所做的那样，我们将状态封装在一个结构体中，然后创建一个该类型的顶层模块变量，这样就不必在所有的函数之间传递它。
// 我们甚至没有保留指向源代码字符串起点的指针。扫描器只处理一遍代码，然后就结束了。
typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
	// 我们从第一行的第一个字符开始，就像一个运动员蹲在起跑线上。
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

// 这个函数依赖于几个辅助函数，其中大部分都是在jlox中已熟悉的。
static bool isAtEnd() {
	return *scanner.current == '\0';
}

static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	// 其中使用扫描器的start和current指针来捕获标识的词素。
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

// 唯一的区别在于，“词素”指向错误信息字符串而不是用户的源代码。
// 同样，我们需要确保错误信息能保持足够长的时间，以便编译器能够读取它。
// 在实践中，我们只会用C语言的字符串字面量来调用这个函数。它们是恒定不变的，所以我们不会有问题。
static Token errorToken(const char* message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	return token;
}

Token scanToken() {
	// 由于对该函数的每次调用都会扫描一个完整的词法标识，所以当我们进入该函数时，就知道我们正处于一个新词法标识的开始处。
	scanner.start = scanner.current;

	// 然后检查是否已达到源代码的结尾。如果是，我们返回一个EOF标识并停止。这是一个标记值，它向编译器发出信号，停止请求更多标记。
	if (isAtEnd()) return makeToken(TOKEN_EOF);

	// 如果我们没有达到结尾，我们会做一些……事情……来扫描下一个标识。

	// 如果这段代码没有成功扫描并返回一个词法标识，那么我们就到达了函数的终点。
	// 这肯定意味着我们遇到了一个扫描器无法识别的字符，所以我们为此返回一个错误标识。
	return errorToken("Unexpected character.");
}