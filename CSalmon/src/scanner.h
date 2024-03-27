#ifndef csalmon_scanner_h
#define csalmon_scanner_h

// 我们用一个枚举来标记它是什么类型的词法标识——数字、标识符、+运算符等等。这个枚举与jlox中的枚举几乎完全相同，所以我们直接来敲定整个事情。
// 除了在所有名称前都加上TOKEN_前缀（因为C语言会将枚举名称抛出到顶层命名空间）之外，唯一的区别就是多了一个TOKEN_ERROR类型。
// 在扫描过程中只会检测到几种错误：未终止的字符串和无法识别的字符。在jlox中，扫描器会自己报告这些错误。
// 在clox中，扫描器会针对这些错误生成一个合成的“错误”标识，并将其传递给编译器。这样一来，编译器就知道发生了一个错误，并可以在报告错误之前启动错误恢复。
// 在jlox中，每个Token将词素保存到其单独的Java字符串中。如果我们在clox中也这样做，我们就必须想办法管理这些字符串的内存。
// 这非常困难，因为我们是通过值传递词法标识的——多个标识可能指向相同的词素字符串。所有权会变得混乱。
// 相反，我们将原始的源码字符串作为我们的字符存储。我们用指向第一个字符的指针和其中包含的字符数来表示一个词素。
// 这意味着我们完全不需要担心管理词素的内存，而且我们可以自由地复制词法标识。只要主源码字符串的寿命超过所有词法标识，一切都可以正常工作。
typedef enum {
	// Single-character tokens. 单字符词法
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
	TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
	TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
	// One or two character tokens. 一或两字符词法
	TOKEN_BANG, TOKEN_BANG_EQUAL,
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_LESS, TOKEN_LESS_EQUAL,
	// Literals. 字面量
	TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
	// Keywords. 关键字
	TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
	TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
	TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
	TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

	TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	const char* start;
	int length;
	int line;
} Token;

void initScanner(const char* source);
// 该函数的每次调用都会扫描并返回源代码中的下一个词法标识。
Token scanToken();

#endif