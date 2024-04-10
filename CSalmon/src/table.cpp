#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// 这就是我们管理表负载因子的方式。我们不会在容量全满的时候才进行扩展。相反，当数组达到75%满时，我们会提前扩展数组。
#define TABLE_MAX_LOAD 0.75

// 就像动态值数组类型一样，哈希表最初以容量0和NULL数组开始。等到需要的时候我们才会分配一些东西。
void initTable(Table* table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

//
void freeTable(Table* table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

// 它负责接受一个键和一个桶数组，并计算出该条目属于哪个桶。
// 这个函数也是线性探测和冲突处理发挥作用的地方。我们在查询哈希表中的现有条目以及决定在哪里插入新条目时，都会使用findEntry()方法。
// 我们会从循环中直接返回，得到一个指向找到的Entry的指针，这样调用方就可以向其中插入内容或从中读取内容。
// 回到tableSet()——最先调用它的函数，我们将新条目存储到返回的桶中，然后就完成了。
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	// 首先，我们通过取余操作将键的哈希码映射为数值边界内的一个索引值。这就给了我们一个桶索引，理想情况下，我们可以在这里找到或放置条目。
	uint32_t index = key->hash % capacity;
	for (;;) {
		Entry* entry = &entries[index];
		Entry* tombstone = NULL;
		// 如果数组索引处的Entry的键为NULL，则表示桶为空。
		// 如果我们使用findEntry()在哈希表中查找东西，这意味着它不存在。如果我们用来插入，这表明我们找到了一个可以插入新条目的地方。
		// 如果桶中的键等于我们要找的，那么这个键已经存在于表中了。
		// 如果我们在做查找，这很好——我们已经找到了要查找的键。如果我们在做插入，这意味着我们要替换该键的值，而不是添加一个新条目。
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				// 如果我们遇到一个真正的空条目，那么这个键就不存在。在这种情况下，若我们经过了一个墓碑，就返回它的桶，而不是返回后面的空桶。
				// 如果我们为了插入一个节点而调用findEntry()，这时我们可以把墓碑桶视为空桶，并重用它来存储新条目。
				// 像这样自动重用墓碑槽，有助于减少墓碑在桶数组中浪费的空间。在插入与删除混合使用的典型用例中，墓碑的数量会增长一段时间，然后趋于稳定。
				// 即便如此，也不能保证大量的删除操作不会导致数组中满是墓碑。在最坏的情况下，我们最终可能没有空桶。这是很糟糕的。
				// 因为，请记住，唯一能阻止findEntry()中无限循环的原因是假设我们最终会命中一个空桶。
				// 所以我们需要仔细考虑墓碑如何与表的负载因子和大小调整进行互动。
				return tombstone != NULL ? tombstone : entry;
			}
			else {
				// 当我们在查询中遵循探测序列时，如果遇到了墓碑，我们会记录它并继续前进。
				// 第一次通过一个墓碑条目时，我们将它存储在这个局部变量中。
				if (tombstone == NULL) tombstone = entry;
			}
		} else if (entry->key == key) {
			return entry;
		}

		// 否则，就是桶中有一个条目，但具有不同的键。这就是一个冲突。
		// 在这种情况下，我们要开始探测。这也就是for循环所做的。我们从条目理想的存放位置开始。如果这个桶是空的或者有相同的键，我们就完成了。
		// 否则，我们就前进到下一个元素——这就是“线性探测”的线性部分——并进行检查。
		// 如果我们超过了数组的末端，第二个模运算符就会把我们重新带回起点。
		// 当我们找到空桶或者与我们要找的桶具有相同键的桶时，我们就退出循环。
		// 你可能会考虑无限循环的问题。如果我们与所有的桶都冲突怎么办？幸运的是，因为负载因子的原因，这种情况不会发生。
		// 因为一旦数组接近满。我们就会扩展数组，所以我们知道总是会有空桶。
		index = (index + 1) % capacity;
	}
}

bool tableGet(Table* table, ObjString* key, Value* value) {
	// 如果表完全是空的，我们肯定找不到这个条目，所以我们先检查一下。这不仅仅是一种优化——它还确保当数组为NULL时，我们不会试图访问桶数组。
	if (table->count == 0) return false;

	// 其它情况下，我们就让findEntry()发挥它的魔力。这将返回一个指向桶的指针。
	// 如果桶是空的（我们通过查看键是否为NULL来检测），那么我们就没有找到包含对应键的Entry。
	// 如果findEntry()确实返回了一个非空的Entry，那么它就是我们的匹配项。我们获取Entry的值并将其复制到输出参数中，这样调用方就可以得到该值。
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	*value = entry->value;
	return true;
}

// 在我们将条目放入哈希表之前，我们确实需要一个地方来实际存储它们。我们需要分配一个桶数组。
static void adjustCapacity(Table* table, int capacity) {
	// 我们创建一个包含capacity个条目的桶数组。
	Entry* entries = ALLOCATE(Entry, capacity);
	// 分配完数组后，我们将每个元素初始化为空桶，然后将数组（及其容量）存储到哈希表的主结构体中。
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	// 当我们调整数组的大小时，我们会分配一个新数组，并重新插入所有的现存条目。
	// 在这个过程中，我们不会把墓碑复制过来。因为无论如何我们都要重新构建探测序列，它们不会增加任何价值，而且只会减慢查找速度。
	// 这意味着我们需要重算计数，因为它可能会在调整大小的期间发生变化。
	table->count = 0;

	// 请记住，为了给每个条目选择存储桶，我们要用其哈希键与数组大小取模。这意味着，当数组大小发生变化时，条目可能会出现在不同的桶中。
	// 这些新的桶可能会出现新的冲突，我们需要处理这些冲突。
	// 因此，获取每个条目所属位置的最简单的方法是从头重新构建哈希表，将每个条目都重新插入到新的空数组中。
	// 我们从前到后遍历旧数组。
	// 只要发现一个非空的桶，我们就把这个条目插入到新的数组中。我们使用findEntry()，传入的是新数组，而不是当前存储在哈希表中的旧数组。
	for (int i = 0; i < table->capacity; i++) {
		Entry* entry = &table->entries[i];
		if (entry->key == NULL) continue;

		Entry* dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		// 这意味着，当我们增加容量时，最终在更大的数组中的条目可能会更少，因为所有的墓碑都被丢弃了。这有点浪费，但不是一个严重的实际问题。
		table->count++;
	}

	// 完成之后，我们就可以释放旧数组的内存。
	FREE_ARRAY(Entry, table->entries, table->capacity);

	table->entries = entries;
	table->capacity = capacity;
}

// 这个函数将给定的键/值对添加到给定的哈希表中。如果该键的条目已存在，新值将覆盖旧值。如果添加了新条目，则该函数返回true。
bool tableSet(Table* table, ObjString* key, Value value) {
	// 在我们向其中插入数据之前，需要确保已经有一个数组，而且足够大。
	// 如果没有足够的容量插入条目，我们就重新分配和扩展数组。
	// GROW_CAPACITY()宏会接受现有容量，并将其增长一倍，以确保在一系列插入操作中得到摊销的常数性能。
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	// 该函数的作用是接受一个键，并找到它应该放在数组中的哪个桶里。它会返回一个指向该桶的指针——数组中Entry的地址。
	Entry* entry = findEntry(table->entries, table->capacity, key);
	// 一旦有了桶，插入就很简单了。我们更新哈希表的大小，如果我们覆盖了一个已经存在的键的值，注意不要增加计数。
	bool isNewKey = entry->key == NULL;
	// 如果我们用新条目替换墓碑，因为这个桶已经被统计过了，所以计数不会改变。
	if (isNewKey && IS_NIL(entry->value)) table->count++;

	// 然后，我们将键和值复制到Entry中的对应字段中。
	entry->key = key;
	entry->value = value;
	return isNewKey;
}

// 如果我们通过简单地清除Entry来删除“biscuit”，那么我们就会中断探测序列，让后面的条目变得孤立、不可访问。
// 这有点像是从链表中删除了一个节点，而没有把指针从上一个节点重新链接到下一个节点。
// 为了解决这个问题，大多数实现都使用了一个叫作墓碑的技巧。我们不会在删除时清除条目，而是将其替换为一个特殊的哨兵条目，称为“墓碑”。
// 当我们在查找过程中顺着探测序列遍历时，如果遇到墓碑，我们不会把它当作是空槽而停止遍历。
// 相反，我们会继续前进，这样删除一个条目不会破坏任何隐式冲突链，我们仍然可以找到它之后的条目。
bool tableDelete(Table* table, ObjString* key) {
	if (table->count == 0) return false;

	// 首先，我们找到包含待删除条目的桶（如果我们没有找到，就没有什么可删除的，所以我们退出）。
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// 我们将该条目替换为墓碑。在clox中，我们使用NULL键和true值来表示，但任何不会与空桶或有效条目相混淆的表示形式都是可行的。
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

void tableAddAll(Table* from, Table* to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry* entry = &from->entries[i];
		if (entry->key != NULL) {
			tableSet(to, entry->key, entry->value);
		}
	}
}