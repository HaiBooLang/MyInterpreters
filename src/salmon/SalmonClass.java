package salmon;

import java.util.List;
import java.util.Map;

public class SalmonClass implements SalmonCallable {
    final String name;
    // 实例存储状态，类存储行为。LoxInstance包含字段的map，而LoxClass包含方法的map。
    private final Map<String, SalmonFunction> methods;

    SalmonClass(String name, Map<String, SalmonFunction> methods) {
        this.name = name;
        this.methods = methods;
    }

    SalmonFunction findMethod(String name) {
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        return null;
    }

    @Override
    public String toString() {
        return name;
    }

    // 当你“调用”一个类时，它会为被调用的类实例化一个新的LoxInstance并返回。
    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        SalmonInstance instance = new SalmonInstance(this);
        return instance;
    }

    // arity() 方法是解释器用于验证你是否向callable中传入了正确数量的参数。
    @Override
    public int arity() {
        return 0;
    }
}
