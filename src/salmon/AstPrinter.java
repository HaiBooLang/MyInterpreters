//package salmon;
//
//class AstPrinter implements Expr.Visitor<String> {
//    String print(Expr expr) {
//        return expr.accept(this);
//    }
//
//    @Override
//    public String visitBinaryExpr(Expr.Binary expr) {
//        return parenthesize(expr.operator.lexeme, expr.left, expr.right);
//    }
//
//    @Override
//    public String visitGroupingExpr(Expr.Grouping expr) {
//        return parenthesize("group", expr.expression);
//    }
//
//    @Override
//    public String visitLiteralExpr(Expr.Literal expr) {
//        if (expr.value == null) return "nil";
//        return expr.value.toString();
//    }
//
//    @Override
//    public String visitUnaryExpr(Expr.Unary expr) {
//        return parenthesize(expr.operator.lexeme, expr.right);
//    }
//
//    // 接收一个名称和一组子表达式作为参数，将它们全部包装在圆括号中，并生成一个如下的字符串：(+ 1 2)。
//    // 请注意，它在每个子表达式上调用accept()并将自身传递进去。 这是递归步骤，可让我们打印整棵树。
//    private String parenthesize(String name, Expr... exprs) {
//        StringBuilder builder = new StringBuilder();
//
//        builder.append("(").append(name);
//        for (Expr expr : exprs) {
//            builder.append(" ");
//            builder.append(expr.accept(this));
//        }
//        builder.append(")");
//
//        return builder.toString();
//    }
//
////    public static void main(String[] args) {
////        Expr expression = new Expr.Binary(
////                new Expr.Unary(
////                        new Token(TokenType.MINUS, "-", null, 1),
////                        new Expr.Literal(123)),
////                new Token(TokenType.STAR, "*", null, 1),
////                new Expr.Grouping(
////                        new Expr.Literal(45.67)));
////
////        System.out.println(new AstPrinter().print(expression));
////    }
//}