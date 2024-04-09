// 结构上讲，字节码类似于机器码。它是一个密集的、线性的二进制指令序列。这样可以保持较低的开销，并可以与高速缓存配合得很好。
// 然而，它是一个更简单、更高级的指令集，比任何真正的芯片都要简单。（在很多字节码格式中，每条指令只有一个字节长，因此称为“字节码”）
// 我们提供编写模拟器来解决这个问题，这个模拟器是一个用软件编写的芯片，每次会解释字节码的一条指令。如果你愿意的话，可以叫它虚拟机（VM）。
// 模拟层增加了开销，这是字节码比本地代码慢的一个关键原因。但作为回报，它为我们提供了可移植性。‘#include "common.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

// 一个高质量的REPL可以优雅地处理多行的输入，并且没有硬编码的行长度限制。这里的REPL有点……简朴，但足以满足我们的需求。
static void repl() {
	char line[1024];
	for (;;) {
		printf("> ");

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}

		// 真正的工作发生在interpret()中。
		interpret(line);
	}
}

// 困难的地方在于，我们想分配一个足以读取整个文件的字符串，但是我们在读取文件之前并不知道它有多大。
static char* readFile(const char* path) {
	// 我们打开文件，但是在读之前，先通过fseek()寻找到文件的最末端。
	FILE* file = fopen(path, "rb");
	// 如果我们不能正确地读取用户的脚本，我们真正能做的就是告诉用户并优雅地退出解释器。如果文件不存在或用户没有访问权限，就会发生这种情况。
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}
	fseek(file, 0L, SEEK_END);

	// 接下来我们调用ftell()，它会告诉我们里文件起始点有多少字节。既然我们定位到了最末端，那它就是文件大小。
	size_t fileSize = ftell(file);

	// 我们退回到起始位置，分配一个相同大小的字符串，然后一次性读取整个文件。
	rewind(file);
	char* buffer = (char*)malloc(fileSize + 1);
	// 如果我们甚至不能分配足够的内存来读取Lox脚本，那么用户可能会有更大的问题需要担心，但我们至少应该尽最大努力让他们知道。
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	// 最后，读取本身可能会失败。这也是不大可能发生的。
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}
	buffer[bytesRead] = '\0';

	fclose(file);
	return buffer;
}

// 我们读取文件并执行生成的Lox源码字符串。然后，根据其结果，我们适当地设置退出码，因为我们是严谨的工具制作者，并且关心这样的小细节。
// 我们还需要释放源代码字符串，因为readFile()会动态地分配内存，并将所有权传递给它的调用者。
static void runFile(const char* path) {
	char* source = readFile(path);
	InterpretResult result = interpret(source);
	free(source);

	if (result == INTERPRET_COMPILE_ERROR) exit(65);
	if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}


int main(int argc, const char* argv[]) {
	initVM();

	// 如果你没有向可执行文件传递任何参数，就会进入REPL。
	if (argc == 1) {
		repl();
	}
	// 如果传入一个参数，就将其当做要运行的脚本的路径。
	else if (argc == 2) {
		runFile(argv[1]);
	}
	else {
		fprintf(stderr, "Usage: clox [path]\n");
		exit(64);
	}

	freeVM();
	return 0;
}