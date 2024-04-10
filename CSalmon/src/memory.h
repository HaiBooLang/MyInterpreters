#ifndef csalmon_memory_h
#define csalmon_memory_h

#include "common.h"

// 使用这个底层宏来分配一个具有给定元素类型和数量的数组。
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// 这个宏会根据给定的当前容量计算出新的容量。
// 为了获得我们想要的性能，重要的部分就是基于旧容量大小进行扩展。我们以2的系数增长，这是一个典型的取值。1.5是另外一个常见的选择。
// 我们还会处理当前容量为0的情况。在这种情况下，我们的容量直接跳到8，而不是从1开始。
// 这就避免了在数组非常小的时候出现额外的内存波动，代价是在非常小的块中浪费几个字节。
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// 一旦我们知道了所需的容量，就可以使用GROW_ARRAY()创建或扩充数组到该大小。
// 这个宏简化了对reallocate()函数的调用，真正的工作就是在其中完成的。
// 宏本身负责获取数组元素类型的大小，并将生成的void*转换成正确类型的指针。
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

// 与GROW_ARRAY()类似，这是对reallocate()调用的包装。这个函数通过传入0作为新的内存块大小，来释放内存。
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// 这个reallocate()函数是我们将在clox中用于所有动态内存管理的唯一函数——分配内存，释放内存以及改变现有分配的大小。
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif