package salmon;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// 这个类声明它是一个访问者。访问方法的返回类型将是Object，即我们在Java代码中用来引用Lox值的根类。
// 为了实现Visitor接口，我们需要为解析器生成的四个表达式树类中分别定义访问方法。
class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
    // 解释器中的environment字段会随着进入和退出局部作用域而改变，它会跟随当前环境。
    // 新加的globals字段则固定指向最外层的全局作用域。
    final Environment globals = new Environment();
    private Environment environment = globals;
    // 我们要把解析信息存储在某个地方，我们会采用另一种常见的方法，将其存储在一个map中，将每个语法树节点与其解析的数据关联起来。
    // 将这些数据存储在节点之外的好处之一就是，可以很容易地丢弃这部分数据——只需要清除map即可。
    private final Map<Expr, Integer> locals = new HashMap<>();

    // 当我们实例化一个解释器时，我们将全局作用域中添加本地函数。
    Interpreter() {
        // 这里有一个名为clock的变量，它的值是一个实现LoxCallable接口的Java匿名类。
        // 这里的clock()函数不接受参数，所以其元数为0。
        // call()方法的实现是直接调用Java函数并将结果转换为以秒为单位的double值。
        globals.define("clock", new SalmonCallable() {
            @Override
            public int arity() {
                return 0;
            }

            @Override
            public Object call(Interpreter interpreter, List<Object> arguments) {
                return (double) System.currentTimeMillis() / 1000.0;
            }

            @Override
            public String toString() {
                return "<native fn>";
            }
        });
    }

    // visit方法是Interpreter类的核心部分，真正的工作是在这里进行的。
    // 我们需要给它们包上一层皮，以便与程序的其他部分对接。解释器的公共API只是一种方法。
    // 该方法会接收一个表达式对应的语法树，并对其进行计算。
    // 如果成功了，evaluate()方法会返回一个对象作为结果值。interpret()方法将结果转为字符串并展示给用户。
    void interpret(List<Stmt> statements) {
        try {
            for (Stmt statement : statements) {
                execute(statement);
            }
        } catch (RuntimeError error) {
            Salmon.runtimeError(error);
        }
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        environment.define(stmt.name.lexeme, null);
        // 我们在当前环境中声明该类的名称。然后我们把类的语法节点转换为LoxClass，即类的运行时表示。
        // 我们回过头来，将类对象存储在我们之前声明的变量中。这个二阶段的变量绑定过程允许在类的方法中引用其自身。
        SalmonClass klass = new SalmonClass(stmt.name.lexeme);
        environment.assign(stmt.name, klass);
        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        while (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.body);
        }
        return null;
    }

    @Override
    public Object visitLogicalExpr(Expr.Logical expr) {
        Object left = evaluate(expr.left);

        if (expr.operator.type == TokenType.OR) {
            if (isTruthy(left)) return left;
        } else {
            if (!isTruthy(left)) return left;
        }

        return evaluate(expr.right);
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        if (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.thenBranch);
        } else if (stmt.elseBranch != null) {
            execute(stmt.elseBranch);
        }
        return null;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        executeBlock(stmt.statements, new Environment(environment));
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        Object value = null;
        if (stmt.initializer != null) {
            value = evaluate(stmt.initializer);
        }

        environment.define(stmt.name.lexeme, value);
        return null;
    }

    @Override
    public Object visitAssignExpr(Expr.Assign expr) {
        Object value = evaluate(expr.value);
        Integer distance = locals.get(expr);
        if (distance != null) {
            environment.assignAt(distance, expr.name, value);
        } else {
            globals.assign(expr.name, value);
        }
        return value;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr) {
        return lookUpVariable(expr.name, expr);
    }

    // 与表达式不同，语句不会产生值，因此visit方法的返回类型是Void.
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        // 我们会接收一个函数语法节点——函数的编译时表示形式——然后将其转换为运行时表示形式。
        SalmonFunction function = new SalmonFunction(stmt, environment);
        // 函数声明与其它文本节点的不同之处在于，声明还会将结果对象绑定到一个新的变量。
        environment.define(stmt.name.lexeme, function);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        Object value = null;
        if (stmt.value != null) value = evaluate(stmt.value);

        throw new Return(value);
    }

    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case GREATER -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left > (double) right;
            }
            case GREATER_EQUAL -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left >= (double) right;
            }
            case LESS -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left < (double) right;
            }
            case LESS_EQUAL -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left <= (double) right;
            }
            case MINUS -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left - (double) right;
            }
            case BANG_EQUAL -> {
                checkNumberOperands(expr.operator, left, right);
                return !isEqual(left, right);
            }
            case EQUAL_EQUAL -> {
                checkNumberOperands(expr.operator, left, right);
                return isEqual(left, right);
            }
            case PLUS -> {
                // +操作符也可以用来拼接两个字符串。
                // 为此，我们不能只是假设操作数是某种类型并将其强制转换，而是要动态地检查操作数类型并选择适当的操作。
                if (left instanceof Double && right instanceof Double) {
                    return (double) left + (double) right;
                }
                if (left instanceof String && right instanceof String) {
                    return (String) left + (String) right;
                }
                // 由于+已经对数字和字符串进行重载，其中已经有检查类型的代码。我们需要做的就是在这两种情况都不匹配时失败。
                throw new RuntimeError(expr.operator, "Operands must be two numbers or two strings.");
            }
            case SLASH -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left / (double) right;
            }
            case STAR -> {
                checkNumberOperands(expr.operator, left, right);
                return (double) left * (double) right;
            }
        }

        // 遥不可及的。
        return null;
    }

    @Override
    public Object visitCallExpr(Expr.Call expr) {
        Object callee = evaluate(expr.callee);

        List<Object> arguments = new ArrayList<>();
        for (Expr argument : expr.arguments) {
            arguments.add(evaluate(argument));
        }

        if (!(callee instanceof SalmonCallable)) {
            throw new RuntimeError(expr.paren, "Can only call functions and classes.");
        }

        SalmonCallable function = (SalmonCallable) callee;

        if (arguments.size() != function.arity()) {
            throw new RuntimeError(expr.paren, "Expected " +
                    function.arity() + " arguments but got " + arguments.size() + ".");
        }

        return function.call(this, arguments);
    }

    // 在表达式中显式使用括号时产生的语法树节点。
    // 一个分组节点中包含一个引用指向对应于括号内的表达式的内部节点。要想计算括号表达式，我们只需要递归地对子表达式求值并返回结果即可。
    @Override
    public Object visitGroupingExpr(Expr.Grouping expr) {
        return null;
    }

    // 我们将字面量树节点转换为运行时值。
    // 我们早在扫描过程中就即时生成了运行时的值，并把它放进了语法标记中。
    // 解析器获取该值并将其插入字面量语法树节点中，所以要对字面量求值，我们只需把它存的值取出来。
    @Override
    public Object visitLiteralExpr(Expr.Literal expr) {
        return expr.value;
    }

    @Override
    public Object visitUnaryExpr(Expr.Unary expr) {
        // 在对一元操作符本身进行计算之前，我们必须先对其操作数子表达式求值。
        // 这表明，解释器正在进行后序遍历——每个节点在自己求值之前必须先对子节点求值。
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case BANG -> {
                return !isTruthy(right);
            }
            case MINUS -> {
                // 子表达式结果必须是数字。因为我们在Java中无法静态地知道这一点，所以我们在执行操作之前先对其进行强制转换。
                // 这个类型转换是在运行时对-求值时发生的。这就是将语言动态类型化的核心所在。
                // 在进行强制转换之前，我们先自己检查对象的类型。
                checkNumberOperand(expr.operator, right);
                return -(double) right;
            }
            // 遥不可及的。
            default -> {
                return null;
            }
        }

    }

    // 让我们看看解析器有什么用处。每次访问一个变量时，它都会告诉解释器，在当前作用域和变量定义的作用域之间隔着多少层作用域。
    // 在运行时，这正好对应于当前环境与解释器可以找到变量值的外围环境之间的environments数量。
    // 解析器通过调用下面的方法将这个数字传递给解释器。
    void resolve(Expr expr, int depth) {
        locals.put(expr, depth);
    }

    // 这里有几件事要做。首先，我们在map中查找已解析的距离值。如果我们没有在map中找到变量对应的距离值，它一定是全局变量。
    // 在这种情况下，我们直接在全局environment中查找。如果变量没有被定义，就会产生一个运行时错误。
    // 如果我们确实查到了一个距离值，那这就是个局部变量，我们可以利用静态分析的结果。
    private Object lookUpVariable(Token name, Expr expr) {
        Integer distance = locals.get(expr);
        if (distance != null) {
            return environment.getAt(distance, name.lexeme);
        } else {
            return globals.get(name);
        }
    }

    // 只是将表达式发送回解释器的访问者实现中
    private Object evaluate(Expr expr) {
        return expr.accept(this);
    }

    // 这类似于处理表达式的evaluate()方法，这是这里处理语句。
    private void execute(Stmt stmt) {
        stmt.accept(this);
    }

    // 这个新方法会在给定的环境上下文中执行一系列语句。在此之前，解释器中的 environment 字段总是指向相同的环境——全局环境。
    // 现在，这个字段会指向当前环境，也就是与要执行的代码的最内层作用域相对应的环境。
    void executeBlock(List<Stmt> statements, Environment environment) {
        Environment previous = this.environment;
        try {
            this.environment = environment;

            for (Stmt statement : statements) {
                execute(statement);
            }
        } finally {
            this.environment = previous;
        }
    }

    // 这是一段像isTruthy()一样的代码，它连接了Lox对象的用户视图和它们在Java中的内部表示。
    private String stringify(Object object) {
        if (object == null) return "nil";

        if (object instanceof Double) {
            String text = object.toString();
            if (text.endsWith(".0")) {
                text = text.substring(0, text.length() - 2);
            }
            return text;
        }

        return object.toString();
    }

    // “真实”指的是什么呢？我们需要简单地讨论一下西方哲学中的一个伟大问题：什么是真理？
    // Lox遵循Ruby的简单规则：false和nil是假的，其他都是真的。
    private boolean isTruthy(Object object) {
        if (object == null) return false;
        if (object instanceof Boolean) return (boolean) object;
        return true;
    }

    private boolean isEqual(Object a, Object b) {
        if (a == null && b == null) return true;
        if (a == null) return false;

        return a.equals(b);
    }

    private void checkNumberOperand(Token operator, Object operand) {
        if (operand instanceof Double) return;
        throw new RuntimeError(operator, "Operand must be a number.");
    }

    private void checkNumberOperands(Token operator, Object left, Object right) {
        if (left instanceof Double && right instanceof Double) return;

        throw new RuntimeError(operator, "Operands must be numbers.");
    }
}
