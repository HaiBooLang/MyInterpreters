#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source) {
	initScanner(source);

    int line = -1;
    // 这个循环是无限的。每循环一次，它就会扫描一个词法标识并打印出来。当它遇到特殊的“文件结束”标识或错误时，就会停止。
    for (;;) {
        Token token = scanToken();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        }
        else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
}