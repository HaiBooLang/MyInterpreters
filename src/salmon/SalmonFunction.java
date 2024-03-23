package salmon;

import java.util.List;

class SalmonFunction implements SalmonCallable {
    private final Stmt.Function declaration;

    SalmonFunction(Stmt.Function declaration) {
        this.declaration = declaration;
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
        Environment environment = new Environment(interpreter.globals);
        for (int i = 0; i < declaration.params.size(); i++) {
            environment.define(declaration.params.get(i).lexeme, arguments.get(i));
        }
        
        try {
            // 一旦函数的主体执行完毕，executeBlock()就会丢弃该函数的本地环境，并恢复调用该函数前的活跃环境。
            interpreter.executeBlock(declaration.body, environment);
        } catch (Return returnValue) {
            return returnValue.value;
        }

        return null;
    }
}
