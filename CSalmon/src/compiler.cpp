#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// 你会注意到，我们写的几乎所有的代码都在编译器中。在运行时，只有两个小指令。
// 你会看到，相比于jlox，这是clox中的一个持续的趋势。优化器工具箱中最大的锤子就是把工作提前到编译器中，这样你就不必在运行时做这些工作了。
// 在本章中，这意味着要准确地解析每个局部变量占用的栈槽。这样，在运行时就不需要进行查找或解析。

// 当我们谈论“控制流”时，我们指的是什么？我们所说的“流”是指执行过程在程序文本中的移动方式。
// 在jlox中，机器人的关注点（当前代码位）是隐式的，它取决于哪些AST节点被存储在各种Java变量中，以及我们正在运行的Java代码是什么。
// 在clox中，它要明确得多。VM的ip字段存储了当前字节码指令的地址。该字段的值正是我们在程序中的“位置”。
// 执行操作通常是通过增加ip进行的。但是我们可以随意地改变这个变量。为了实现控制流，所需要做的就是以更有趣的方式改变ip。
// 要想跳过一大块代码，我们只需将ip字段设置为其后代码的字节码指令的地址。为了有条件地跳过一些代码，我们需要一条指令来查看栈顶的值。
// 如果它是假，就在ip上增加一个给定的偏移量，跳过一系列指令。否则，它什么也不做，并照常执行下一条指令。
// 当我们编译成字节码时，代码中显式的嵌套块结构就消失了，只留下一系列扁平的指令。Lox是一种结构化的编程语言，但clox字节码却不是。
// 正确的（或者说错误的，取决于你怎么看待它）字节码指令集可以跳转到代码块的中间位置，或从一个作用域跳到另一个作用域。

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
// 我们正向一个解析函数传递参数。但是这些函数是存储在一个函数指令表格中的，所以所有的解析函数需要具有相同的类型。
typedef void (*ParseFn)(bool canAssign);

// 我们还知道，我们需要一个表格，给定一个标识类型，可以从中找到：
// 编译以该类型标识为起点的前缀表达式的函数，编译一个左操作数后跟该类型标识的中缀表达式的函数，以及使用该标识作为操作符的中缀表达式的优先级。
typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

// 我们存储变量的名称。当我们解析一个标识符时，会将标识符的词素与每个局部变量名称进行比较，以找到一个匹配项。
typedef struct {
	Token name;
	// depth字段记录了声明局部变量的代码块的作用域深度。这就是我们现在需要的所有状态。
	int depth;
} Local;

// 在jlox中，我们使用“环境”HashMap链来跟踪当前在作用域中的局部变量。
// 这是一种经典的、教科书式的词法作用域表示方式。对于clox，像往常一样，我们更接近于硬件。所有的状态都保存了一个新的结构体中。
// 我们有一个简单、扁平的数组，其中包含了编译过程中每个时间点上处于作用域内的所有局部变量。
// 它们在数组中的顺序与它们的声明在代码中出现的顺序相同。
// 由于我们用来编码局部变量的指令操作数是一个字节，所以我们的虚拟机对同时处于作用域内的局部变量的数量有一个硬性限制。
// 这意味着我们也可以给局部变量数组一个固定的大小。
typedef struct {
	Local locals[UINT8_COUNT];
	// localCount字段记录了作用域中有多少局部变量——有多少个数组槽在使用。
	int localCount;
	// 我们还会跟踪“作用域深度”。这指的是我们正在编译的当前代码外围的代码块数量。
	// 0是全局作用域，1是第一个顶层块，2是它内部的块，你懂的。我们用它来跟踪每个局部变量属于哪个块，这样当一个块结束时，我们就知道该删除哪些局部变量。
	int scopeDepth;
} Compiler;

// 如果我们是有原则的工程师，我们应该给前端的每个函数添加一个参数，接受一个指向Compiler的指针。
// 我们在一开始就创建一个Compiler，并小心地在将它贯穿于每个函数的调用中……
// 但这意味着要对我们已经写好的代码进行大量无聊的修改，所以这里用一个全局变量代替。
Compiler* current = NULL;

//static void binary(bool canAssign);
//static void literal(bool canAssign);
//static void grouping(bool canAssign);
//static void number(bool canAssign);
//static void string(bool canAssign);
//static void unary(bool canAssign);
//static void variable(bool canAssign);

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
		rules[TOKEN_IDENTIFIER] = { variable, NULL,   PREC_NONE };
		rules[TOKEN_STRING] = { string,   NULL,   PREC_NONE };
		rules[TOKEN_NUMBER] = { number,   NULL,   PREC_NONE };
		rules[TOKEN_AND] = { NULL,     and_,   PREC_AND };
		rules[TOKEN_CLASS] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_ELSE] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_FALSE] = { literal,  NULL,   PREC_NONE };
		rules[TOKEN_FOR] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_FUN] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_IF] = { NULL,     NULL,   PREC_NONE };
		rules[TOKEN_NIL] = { literal,  NULL,   PREC_NONE };
		rules[TOKEN_OR] = { NULL,     or_,    PREC_OR };
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

// 当我们第一次启动虚拟机时，我们会调用它使所有东西进入一个干净的状态。
static void initCompiler(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
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

static void binary(bool canAssign) {
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

static void literal(bool canAssign) {
	// 因为parsePrecedence()已经消耗了关键字标识，我们需要做的就是输出正确的指令。我们根据解析出的标识的类型来确定指令。
	switch (parser.previous.type) {
	case TOKEN_FALSE: emitByte(OP_FALSE); break;
	case TOKEN_NIL: emitByte(OP_NIL); break;
	case TOKEN_TRUE: emitByte(OP_TRUE); break;
	default: return; // Unreachable.
	}
}

static void grouping(bool canAssign) {
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
 
static bool identifiersEqual(Token* a, Token* b) {
	// 既然我们知道两个词素的长度，那我们首先检查它。如果长度相同，我们就使用memcmp()检查字符。
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

// 这会初始化编译器变量数组中下一个可用的Local。它存储了变量的名称和持有变量的作用域的深度。
static void addLocal(Token name) {
	// 使用局部变量的指令通过槽的索引来引用变量。该索引存储在一个单字节操作数中，这意味着虚拟机一次最多只能支持256个局部变量。
	// 如果我们试图超过这个范围，不仅不能在运行时引用变量，而且编译器也会覆盖自己的局部变量数组。
	if (current->localCount == UINT8_COUNT) {
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	// 一旦变量声明开始——换句话说，在它的初始化式之前——名称就会在当前作用域中声明。变量存在，但处于特殊的“未初始化”状态。
	// 然后我们编译初始化式。如果在表达式中的任何一个时间点，我们解析了一个指向该变量的标识符，我们会发现它还没有初始化，并报告错误。
	// 在我们完成初始化表达式的编译之后，把变量标记为已初始化并可供使用。
	// 为了实现这一点，当声明一个局部变量时，我们需要以某种方式表明“未初始化”状态。相对地，我们将变量的作用域深度设置为一个特殊的哨兵值-1。
	local->depth = -1;
}

// 在这里，编译器记录变量的存在。我们只对局部变量这样做，所以如果在顶层全局作用域中，就直接退出。
// 因为全局变量是后期绑定的，所以编译器不会跟踪它所看到的关于全局变量的声明。
// 但是对于局部变量，编译器确实需要记住变量的存在。这就是声明的作用——将变量添加到编译器在当前作用域内的变量列表中。
static void declareVariable() {
	if (current->scopeDepth == 0) return;

	Token* name = &parser.previous;

	// 局部变量在声明时被追加到数组中，这意味着当前作用域始终位于数组的末端。当我们声明一个新的变量时，我们从末尾开始，反向查找具有相同名称的已有变量。
	// 如果是当前作用域中找到，我们就报告错误。
	// 此外，如果我们已经到达了数组开头或另一个作用域中的变量，我们就知道已经检查了当前作用域中的所有现有变量。
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error("Already a variable with this name in this scope.");
		}
	}

	addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);

	// 首先，我们“声明”这个变量。之后，如果我们在局部作用域中，则退出函数。
	// 在运行时，不会通过名称查询局部变量。不需要将变量的名称放入常量表中，所以如果声明在局部作用域内，则返回一个假的表索引。
	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

// 所这就是编译器中“声明”和“定义”变量的真正含义。“声明”是指变量被添加到作用域中，而“定义”是变量可以被使用的时候。
static void markInitialized() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// 它会输出字节码指令，用于定义新变量并存储其初始化值。变量名在常量表中的索引是该指令的操作数。
// 在基于堆栈的虚拟机中，我们通常是最后发出这条指令。
// 在运行时，我们首先执行变量初始化器的代码，将值留在栈中。然后这条指令会获取该值并保存起来，以供日后使用。
static void defineVariable(uint8_t global) {
	// 如果处于局部作用域内，就需要生成一个字节码来存储局部变量。
	// 没有代码会在运行时创建局部变量。想想虚拟机现在处于什么状态。
	// 它已经执行了变量初始化表达式的代码（如果用户省略了初始化，则是隐式的nil），并且该值作为唯一保留的临时变量位于栈顶。
	// 我们还知道，新的局部变量会被分配到栈顶……这个值已经在那里了。因此，没有什么可做的。临时变量直接成为局部变量。没有比这更有效的方法了。
	if (current->scopeDepth > 0) {
		// 稍后，一旦变量的初始化式编译完成，我们将其标记为已初始化。
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

// 变量声明的解析从varDeclaration()开始，并依赖于其它几个函数。
// 首先，parseVariable()会使用标识符标识作为变量名称，将其词素作为字符串添加到字节码块的常量表中，然后返回它的常量表索引。
// 接着，在varDeclaration()编译完初始化表达式后，会调用defineVariable()生成字节码，将变量的值存储到全局变量哈希表中。
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

//Statement----------------------------------------------------------------

static void printStatement() {
	// print语句会对表达式求值并打印出结果，所以我们首先解析并编译这个表达式。
	expression();
	// 语法要求在表达式之后有一个分号，所以我们消耗一个分号标识。
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	// 最后，我们生成一条新指令来打印结果。
	emitByte(OP_PRINT);
}

// 第一个程序会生成一个字节码指令，并为跳转偏移量写入一个占位符操作数。
// 我们把操作码作为参数传入，因为稍后我们会有两个不同的指令都使用这个辅助函数。
// 我们使用两个字节作为跳转偏移量的操作数。一个16位的偏移量可以让我们跳转65535个字节的代码，这对于我们的需求来说应该足够了。
static int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitByte(0xff);
	emitByte(0xff);
	return currentChunk()->count - 2;
}

// 该函数会返回生成的指令在字节码块中的偏移量。编译完then分支后，我们将这个偏移量传递给这个函数。
// 这个函数会返回到字节码中，并将给定位置的操作数替换为计算出的跳转偏移量。
// 我们在生成下一条希望跳转的指令之前调用patchJump()，因此会使用当前字节码计数来确定要跳转的距离。
static void patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself.
	int jump = currentChunk()->count - offset - 2;

	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}

	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

static void ifStatement() {
	// 首先我们编译条件表达式（用小括号括起来）。在运行时，这会将条件值留在栈顶。我们将通过它来决定是执行then分支还是跳过它。
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	// 然后我们生成一个新的OP_JUMP_IF_ELSE指令。
	// 这条指令有一个操作数，用来表示ip的偏移量——要跳过多少字节的代码。如果条件是假，它就按这个值调整ip。
	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	// 我们可以让OP_JUMP_IF_FALSE指令自身弹出条件值，但很快我们会对不希望弹出条件值的逻辑运算符使用相同的指令。
	// 相对地，我们在编译if语句时，会让编译器生成几条显式的OP_POP指令，我们需要注意生成的代码中的每一条执行路径都要弹出条件值。
	emitByte(OP_POP);
	statement();

	int elseJump = emitJump(OP_JUMP);

	// 但我们有个问题。当我们写OP_JUMP_IF_FALSE指令的操作数时，我们怎么知道要跳多远？
	// 为了解决这个问题，我们使用了一个经典的技巧，叫作回填（backpatching）。
	// 我们首先生成跳转指令，并附上一个占位的偏移量操作数，我们跟踪这个半成品指令的位置。
	// 接下来，我们编译then主体。一旦完成，我们就知道要跳多远。所以我们回去将占位符替换为真正的偏移量，现在我们可以计算它了。
	patchJump(thenJump);
	emitByte(OP_POP);

	// 当条件为假时，我们会跳过then分支。如果存在else分支，ip就会出现在其字节码的开头处。
	if (match(TOKEN_ELSE)) statement();

	// 当条件为真时，执行完then分支后，我们需要跳过else分支。
	// 在执行完then分支后，会跳转到else分支之后的下一条语句。与其它跳转不同，这个跳转是无条件的。我们一定会接受该跳转，所以我们需要另一条指令来表达它。
	patchJump(elseJump);
}

// 执行代码块只是意味着一个接一个地执行其中包含的语句，所以不需要编译它们。
// 从语义上讲，块所做的事就是创建作用域。在我们编译块的主体之前，我们会调用这个函数进入一个新的局部作用域。
static void beginScope() {
	current->scopeDepth++;
}

static void block() {
	// 它会一直解析声明和语句，直到遇见右括号。就像我们在解析器中的所有循环一样，我们也要检查标识流是否结束。
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void endScope() {
	// 当一个代码块结束时，我们需要让其中的变量安息。
	// 当我们弹出一个作用域时，后向遍历局部变量数组，查找在刚刚离开的作用域深度上声明的所有变量。我们通过简单地递减数组长度来丢弃它们。
	// 这里也有一个运行时的因素。局部变量占用了堆栈中的槽位。当局部变量退出作用域时，这个槽就不再需要了，应该被释放。
	// 因此，对于我们丢弃的每一个变量，我们也要生成一条OP_POP指令，将其从栈中弹出。
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP);
		current->localCount--;
	}
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
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	}
	else {
		expressionStatement();
	}
}

//Statement---------------------------------------------------------------- 

// 为了编译数值字面量，我们在数组的TOKEN_NUMBER索引处存储一个指向下面函数的指针。
static void number(bool canAssign) {
	// 我们假定数值字面量标识已经被消耗了，并被存储在previous中。我们获取该词素，并使用C标准库将其转换为一个double值。
	double value = strtod(parser.previous.start, NULL);
	// 然后我们用下面的函数生成加载该double值的字节码。
	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	// 这里直接从词素中获取字符串的字符。+1和-2部分去除了开头和结尾的引号。然后，它创建了一个字符串对象，将其包装为一个Value，并塞入常量表中。
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static int resolveLocal(Compiler* compiler, Token* name) {
	// 我们会遍历当前在作用域内的局部变量列表。如果有一个名称与标识符相同，则标识符一定指向该变量。我们已经找到了它！
	// 我们后向遍历数组，这样就能找到最后一个带有该标识符的已声明变量。这可以确保内部的局部变量能正确地遮蔽外围作用域中的同名变量。
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			// 当解析指向局部变量的引用时，我们会检查作用域深度，看它是否被完全定义。
			// 如果变量的深度是哨兵值，那这一定是在变量自身的初始化式中对该变量的引用，我们会将其报告为一个错误。
			if (local->depth == -1) {
				error("Can't read local variable in its own initializer.");
			}
			// 在运行时，我们使用栈中槽索引来加载和存储局部变量，因此编译器在解析变量之后需要计算索引。
			// 每当一个变量被声明，我们就将它追加到编译器的局部变量数组中。这意味着第一个局部变量在索引0的位置，下一个在索引1的位置，以此类推。
			// 换句话说，编译器中的局部变量数组的布局与虚拟机堆栈在运行时的布局完全相同。变量在局部变量数组中的索引与其在栈中的槽位相同。
			return i;
		}
	}

	// 如果我们在整个数组中都没有找到具有指定名称的变量，那它肯定不是局部变量。在这种情况下，我们返回-1，表示没有找到，应该假定它是一个全局变量。
	return -1;
}

// 这里会调用与之前相同的identifierConstant()函数，以获取给定的标识符标识，并将其词素作为字符串添加到字节码块的常量表中。
// 剩下的工作就是生成一条指令，加载具有该名称的全局变量。
static void namedVariable(Token name, bool canAssign) {
	// 我们不对变量访问和赋值对应的字节码指令进行硬编码，而是使用了一些C变量。
	// 首先，我们尝试查找具有给定名称的局部变量，如果我们找到了，就使用处理局部变量的指令。
	// 否则，我们就假定它是一个全局变量，并使用现有的处理全局变量的字节码。
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	// 在标识符表达式的解析函数中，我们会查找标识符后面的等号。
	// 如果找到了，我们就不会生成变量访问的代码，我们会编译所赋的值，然后生成一个赋值指令。
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(setOp, (uint8_t)arg);
	}
	else {
		emitBytes(getOp, (uint8_t)arg);
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
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

// 在这个方法被调用时，左侧的表达式已经被编译了。这意味着，在运行时，它的值将会在栈顶。
// 如果这个值为假，我们就知道整个and表达式的结果一定是假，所以我们跳过右边的操作数，将左边的值作为整个表达式的结果。
// 否则，我们就丢弃左值，计算右操作数，并将它作为整个and表达式的结果。
static void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);

	emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	patchJump(endJump);
}

// 在or表达式中，如果左侧值为真，那么我们就跳过右侧的操作数。因此，当值为真时，我们需要跳过。
// 我们可以添加一条单独的指令，但为了说明编译器如何自由地将语言的语义映射为它想要的任何指令序列，我会使用已有的跳转指令来实现它。
// 说实话，这并不是最好的方法。（这种方式中）需要调度的指令更多，开销也更大。
static void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);

	parsePrecedence(PREC_OR);
	patchJump(endJump);
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
	// 我们搞砸了优先级处理，因为variable()没有考虑包含变量的外围表达式的优先级。
	// 如果变量恰好是中缀操作符的右操作数，或者是一元操作符的操作数，那么这个包含表达式的优先级太高，不允许使用=。
	// 为了解决这个问题，variable()应该只在低优先级表达式的上下文中寻找并使用=。
	bool canAssign = precedence <= PREC_ASSIGNMENT;
	// 我们正向一个解析函数传递参数。但是这些函数是存储在一个函数指令表格中的，所以所有的解析函数需要具有相同的类型。
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	// 如果=没有作为表达式的一部分被消耗，那么其它任何东西都不会消耗它。这是一个错误，我们应该报告它。
	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

// 它只是简单地返回指定索引处的规则。
static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

// 我们将字节码块传入，而编译器会向其中写入代码，如何compile()返回编译是否成功。
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);

	Compiler compiler;
	initCompiler(&compiler);

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