#ifndef csalmon_object_h
#define csalmon_object_h

#include "common.h"
#include "value.h"

// 因为我们会经常访问这些标记类型，所以有必要编写一个宏，从给定的Value中提取对象类型标签。
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)
// 给定一个Obj*，你可以将其“向下转换”为一个/ObjString*。当然，你需要确保你的Obj*指针确实指向一个实际的ObjString中的obj字段。
// 否则，你就会不安全地重新解释内存中的随机比特位。为了检测这种类型转换是否安全，我们再添加另一个宏。
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_STRING,
} ObjType;

struct Obj {
	ObjType type;
};

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
};

// 因为函数体使用了两次value。宏的展开方式是在主体中形参名称出现的每个地方插入实参表达式。
// 如果一个宏中使用某个参数超过一次，则该表达式就会被求值多次。
static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif