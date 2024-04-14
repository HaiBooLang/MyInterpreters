#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    // 当newSize为0时，我们通过调用free()来自己处理回收的情况。
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    // 其它情况下，我们依赖于C标准库的realloc()函数。
    // 该函数可以方便地支持我们策略中的其它三个场景。当oldSize为0时，realloc() 等同于调用malloc()。
    // 有趣的情况是当oldSize和newSize都不为0时。它们会告诉realloc()要调整之前分配的块的大小。
    // 如果新的大小小于现有的内存块，它就只是更新块的大小，并返回传入的指针。如果新块大小更大，它就会尝试增长现有的内存块。
    // 如果没有空间支持块的增长，realloc()会分配一个所需大小的新的内存块，复制旧的字节，释放旧内存块，然后返回一个指向新内存块的指针。
    void* result = realloc(pointer, newSize);

    // 如果我们的VM不能得到它所需要的内存，那就做不了什么有用的事情.
    // 但我们至少可以检测这一点，并立即中止进程，而不是返回一个NULL指针，然后让程序运行偏离轨道。
    if (result == NULL) exit(1);

    return result;
}

// 当我们使用完一个函数对象后，必须将它借用的比特位返还给操作系统。
static void freeObject(Obj* object) {
    switch (object->type) {
    case OBJ_FUNCTION: {
        // 这个switch语句负责释放ObjFunction本身以及它所占用的其它内存。函数拥有自己的字节码块，所以我们调用Chunk中类似析构器的函数。
        ObjFunction* function = (ObjFunction*)object;
        freeChunk(&function->chunk);
        FREE(ObjFunction, object);
        break;
    }
    case OBJ_STRING: {
        ObjString* string = (ObjString*)object;
        // 我们不仅释放了Obj本身。因为有些对象类型还分配了它们所拥有的其它内存，我们还需要一些特定于类型的代码来处理每种对象类型的特殊需求。
        // 在这里，这意味着我们释放字符数组，然后释放ObjString。它们都使用了最后一个内存管理宏。
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjString, object);
        break;
    }
    }
}


void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}