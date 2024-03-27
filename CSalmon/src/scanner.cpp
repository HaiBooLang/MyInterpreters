#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

// 当我们的扫描器一点点处理用户的源代码时，它会跟踪自己已经走了多远。
// 就像我们在虚拟机中所做的那样，我们将状态封装在一个结构体中，然后创建一个该类型的顶层模块变量，这样就不必在所有的函数之间传递它。
// 我们甚至没有保留指向源代码字符串起点的指针。扫描器只处理一遍代码，然后就结束了。
typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
	// 我们从第一行的第一个字符开始，就像一个运动员蹲在起跑线上。
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}