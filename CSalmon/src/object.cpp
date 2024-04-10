#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// 这个宏的存在主要是为了避免重复地将void*转换回期望的类型。
#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

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
	return string;
}

// 前面的copyString()函数假定它不能拥有传入的字符的所有权。
// 相对地，它保守地在堆上创建了一个ObjString可以拥有的字符的副本。对于传入的字符位于源字符串中间的字面量来说，这样做是正确的。
// 但是，对于连接，我们已经在堆上动态地分配了一个字符数组。
// 再做一个副本是多余的（而且意味着concatenate()必须记得释放它的副本）。相反，这个函数要求拥有传入字符串的所有权。
ObjString* takeString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	return allocateString(chars, length, hash);
}

// 由于连接等字符串操作，一些ObjString会在运行时被动态创建。
// 这些字符串显然需要为字符动态分配内存，这也意味着该字符串不再需要这些内存时，要释放它们。
// 如果我们有一个ObjString存储字符串字面量，并且试图释放其中指向原始的源代码字符串的字符数组，糟糕的事情就会发生。
// 因此，对于字面量，我们预先将字符复制到堆中。这样一来，每个ObjString都能可靠地拥有自己的字符数组，并可以释放它。
ObjString* copyString(const char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	char* heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	}
}