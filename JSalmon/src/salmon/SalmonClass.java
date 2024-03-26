package salmon;

import java.util.List;
import java.util.Map;

public class SalmonClass implements SalmonCallable {
    final String name;
    final SalmonClass superclass;
    // 实例存储状态，类存储行为。LoxInstance包含字段的map，而LoxClass包含方法的map。
    private final Map<String, SalmonFunction> methods;

    SalmonClass(String name, SalmonClass superclass, Map<String, SalmonFunction> methods) {
        this.superclass = superclass;
        this.name = name;
        this.methods = methods;
    }

    SalmonFunction findMethod(String name) {
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        // 如果你能在超类的实例上调用某些方法，那么当给你一个子类的实例时，你也应该能调用这个方法。
        if (superclass != null) {
            return superclass.findMethod(name);
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

        SalmonFunction initializer = findMethod("init");
        if (initializer != null) {
            initializer.bind(instance).call(interpreter, arguments);
        }

        return instance;
    }

    // arity() 方法是解释器用于验证你是否向callable中传入了正确数量的参数。
    // 如果有初始化方法，该方法的元数就决定了在调用类本身的时候需要传入多少个参数。
    // 但是，为了方便起见，我们并不要求类定义初始化方法。如果你没有初始化方法，元数仍然是0。
    @Override
    public int arity() {
        SalmonFunction initializer = findMethod("init");
        if (initializer == null) return 0;
        return initializer.arity();
    }
}
