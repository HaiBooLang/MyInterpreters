package salmon;

import java.util.HashMap;
import java.util.Map;

// 变量与值之间的绑定关系需要保存在某个地方，这种数据结构就被称为环境。
// 你可以把它想象成一个映射，其中键是变量名称，值就是变量的值。
// 使用原生字符串可以保证具有相同名称的标识符标记都应该指向相同的变量（暂时忽略作用域）。
public class Environment {
    // 强化Environment类对嵌套的支持，我们在每个环境中添加一个对其外围环境的引用。
    final Environment enclosing;
    private final Map<String, Object> values = new HashMap<>();

    // 无参构造函数用于全局作用域环境，它是环境链的结束点。
    Environment() {
        enclosing = null;
    }

    // 另一个构造函数用来创建一个嵌套在给定外部作用域内的新的局部作用域。
    Environment(Environment enclosing) {
        this.enclosing = enclosing;
    }

    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        // 如果当前环境中没有找到变量，就在外围环境中尝试。然后递归地重复该操作，最终会遍历完整个链路。
        if (enclosing != null) return enclosing.get(name);

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

        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }

    Object getAt(int distance, String name) {
        return ancestor(distance).values.get(name);
    }

    void assignAt(int distance, Token name, Object value) {
        ancestor(distance).values.put(name.lexeme, value);
    }

    // 现在我们明确知道链路中的哪个环境中会包含该变量。我们使用下面的辅助方法直达这个环境。
    // 该方法在环境链中经过确定的跳数之后，返回对应的环境。一旦我们有了环境，getAt()方法就可以直接返回对应环境map中的变量值。
    // 甚至不需要检查变量是否存在——我们知道它是存在的，因为解析器之前已经确认过了。
    Environment ancestor(int distance) {
        Environment environment = this;
        for (int i = 0; i < distance; i++) {
            environment = environment.enclosing;
        }

        return environment;
    }
}
