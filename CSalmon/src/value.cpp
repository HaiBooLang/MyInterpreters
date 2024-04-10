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
    case VAL_OBJ: {
        ObjString* aString = AS_STRING(a);
        ObjString* bString = AS_STRING(b);
        return aString->length == bString->length && memcmp(aString->chars, bString->chars, aString->length) == 0;
    }
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