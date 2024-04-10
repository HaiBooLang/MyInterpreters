#ifndef csalmon_table_h
#define csalmon_table_h

#include "common.h"
#include "value.h"

// 哈希表（无论你的语言中怎么称呼它）是将一组键和一组值关联起来。每个键/值对是表中的一个条目。
// 给定一个键，可以查找它对应的值。你可以按键添加新的键/值对或删除条目。如果你为已有的键添加新值，它就会替换原先的条目。
// 哈希表之所以出现在这么多的语言中，是因为它们非常强大。
// 这种强大的能力主要来自于一个指标：给定一个键，哈希表会在常量时间内返回对应的值，不管哈希表中有多少键。
// 数组越大，映射到同一个桶的索引就越少，可能发生的冲突也就越少。哈希表实现者评估这种冲突的可能性的方式就是计算表的负载因子。
// 它的定义是条目的数量除以桶的数量。负载因子越大，发生冲突的可能性就越大。
// 就像我们前面实现的动态数组一样，我们在哈希表的数组被填满时，重新分配并扩大该数组。
// 但与常规的动态数组不同的是，我们不会等到数组填满。相反，我们选择一个理想的负载因子，当数组的负载因子超过该值时，我们就扩大数组。
// 即使负载因子很低，仍可能发生碰撞。
// 解决冲突的技术可以分为两大类。第一类是拉链法。每个桶中不再包含一个条目，而是包含条目的集合。
// 在经典的实现中，每个桶都指向一个条目的链表。要查找一个条目，你要先找到它的桶，然后遍历列表，直到找到包含匹配键的条目。
// 在最坏的情况下，每个条目都碰撞到同一个桶中，数据结构会退化成一个无序链表，查询复杂度为O(n)。
// 在实践中，通过控制负载因子和条目在桶中的分散方式，可以很容易地避免这种情况。
// 拉链法在概念上很简单——它实际上就是一个链表数组。大多数操作实现都可以直接实现，甚至是删除（正如我们将看到的，这可能会很麻烦）。
// 但它并不适合现代的CPU。它有很多指针带来的开销，并且倾向于在内存中分散的小的链表节点，这对缓存的使用不是很好。
// 另一种技术称为开放地址或（令人困惑的）封闭哈希。
// 使用这种技术时，所有的条目都直接存储在桶数组中，每个桶有一个条目。如果两个条目在同一个桶中发生冲突，我们会找一个其它的空桶来代替。
// 将所有条目存储在一个单一的、大的、连续的数组中，对于保持内存表示方式的简单和快速是非常好的。但它使得哈希表上的所有操作变得非常复杂。
// 当插入一个条目时，它的桶可能已经满了，这就会让我们去查看另一个桶。而那个桶本身可能也被占用了，等等。
// 这个查找可用存储桶的过程被称为探测，而检查存储桶的顺序是探测序列。
// 我们会选择最简单的方法来有效地完成工作。这就是良好的老式线性探测法。当查找一个条目时，我们先在它的键映射的桶中查找。
// 如果它不在里面，我们就在数组的下一个元素中查找，以此类推。如果我们到了数组终点，就绕回到起点。
// 线性探测的好处是它对缓存友好。因为你是直接按照内存顺序遍历数组，所以它可以保持CPU缓存行完整且正常。坏处是，它容易聚集。
// 如果你有很多具有相似键值的条目，那最终可能会产生许多相互紧挨的冲突、溢出的桶。
// 哈希函数接受一些更大的数据块，并将其“哈希”生成一个固定大小的整数哈希码，该值取决于原始数据的每一个比特。
// 一个好的哈希函数有三个主要目标：
// 它必须是确定性的。相同的输入必须总是哈希到相同的数字。
// 它必须是均匀的。给定一组典型的输入，它应该产生一个广泛而均匀分布的输出数字范围，尽可能少地出现簇或模式。
// 它必须是快速的。对哈希表的每个操作都需要我们首先对键进行哈希。如果哈希计算很慢，就有可能会抵消底层数组存储的速度优势。

// 这是一个简单的键/值对。因为键总是一个字符串，我们直接存储ObjString指针，而不是将其包装在Value中。
typedef struct {
	ObjString* key;
	Value value;
} Entry;

// 哈希表是一个条目数组。就像前面的动态数组一样，我们既要跟踪数组的分配大小（容量，capacity）和当前存储在其中的键/值对数量（计数，count）。
// 数量与容量的比值正是哈希表的负载因子。
typedef struct {
	int count;
	int capacity;
	Entry* entries;
} Table;

// 为了创建一个新的、空的哈希表，我们声明一个类似构造器的函数。
void initTable(Table* table);
void freeTable(Table* table);
// 传入一个表和一个键。如果它找到一个带有该键的条目，则返回true，否则返回false。如果该条目存在，输出的value参数会指向结果值。
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
// 既然如此，我们也来定义一个辅助函数，将一个哈希表的所有条目复制到另一个哈希表中。
void tableAddAll(Table* from, Table* to);

#endif