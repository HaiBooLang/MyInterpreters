#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"

// 
void initValueArray(ValueArray* array) {
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

bool valuesEqual(Value a, Value b) {
    // 首先，我们检查类型。如果两个Value的类型不同，它们肯定不相等。否则，我们就把这两个Value拆装并直接进行比较。
    if (a.type != b.type) return false;
    // 对于每一种值类型，我们都有一个单独的case分支来处理值本身的比较。
    // 因为填充以及联合体字段的大小不同，Value中会包含无用的比特位。
    // C语言不能保证这些值是什么，所以两个相同的Value在未使用的内存中可能是完全不同的。
    switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    // 在创建字符串时，我们增加了一点开销来进行驻留。但作为回报，在运行时，字符串的相等操作符要快得多。
    // 在Lox这样的动态类型语言中，这一点更为关键，因为在这种语言中，方法调用和实例属性都是在运行时根据名称查找的。
    // 如果测试字符串是否相等是很慢的，那就意味着按名称查找方法也很慢。在面向对象的语言中，如果这一点很慢，那么一切都会变得很慢。
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);

    default:         return false; // Unreachable.
    }
}

//
void printValue(Value value) {
    switch (value.type) {
    case VAL_BOOL:
        printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ: printObject(value); break;
    }
}