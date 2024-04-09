#ifndef csalmon_common_h
#define csalmon_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 当这个标志被定义后，我们使用现有的“debug”模块打印出块中的字节码。
#define DEBUG_PRINT_CODE
// 定义了这个标志之后，虚拟机在执行每条指令之前都会反汇编并将其打印出来。
#define DEBUG_TRACE_EXECUTION

#endif

