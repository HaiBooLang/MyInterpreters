package salmon;

import java.util.HashMap;
import java.util.Map;

// 变量与值之间的绑定关系需要保存在某个地方，这种数据结构就被称为环境。
// 你可以把它想象成一个映射，其中键是变量名称，值就是变量的值。
// 使用原生字符串可以保证具有相同名称的标识符标记都应该指向相同的变量（暂时忽略作用域）。
public class Environment {
    private final Map<String, Object> values = new HashMap<>();

    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        // 因为将其当作静态错误会使递归声明过于困难，因此我们把这个错误推迟到运行时。
        // 在一个变量被定义之前引用它是可以的，只要你不对引用进行求值。
        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }

    // 变量定义操作可以将一个新的名称与一个值进行绑定。
    void define(String name, Object value) {
        values.put(name, value);
    }

    // 赋值与定义的主要区别在于，赋值操作不允许创建新变量。如果环境的变量映射中不存在变量的键，那就是一个运行时错误。
    void assign(Token name, Object value) {
        if (values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }

        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }
}
