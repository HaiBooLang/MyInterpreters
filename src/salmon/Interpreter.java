package salmon;

import java.util.List;

// 这个类声明它是一个访问者。访问方法的返回类型将是Object，即我们在Java代码中用来引用Lox值的根类。
// 为了实现Visitor接口，我们需要为解析器生成的四个表达式树类中分别定义访问方法。
class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
    private Environment environment = new Environment();

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
        environment.assign(expr.name, value);
        return value;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr) {
        return environment.get(expr.name);
    }

    // 与表达式不同，语句不会产生值，因此visit方法的返回类型是Void.
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
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
