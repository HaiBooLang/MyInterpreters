#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

// 为了把“优先级”作为一个参数，我们用数值来定义它。
// 这些是Salmon中的所有优先级，按照从低到高的顺序排列。由于C语言会隐式地为枚举赋值连续递增的数字，这就意味着PREC_CALL在数值上比PREC_UNARY要大。
typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_OR,          // or
	PREC_AND,         // and
	PREC_EQUALITY,    // == !=
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,        // + -
	PREC_FACTOR,      // * /
	PREC_UNARY,       // ! -
	PREC_CALL,        // . ()
	PREC_PRIMARY
} Precedence;

// 这个ParseFn类型是一个简单的函数类型定义，这类函数不需要任何参数且不返回任何内容。
typedef void (*ParseFn)();

// 我们还知道，我们需要一个表格，给定一个标识类型，可以从中找到：
// 编译以该类型标识为起点的前缀表达式的函数，编译一个左操作数后跟该类型标识的中缀表达式的函数，以及使用该标识作为操作符的中缀表达式的优先级。
typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

static void grouping();
static void unary();
static void binary();
static void number();
static void literal();
static void string();


// 你可以看到grouping和unary是如何被插入到它们各自标识类型对应的前缀解析器列中的。
// 在下一列中，binary被连接到四个算术中缀操作符上。这些中缀操作符的优先级也设置在最后一列。
// 除此之外，表格的其余部分都是NULL和PREC_NONE。这些空的单元格中大部分是因为没有与这些标识相关联的表达式。
class Rules {
public:
	Rules() :rules(40)
	{
		rules[TOKEN_LEFT_PAREN] = { grouping, NULL,   PREC_NONE };
		rules[TOKEN_RIGHT_PAREN] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_LEFT_BRACE] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_RIGHT_BRACE] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_COMMA] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_DOT] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_MINUS] = { unary,    binary, PREC_TERM };
		rules[TOKEN_PLUS] = { NULL,     binary, PREC_TERM };
		rules[TOKEN_SEMICOLON] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_SLASH] = { NULL,     binary, PREC_FACTOR };
		rules[TOKEN_STAR] = { NULL,     binary, PREC_FACTOR };
		rules[TOKEN_BANG] = { unary,    NULL,   PREC_NONE };
		rules[TOKEN_BANG_EQUAL] = { NULL,     binary, PREC_EQUALITY };
		rules[TOKEN_EQUAL] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_EQUAL_EQUAL] = { NULL,     binary,   PREC_EQUALITY };
		rules[TOKEN_GREATER] = { NULL,     binary,   PREC_COMPARISON };
		rules[TOKEN_GREATER_EQUAL] = { NULL,     binary,   PREC_COMPARISON };
		rules[TOKEN_LESS] = { NULL,     binary,   PREC_COMPARISON };
		rules[TOKEN_LESS_EQUAL] = { NULL,     binary,   PREC_COMPARISON };
		rules[TOKEN_IDENTIFIER] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_STRING] = { string,   NULL,   PREC_NONE };
		rules[TOKEN_NUMBER] = { number,   NULL,   PREC_NONE };
		rules[TOKEN_AND] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_CLASS] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_ELSE] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_FALSE] = { literal,  NULL,   PREC_NONE };
		rules[TOKEN_FOR] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_FUN] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_IF] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_NIL] = { literal,  NULL,   PREC_NONE };
		rules[TOKEN_OR] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_PRINT] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_RETURN] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_SUPER] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_THIS] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_TRUE] = { literal,  NULL,   PREC_NONE };
		rules[TOKEN_VAR] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_WHILE] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_ERROR] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_EOF] = { NULL,     NULL,   PREC_NONE };
	}

	ParseRule& operator[](int index) {
		return rules[index];
	}
private:
	std::vector<ParseRule> rules;
};

Rules rules;

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

// 如果当前标识符合给定的类型，check()函数返回true。
// 将它封装在一个函数中似乎有点傻，但我们以后会更多地使用它，而且我们认为像这样简短的动词命名的函数使解析器更容易阅读。
static bool check(TokenType type) {
	return parser.current.type == type;
}

// 如果当前的标识是指定类型，我们就消耗该标识并返回true。否则，我们就不处理该标识并返回false。
static bool match(TokenType type) {
	if (!check(type)) return false;
	advance();
	return true;
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

static uint8_t makeConstant(Value value) {
	// 它将给定的值添加到字节码块的常量表的末尾，并返回其索引。这个新函数的工作主要是确保我们没有太多常量。
	// 由于OP_CONSTANT指令使用单个字节来索引操作数，所以我们在一个块中最多只能存储和加载256个常量。
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}

static void emitConstant(Value value) {
	// 首先，我们将值添加到常量表中，然后我们发出一条OP_CONSTANT指令，在运行时将其压入栈中。
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	// 只有在代码没有错误的情况下，我们才会这样做。
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary() {
	// 当前缀解析函数被调用时，前缀标识已经被消耗了。中缀解析函数被调用时，情况更进一步——整个左操作数已经被编译，而随后的中缀操作符也已经被消耗掉。
	// 首先左操作数已经被编译的事实是很好的。这意味着在运行时，其代码已经被执行了。当它运行时，它产生的值最终进入栈中。而这正是中缀操作符需要它的地方。
	// 每个二元运算符的右操作数的优先级都比自己高一级。
	// 我们可以通过getRule()动态地查找，我们很快就会讲到。有了它，我们就可以使用比当前运算符高一级的优先级来调用parsePrecedence()。
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule->precedence + 1));

	// 然后我们使用binary()来处理算术操作符的其余部分。
	// 这个函数会编译右边的操作数，就像unary()编译自己的尾操作数那样。最后，它会发出执行对应二元运算的字节码指令。
	// 当运行时，虚拟机会按顺序执行左、右操作数的代码，将它们的值留在栈上。然后它会执行操作符的指令。
	// 这时，会从栈中弹出这两个值，计算结果，并将结果推入栈中。
	switch (operatorType) {
	case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
	case TOKEN_GREATER:       emitByte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS:          emitByte(OP_LESS); break;
	case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS:          emitByte(OP_ADD); break;
	case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
	case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
	case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
	default: return; // Unreachable.
	}
}

static void literal() {
	// 因为parsePrecedence()已经消耗了关键字标识，我们需要做的就是输出正确的指令。我们根据解析出的标识的类型来确定指令。
	switch (parser.previous.type) {
	case TOKEN_FALSE: emitByte(OP_FALSE); break;
	case TOKEN_NIL: emitByte(OP_NIL); break;
	case TOKEN_TRUE: emitByte(OP_TRUE); break;
	default: return; // Unreachable.
	}
}

static void grouping() {
	// 我们假定初始的(已经被消耗了。我们递归地调用expression()来编译括号之间的表达式，然后解析结尾的)。
	// 就后端而言，分组表达式实际上没有任何意义。它的唯一功能是语法上的——它允许你在需要高优先级的地方插入一个低优先级的表达式。
	// 因此，它本身没有运行时语法，也就不会发出任何字节码。对expression()的内部调用负责为括号内的表达式生成字节码。
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void expression() {
	// 我们只需要解析最低优先级，它也包含了所有更高优先级的表达式。
	parsePrecedence(PREC_ASSIGNMENT);
}

// 这个函数接受给定的标识，并将其词素作为一个字符串添加到字节码块的常量表中。然后，它会返回该常量在常量表中的索引。
static uint8_t identifierConstant(Token* name) {
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}
 
static uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	return identifierConstant(&parser.previous);
}

// 它会输出字节码指令，用于定义新变量并存储其初始化值。变量名在常量表中的索引是该指令的操作数。
// 在基于堆栈的虚拟机中，我们通常是最后发出这条指令。
// 在运行时，我们首先执行变量初始化器的代码，将值留在栈中。然后这条指令会获取该值并保存起来，以供日后使用。
static void defineVariable(uint8_t global) {
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static void varDeclaration() {
	// 关键字后面跟着变量名。它是由parseVariable()编译的。
	uint8_t global = parseVariable("Expect variable name.");

	if (match(TOKEN_EQUAL)) {
		// 然后我们会寻找一个=，后跟初始化表达式。
		expression();
	}
	else {
		// 如果用户没有初始化变量，编译器会生成OP_NIL指令隐式地将其初始化为nil。
		emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON,
		"Expect ';' after variable declaration.");

	// 全局变量在运行时是按名称查找的。这意味着虚拟机（字节码解释器循环）需要访问该名称。
	// 整个字符串太大，不能作为操作数塞进字节码流中。相反，我们将字符串存储到常量表中，然后指令通过该名称在表中的索引来引用它。
	defineVariable(global);
}

static void synchronize() {
	parser.panicMode = false;

	// 我们会不分青红皂白地跳过标识，直到我们到达一个看起来像是语句边界的位置。
	// 我们识别边界的方式包括，查找可以结束一条语句的前驱标识，如分号；
	// 或者我们可以查找能够开始一条语句的后续标识，通常是控制流或声明语句的关键字之一。
	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;
		switch (parser.current.type) {
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;

		default:
			; // Do nothing.
		}

		advance();
	}
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}
	// 与jlox一样，clox也使用了恐慌模式下的错误恢复来减少它所报告的级联编译错误。
	// 当编译器到达同步点时，就退出恐慌模式。对于Lox来说，我们选择语句边界作为同步点。
	// 如果我们在解析前一条语句时遇到编译错误，我们就会进入恐慌模式。当这种情况发生时，我们会在这条语句之后开始同步。
	if (parser.panicMode) synchronize();
}

static void printStatement() {
	// print语句会对表达式求值并打印出结果，所以我们首先解析并编译这个表达式。
	expression();
	// 语法要求在表达式之后有一个分号，所以我们消耗一个分号标识。
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	// 最后，我们生成一条新指令来打印结果。
	emitByte(OP_PRINT);
}

// “表达式语句”就是一个表达式后面跟着一个分号。这是在需要语句的上下文中写表达式的方式。
// 通常来说，这样你就可以调用函数或执行赋值操作以触发其副作用。
// 从语义上说，表达式语句会对表达式求值并丢弃结果。编译器直接对这种行为进行编码。它会编译表达式，然后生成一条OP_POP指令。
static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else {
		expressionStatement();
	}
}

// 为了编译数值字面量，我们在数组的TOKEN_NUMBER索引处存储一个指向下面函数的指针。
static void number() {
	// 我们假定数值字面量标识已经被消耗了，并被存储在previous中。我们获取该词素，并使用C标准库将其转换为一个double值。
	double value = strtod(parser.previous.start, NULL);
	// 然后我们用下面的函数生成加载该double值的字节码。
	emitConstant(NUMBER_VAL(value));
}

static void string() {
	// 这里直接从词素中获取字符串的字符。+1和-2部分去除了开头和结尾的引号。然后，它创建了一个字符串对象，将其包装为一个Value，并塞入常量表中。
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void unary() {
	// 前导的-标识已经被消耗掉了，并被放在parser.previous中。我们从中获取标识类型，以了解当前正在处理的是哪个一元运算符。
	TokenType operatorType = parser.previous.type;

	// 我们使用一元运算符本身的PREC_UNARY优先级来允许嵌套的一元表达式。
	// 因为一元运算符的优先级很高，所以正确地排除了二元运算符之类的东西。
	parsePrecedence(PREC_UNARY);

	// 之后，我们发出字节码执行取负运算。
	switch (operatorType) {
	case TOKEN_BANG: emitByte(OP_NOT); break;
	case TOKEN_MINUS: emitByte(OP_NEGATE); break;
	default: return; // Unreachable.
	}
}

// 这个函数（一旦实现）从当前的标识开始，解析给定优先级或更高优先级的任何表达式。
static void parsePrecedence(Precedence precedence) {
	// 我们读取下一个标识并查找对应的ParseRule。如果没有前缀解析器，那么这个标识一定是语法错误。我们会报告这个错误并返回给调用方。
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	// 如果下一个标识的优先级太低，或者根本不是一个中缀操作符，我们就结束了。我们已经尽可能多地解析了表达式。
	if (prefixRule == NULL) {
		error("Expect expression.");
		return;
	}

	// 否则，我们就调用前缀解析函数，让它做自己的事情。该前缀解析器会编译表达式的其余部分，消耗它需要的任何其它标识，然后返回这里。
	prefixRule();
	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

// 它只是简单地返回指定索引处的规则。
static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

// 我们将字节码块传入，而编译器会向其中写入代码，如何compile()返回编译是否成功。
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);

	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	// 对advance()的调用会在扫描器上“启动泵”。
	advance();

	// 我们会一直编译声明语句，直到到达源文件的结尾。
	while (!match(TOKEN_EOF)) {
		declaration();
	}

	endCompiler();

	// 如果发生错误，compile()应该返回false。
	return !parser.hadError;
}