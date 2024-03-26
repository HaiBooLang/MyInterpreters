package salmon;

import java.util.List;

interface SalmonCallable {
    // 查询函数的元数。
    int arity();

    // 我们会传入解释器，以防实现call()方法的类会需要它。
    // 我们也会提供已求值的参数值列表。接口实现者的任务就是返回调用表达式产生的值。
    Object call(Interpreter interpreter, List<Object> arguments);
}
