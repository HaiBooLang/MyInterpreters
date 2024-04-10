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
	return object;
}

// 它在堆上创建一个新的ObjString，然后初始化其字段。这有点像OOP语言中的构建函数。
// 因此，它首先调用“基类”的构造函数来初始化Obj状态，使用了一个新的宏。
static ObjString* allocateString(char* chars, int length) {
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	return string;
}

// 由于连接等字符串操作，一些ObjString会在运行时被动态创建。
// 这些字符串显然需要为字符动态分配内存，这也意味着该字符串不再需要这些内存时，要释放它们。
// 如果我们有一个ObjString存储字符串字面量，并且试图释放其中指向原始的源代码字符串的字符数组，糟糕的事情就会发生。
// 因此，对于字面量，我们预先将字符复制到堆中。这样一来，每个ObjString都能可靠地拥有自己的字符数组，并可以释放它。
ObjString* copyString(const char* chars, int length) {
	char* heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length);
}