#include <stdio.h>
#include "common.h"
#include "debug.h"
#include "vm.h"

// ����������һ��ȫ��VM���󡣷�������ֻ��Ҫһ��������������������ñ����еĴ�����ҳ���ϸ���㡣
VM vm;

void initVM() {
}

void freeVM() {
}

static InterpretResult run() {
	// Ϊ��ʹ���������ȷ���궨�屾��Ҫ�������ڸú����С������ڿ�ʼʱ���������ǣ�Ȼ����Ϊ���ǱȽϹ��ģ��ڽ���ʱȡ�����ǵĶ��塣
	// READ_BYTE�������ȡip��ǰָ���ֽڣ�Ȼ���ƽ�ָ��ָ�롣
#define READ_BYTE() (*vm.ip++)
	// READ_CONTANT()���ֽ����ж�ȡ��һ���ֽڣ����õ���������Ϊ���������ڴ����ĳ������в�����Ӧ��Value��
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

	// ������һ�����Ͻ��е����ѭ����ÿ��ѭ���У����ǻ��ȡ��ִ��һ���ֽ���ָ�
	for (;;) {
		// �����������־֮���������ִ��ÿ��ָ��֮ǰ���ᷴ��ಢ�����ӡ������
		// ���� disassembleInstruction() ��������һ������offset��Ϊ�ֽ�ƫ�����������ǽ���ǰָ�����ô洢Ϊһ��ֱ��ָ�룬
		// ������������Ҫ��һ��СС��ָ�����㣬��ipת���ɴ��ֽ��뿪ʼ�����ƫ������
#ifdef DEBUG_TRACE_EXECUTION
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction = 0;
		// Ϊ�˴���һ��ָ�����������ҪŪ���Ҫ�����������ָ�READ_BYTE�������ȡip��ǰָ���ֽڣ�Ȼ���ƽ�ָ��ָ�롣
		// �κ�ָ��ĵ�һ���ֽڶ��ǲ����롣����һ�������룬������Ҫ�ҵ�ʵ�ָ�ָ���������ȷ��C���롣������̱���Ϊ�����ָ����ɡ�
		switch (instruction = READ_BYTE()) {
		case OP_CONSTANT: {
			Value constant = READ_CONSTANT();
			printValue(constant);
			printf("\n");
			break;
		}
		case OP_RETURN: {
			return INTERPRET_OK;
		}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk) {
	vm.chunk = chunk;
	// ����ͨ����ipָ����еĵ�һ���ֽ����������ʼ����
	// �������ִ�е����������ж�����ˣ�IP����ָ����һ��ָ������ǵ�ǰ���ڴ����ָ�
	vm.ip = vm.chunk->code;
	return run();
}