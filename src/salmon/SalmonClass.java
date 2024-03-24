package salmon;

import java.util.List;

public class SalmonClass implements SalmonCallable {
    final String name;

    SalmonClass(String name) {
        this.name = name;
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
