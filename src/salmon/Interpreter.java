package salmon;

// 这个类声明它是一个访问者。访问方法的返回类型将是Object，即我们在Java代码中用来引用Lox值的根类。
// 为了实现Visitor接口，我们需要为解析器生成的四个表达式树类中分别定义访问方法。
class Interpreter implements Expr.Visitor<Object> {
    @Override
    public Object visitBinaryExpr(Expr.Binary expr) {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case GREATER -> {
                return (double) left > (double) right;
            }
            case GREATER_EQUAL -> {
                return (double) left >= (double) right;
            }
            case LESS -> {
                return (double) left < (double) right;
            }
            case LESS_EQUAL -> {
                return (double) left <= (double) right;
            }
            case MINUS -> {
                return (double) left - (double) right;
            }
            case BANG_EQUAL -> {
                return !isEqual(left, right);
            }
            case EQUAL_EQUAL -> {
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
            }
            case SLASH -> {
                return (double) left / (double) right;
            }
            case STAR -> {
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

        return switch (expr.operator.type) {
            case BANG -> !isTruthy(right);
            // 子表达式结果必须是数字。因为我们在Java中无法静态地知道这一点，所以我们在执行操作之前先对其进行强制转换。
            // 这个类型转换是在运行时对-求值时发生的。这就是将语言动态类型化的核心所在。
            case MINUS -> -(double) right;
            // 遥不可及的。
            default -> null;
        };

    }

    // 只是将表达式发送回解释器的访问者实现中
    private Object evaluate(Expr expr) {
        return expr.accept(this);
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
}
