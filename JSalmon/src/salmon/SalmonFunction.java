package salmon;

import java.util.List;

class SalmonFunction implements SalmonCallable {
    private final Environment closure;
    private final Stmt.Function declaration;
    private final boolean isInitializer;

    SalmonFunction(Stmt.Function declaration, Environment closure, boolean isInitializer) {
        this.isInitializer = isInitializer;
        this.closure = closure;
        this.declaration = declaration;
    }

    // 我们基于方法的原始闭包创建了一个新的环境。就像是闭包内的闭包。当方法被调用时，它将变成方法体对应环境的父环境。
    SalmonFunction bind(SalmonInstance instance) {
        Environment environment = new Environment(closure);
        environment.define("this", instance);
        return new SalmonFunction(declaration, environment, isInitializer);
    }

    @Override
    public int arity() {
        return declaration.params.size();
    }

    @Override
    public String toString() {
        return "<fn " + declaration.name.lexeme + ">";
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        // 参数是函数的核心，尤其是考虑到函数封装了其参数——函数之外的代码看不到这些参数。
        // 这意味着每个函数都会维护自己的环境，其中存储着那些变量。
        // 此外，这个环境必须是动态创建的。每次函数调用都会获得自己的环境，否则，递归就会中断。
        // 通过在执行函数主体时使用不同的环境，用同样的代码调用相同的函数可以产生不同的结果。
        Environment environment = new Environment(closure);
        for (int i = 0; i < declaration.params.size(); i++) {
            environment.define(declaration.params.get(i).lexeme, arguments.get(i));
        }

        try {
            // 一旦函数的主体执行完毕，executeBlock()就会丢弃该函数的本地环境，并恢复调用该函数前的活跃环境。
            interpreter.executeBlock(declaration.body, environment);
        } catch (Return returnValue) {
            // 如果我们在一个初始化方法中执行return语句时，我们仍然返回this，而不是返回值（该值始终是nil）。
            if (isInitializer) return closure.getAt(0, "this");
            return returnValue.value;
        }

        // 如果我们让init()方法总是返回this（即使是被直接调用时），它会使clox中的构造函数实现更加简单。
        // 如果该函数是一个初始化方法，我们会覆盖实际的返回值并强行返回this。这个操作依赖于一个新的isInitializer字段。
        if (isInitializer) return closure.getAt(0, "this");

        return null;
    }
}
