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

// 为了读取下一个字符，我们使用一个新的辅助函数，它会消费当前字符并将其返回。
static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

static char peek() {
	return *scanner.current;
}

// 这就像peek()一样，但是是针对当前字符之后的一个字符。
static char peekNext() {
	if (isAtEnd()) return '\0';
	return scanner.current[1];
}

// 如果当前字符是所需的字符，则指针前进并返回true。否则，我们返回false表示没有匹配。
static bool match(char expected) {
	if (isAtEnd()) return false;
	if (*scanner.current != expected) return false;
	scanner.current++;
	return true;
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

// 这将使扫描器跳过所有的前置空白字符。在这个调用返回后，我们知道下一个字符是一个有意义的字符（或者我们到达了源代码的末尾）。
// 这有点像一个独立的微型扫描器。它循环，消费遇到的每一个空白字符。我们需要注意的是，它不会消耗任何非空白字符。
static void skipWhitespace() {
	for (;;) {
		char c = peek();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;
		case '\n':
			// 当我们消费换行符时，也会增加当前行数。
			scanner.line++;
			advance();
			break;
		case '/':
			// Lox中的注释以//开头，因此与!=类似，我们需要前瞻第二个字符。
			if (peekNext() == '/') {
				// 我们使用peek()来检查换行符，但是不消费它。
				// 这样一来，换行符将成为skipWhitespace()外部下一轮循环中的当前字符，我们就能识别它并增加scanner.line。
				while (peek() != '\n' && !isAtEnd()) advance();
			}
			else {
				return;
			}
			break;
		default:
			return;
		}
	}
}

// 在clox中，词法标识只存储词素——即用户源代码中出现的字符序列。稍后在编译器中，当我们准备将其存储在字节码块中的常量表中时，我们会将词素转换为运行时值。
static Token string() {
	// 我们消费字符，直到遇见右引号。我们也会追踪字符串字面量中的换行符（Lox支持多行字符串）。
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n') scanner.line++;
		advance();
	}

	// 并且，与之前一样，我们会优雅地处理在找到结束引号之前源代码耗尽的问题。
	if (isAtEnd()) return errorToken("Unterminated string.");

	advance();
	return makeToken(TOKEN_STRING);
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

// 它与jlox版本几乎是相同的，只是我们还没有将词素转换为浮点数。
static Token number() {
	while (isDigit(peek())) advance();

	// 寻找小数部分。
	if (peek() == '.' && isDigit(peekNext())) {
		// 消费 "."。
		advance();

		while (isDigit(peek())) advance();
	}

	return makeToken(TOKEN_NUMBER);
}

static bool isAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// 我们将此用于树中的所有无分支路径。一旦我们发现一个前缀，其只有可能是一种保留字，我们需要验证两件事。词素必须与关键字一样长。
// 如果我们字符数量确实正确，并且它们是我们想要的字符，那这就是一个关键字，我们返回相关的标识类型。否则，它必然是一个普通的标识符。
static TokenType checkKeyword(int start, int length,
	const char* rest, TokenType type) {
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	// 字典树会存储一组字符串。大多数其它用于存储字符串的数据结构都包含原始字符数组，然后将它们封装在一些更大的结果中，以帮助你更快地搜索。
	// 字典树则不同，在其中你找不到一个完整的字符串。相应地，字典树中“包含”的每个字符串被表示为通过字符树中节点的路径。
	// 字典树是一种更基本的数据结构的特殊情况：确定性有限状态机（deterministic finite automaton ，DFA）。
	// 你可能还知道它的其它名字：有限状态机，或就叫状态机。状态机是非常重要的，从游戏编程到实现网络协议的一切方面都很有用。
	// 在DFA中，你有一组状态，它们之间有转换，形成一个图。在任何时间点，机器都“处于”其中一个状态。它通过转换过渡到其它状态。
	// 当你使用DFA进行词法分析时，每个转换都是从字符串中匹配到的一个字符。每个状态代表一组允许的字符。
	// 我们的关键字树正是一个能够识别Lox关键字的DFA。但是DFA比简单的树更强大，因为它们可以是任意的图。转换可以在状态之间形成循环。这让你可以识别任意长的字符串。
	// 然而，手工完成这种巨型DFA是一个巨大的挑战。这就是Lex诞生的原因。
	// 你给它一个关于语法的简单文本描述——一堆正则表达式——它就会自动为你生成一个DFA，并生成一堆实现它的C代码。
	// 我们就不走这条路了。我们已经有了一个完全可用的简单扫描器。我们只需要一个很小的字典树来识别关键字。
	// 我们不会为每个节点都增加一个switch语句。相反，我们有一个工具函数来测试潜在关键字词素的剩余部分。
	switch (scanner.start[0]) {
	case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);			// 这些是对应于单个关键字的首字母。
	case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
	case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
	case 'f':														// 我们有几个关键字是在第一个字母之后又有树的分支。
		// 在我们进入switch语句之前，需要先检查是否有第二个字母。毕竟，“f”本身也是一个有效的标识符。
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
			case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
			case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
			}
		}
		break;
	case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
	case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
	case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
	case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
	case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
	case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
	case 't':
		if (scanner.current - scanner.start > 1) {
			switch (scanner.start[1]) {
			case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
			case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
			}
		}
		break;
	case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
	case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
	}
	return TOKEN_IDENTIFIER;
}

// 一旦我们发现一个标识符，我们就通过下面的方法扫描其余部分。
// 在第一个字母之后，我们也允许使用数字，并且我们会一直消费字母数字，直到消费完为止。然后我们生成一个具有适当类型的词法标识。
static Token identifier() {
	while (isAlpha(peek()) || isDigit(peek())) advance();
	return makeToken(identifierType());
}

Token scanToken() {
	// 我们的扫描器需要处理空格、制表符和换行符，但是这些字符不会成为任何标识词素的一部分。
	// 我们可以在scanToken()中的主要的字符switch语句中检查这些字符，但要想确保当你调用该函数时，它仍然能正确地找到空白字符后的下一个标识，这就有点棘手了。
	// 我们必须将整个函数封装在一个循环或其它东西中。
	skipWhitespace();

	// 由于对该函数的每次调用都会扫描一个完整的词法标识，所以当我们进入该函数时，就知道我们正处于一个新词法标识的开始处。
	scanner.start = scanner.current;

	// 然后检查是否已达到源代码的结尾。如果是，我们返回一个EOF标识并停止。这是一个标记值，它向编译器发出信号，停止请求更多标记。
	if (isAtEnd()) return makeToken(TOKEN_EOF);

	// 如果我们没有达到结尾，我们会做一些……事情……来扫描下一个标识。
	char c = advance();

	if (isAlpha(c)) return identifier();
	if (isDigit(c)) return number();

	switch (c) {
	case '(': return makeToken(TOKEN_LEFT_PAREN);									// 最简单的词法标识只有一个字符。
	case ')': return makeToken(TOKEN_RIGHT_PAREN);
	case '{': return makeToken(TOKEN_LEFT_BRACE);
	case '}': return makeToken(TOKEN_RIGHT_BRACE);
	case ';': return makeToken(TOKEN_SEMICOLON);
	case ',': return makeToken(TOKEN_COMMA);
	case '.': return makeToken(TOKEN_DOT);
	case '-': return makeToken(TOKEN_MINUS);
	case '+': return makeToken(TOKEN_PLUS);
	case '/': return makeToken(TOKEN_SLASH);
	case '*': return makeToken(TOKEN_STAR);
	case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);			// 接下来是两个字符的符号。
	case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
	case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
	case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
	case '"': return string();														// 数字和字符串标识比较特殊，因为它们有一个与之关联的运行时值。
	}

	// 如果这段代码没有成功扫描并返回一个词法标识，那么我们就到达了函数的终点。
	// 这肯定意味着我们遇到了一个扫描器无法识别的字符，所以我们为此返回一个错误标识。
	return errorToken("Unexpected character.");
}