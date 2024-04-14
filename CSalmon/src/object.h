#ifndef csalmon_object_h
#define csalmon_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

// 因为我们会经常访问这些标记类型，所以有必要编写一个宏，从给定的Value中提取对象类型标签。
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
// 给定一个Obj*，你可以将其“向下转换”为一个/ObjString*。当然，你需要确保你的Obj*指针确实指向一个实际的ObjString中的obj字段。
// 否则，你就会不安全地重新解释内存中的随机比特位。为了检测这种类型转换是否安全，我们再添加另一个宏。
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_FUNCTION,
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
	// 今天我们至少应该做到最基本的一点：确保虚拟机可以找到每一个分配的对象，即使Lox程序本身不再引用它们，从而避免泄露内存。
	// 我们会创建一个链表存储每个Obj。虚拟机可以遍历这个列表，找到在堆上分配的每一个对象，无论用户的程序或虚拟机的堆栈是否仍然有对它的引用。
	// 我们可以定义一个单独的链表节点结构体，但那样我们也必须分配这些节点。相反，我们会使用侵入式列表——Obj结构体本身将作为链表节点。
	struct Obj* next;
};

// 函数是Lox中的一等公民，所以它们需要作为实际的Lox对象。因此，ObjFunction具有所有对象类型共享的Obj头。
// arity字段存储了函数所需要的参数数量。然后，除了字节码块，我们还需要存储函数名称。这有助于报告可读的运行时错误。
typedef struct {
	Obj obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

// 字符串对象中包含一个字符数组。
// 这些字符存储在一个单独的、由堆分配的数组中，这样我们就可以按需为每个字符串留出空间。我们还会保存数组中的字节数。
struct ObjString {
	// 因为ObjString是一个Obj，它也需要所有Obj共有的状态。它通过将第一个字段置为Obj来实现这一点。
	// C语言规定，结构体的字段在内存中是按照它们的声明顺序排列的。此外，当结构体嵌套时，内部结构体的字段会在适当的位置展开。
	// 注意ObjString的第一个字节是如何与Obj精确对齐的。这并非巧合——是C语言强制要求的。
	// 这是为实现一个巧妙的模式而设计的：你可以接受一个指向结构体的指针，并安全地将其转换为指向其第一个字段的指针，反之亦可。
	Obj obj;
	int length;
	char* chars;
	// 遍历整个字符串来计算哈希值是有点慢的。如果我们每次在哈希表中查找键时都要遍历字符串，就会失去哈希表的一些性能优势。
	// 所以我们要做一件显而易见的事：缓存它。
	// 每个ObjString 会存储其字符串的哈希码。由于字符串在Lox中是不可变的，所以我们可以预先计算一次哈希代码，并确保它永远不会失效。
	// 提前缓存它是有道理的：分配字符串并复制其字符已然是一个O(n)的操作了，所以这是一个很好的时机来执行字符串哈希的O(n)计算。
	uint32_t hash;
};

ObjFunction* newFunction();

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

// 因为函数体使用了两次value。宏的展开方式是在主体中形参名称出现的每个地方插入实参表达式。
// 如果一个宏中使用某个参数超过一次，则该表达式就会被求值多次。
static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif