#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

// 这个宏的存在主要是为了避免重复地将void*转换回期望的类型。
#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

// 我们使用好朋友ALLOCATE_OBJ()来分配内存并初始化对象的头信息，以便虚拟机知道它是什么类型的对象。
// 我们没有像对ObjString那样传入参数来初始化函数，而是将函数设置为一种空白状态——零参数、无名称、无代码。这里会在稍后创建函数后被填入数据。
ObjFunction* newFunction() {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

// 它在堆上分配了一个给定大小的对象。
// 注意，这个大小不仅仅是Obj本身的大小。调用者传入字节数，以便为被创建的对象类型留出额外的载荷字段所需的空间。
static Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, 0, size);
	object->type = type;
	// 每当我们分配一个Obj时，就将其插入到列表中。
	// 由于这是一个单链表，所以最容易插入的地方是头部。这样，我们就不需要同时存储一个指向尾部的指针并保持对其更新。
	object->next = vm.objects;
	vm.objects = object;
	return object;
}

// 它在堆上创建一个新的ObjString，然后初始化其字段。这有点像OOP语言中的构建函数。
// 因此，它首先调用“基类”的构造函数来初始化Obj状态，使用了一个新的宏。
static ObjString* allocateString(char* chars, int length, uint32_t hash) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	// 对于clox，我们会自动驻留每个字符串。这意味着，每当我们创建了一个新的唯一字符串，就将其添加到表中。
	tableSet(&vm.strings, string, NIL_VAL);
	return string;
}

// 该算法被称为“FNV-1a”，是我所知道的最短的正统哈希函数。
// 基本思想非常简单，许多哈希函数都遵循同样的模式。从一些初始哈希值开始，通常是一个带有某些精心选择的数学特性的常量。
// 然后遍历需要哈希的数据。对于每个字节（有些是每个字），以某种方式将比特与哈希值混合，然后将结果比特进行一些扰乱。
// “混合”和“扰乱”的含义可以变得相当复杂。不过，最终的基本目标是均匀——我们希望得到的哈希值尽可能广泛地分散在数组范围内，以避免碰撞和聚集。
static uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619;
	}
	return hash;
}

// 前面的copyString()函数假定它不能拥有传入的字符的所有权。
// 相对地，它保守地在堆上创建了一个ObjString可以拥有的字符的副本。对于传入的字符位于源字符串中间的字面量来说，这样做是正确的。
// 但是，对于连接，我们已经在堆上动态地分配了一个字符数组。
// 再做一个副本是多余的（而且意味着concatenate()必须记得释放它的副本）。相反，这个函数要求拥有传入字符串的所有权。
ObjString* takeString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	// 我们首先在字符串表中查找该字符串。
	// 如果找到了，在返回它之前，我们释放传入的字符串的内存。因为所有权被传递给了这个函数，我们不再需要这个重复的字符串，所以由我们释放它。
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return allocateString(chars, length, hash);
}

// 由于连接等字符串操作，一些ObjString会在运行时被动态创建。
// 这些字符串显然需要为字符动态分配内存，这也意味着该字符串不再需要这些内存时，要释放它们。
// 如果我们有一个ObjString存储字符串字面量，并且试图释放其中指向原始的源代码字符串的字符数组，糟糕的事情就会发生。
// 因此，对于字面量，我们预先将字符复制到堆中。这样一来，每个ObjString都能可靠地拥有自己的字符数组，并可以释放它。
ObjString* copyString(const char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	// 假定一个字符串是唯一的，这就会把它放入表中，但在此之前，我们需要实际检查字符串是否有重复。
	// 当把一个字符串复制到新的LoxString中时，我们首先在字符串表中查找它。
	// 如果找到了，我们就不“复制”，而是直接返回该字符串的引用。如果没有找到，我们就是落空了，则分配一个新字符串，并将其存储到字符串表中。
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) return interned;
	char* heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction* function) {
	// 既然函数知道它的名称，那就应该说出来。
	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
	case OBJ_FUNCTION:
		printFunction(AS_FUNCTION(value));
		break;
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	}
}