package salmon;

import java.util.HashMap;
import java.util.Map;

public class SalmonInstance {
    private SalmonClass klass;
    // map中的每个键是一个属性名称，对应的值就是该属性的值。
    // 实例存储状态，类存储行为。LoxInstance包含字段的map，而LoxClass包含方法的map。
    private final Map<String, Object> fields = new HashMap<>();

    SalmonInstance(SalmonClass klass) {
        this.klass = klass;
    }

    // 当访问一个属性时，你可能会得到一个字段（存储在实例上的状态值），或者你会得到一个实例类中定义的方法。
    Object get(Token name) {
        if (fields.containsKey(name.lexeme)) {
            return fields.get(name.lexeme);
        }

        SalmonFunction method = klass.findMethod(name.lexeme);
        if (method != null) return method.bind(this);

        // 我们需要处理的一个有趣的边缘情况是，如果这个实例中不包含给定名称的属性，会发生什么。
        // 我们可以悄悄返回一些假值，如nil，但是根据我对JavaScript等语言的经验，这种行为只是掩盖了错误，而没有做任何有用的事。
        // 相反，我们将它作为一个运行时错误。
        throw new RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
    }

    void set(Token name, Object value) {
        fields.put(name.lexeme, value);
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }
}
