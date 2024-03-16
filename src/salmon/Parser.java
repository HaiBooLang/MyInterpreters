package salmon;

import java.util.List;

public class Parser {
    // 这是一个简单的哨兵类，我们用它来帮助解析器摆脱错误。
    // error()方法是返回错误而不是抛出错误，因为我们希望解析器内的调用方法决定是否要跳脱出该错误。
    // 有些解析错误发生在解析器不可能进入异常状态的地方，这时我们就不需要同步。在这些地方，我们只需要报告错误，然后继续解析。
    private static class ParseError extends RuntimeException {}
    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    // expression     → equality ;
    private Expr expression() {
        return equality();
    }

    // equality       → comparison ( ( "!=" | "==" ) comparison )* ;
    private Expr equality() {
        // 规则体中的第一个comparison非终止符变成了方法中对comparison()的第一次调用。
        // 我们获取结果并将其保存在一个局部变量中。
        // 在对等式表达式序列进行压缩时，会创建一个由二元操作符节点组成的左结合嵌套树。
        // ----
        // 如果解析器从未遇到过等式操作符，它就永远不会进入循环。
        // 在这种情况下，equality()方法有效地调用并返回comparison()。
        // 这样一来，这个方法就会匹配一个等式运算符或任何更高优先级的表达式。
        Expr expr = comparison();

        // 然后，规则中的( ... )*循环映射为一个while循环。
        // 在规则体中，我们必须先找到一个 != 或==标记。
        // 因此，如果我们没有看到其中任一标记，我们必须结束相等(不相等)运算符的序列。
        while (match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL)) {
            Token operator = previous();
            Expr right = comparison();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
    // 语法规则与equality几乎完全相同，相应的代码也是如此。
    // 唯一的区别是匹配的操作符的标记类型，而且现在获取操作数时调用的方法是term()而不是comparison()。
    private Expr comparison() {
        Expr expr = term();

        while (match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // term           → factor ( ( "-" | "+" ) factor )* ;
    private Expr term() {
        Expr expr = factor();

        while (match(TokenType.MINUS, TokenType.PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // factor         → unary ( ( "/" | "*" ) unary )* ;
    private Expr factor() {
        Expr expr = unary();

        while (match(TokenType.SLASH, TokenType.STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // unary          → ( "!" | "-" ) unary | primary ;
    // 我们先检查当前的标记以确认要如何进行解析。
    // 如果是!或-，我们一定有一个一元表达式。在这种情况下，我们使用当前的标记递归调用unary()来解析操作数。
    // 将所有这些都包装到一元表达式语法树中，我们就完成了。
    // 否则，我们就达到了最高级别的优先级，即基本表达式。
    private Expr unary() {
        if (match(TokenType.BANG, TokenType.MINUS)) {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return primary();
    }

    // primary        → NUMBER | STRING | "true" | "false" | "nil" | "(" expression ")" ;
    // 该规则中大部分都是终止符，可以直接进行解析。
    private Expr primary() {
        if (match(TokenType.FALSE)) return new Expr.Literal(false);
        if (match(TokenType.TRUE)) return new Expr.Literal(true);
        if (match(TokenType.NIL)) return new Expr.Literal(null);

        if (match(TokenType.NUMBER, TokenType.STRING)) {
            return new Expr.Literal(previous().literal);
        }

        // 当我们匹配了一个开头(并解析了里面的表达式后，我们必须找到一个)标记。如果没有找到，那就是一个错误。
        if (match(TokenType.LEFT_PAREN)) {
            Expr expr = expression();
            consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }
    }

    // 这个检查会判断当前的标记是否属于给定的类型之一。
    // 如果是，则消费该标记并返回true；否则，就返回false并保留当前标记。
    private boolean match(TokenType... types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }

        return false;
    }

    // 如果当前标记属于给定类型，则check()方法返回true。
    // 与match()不同的是，它从不消费标记，只是读取。
    private boolean check(TokenType type) {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    // advance()方法会消费当前的标记并返回它，类似于扫描器中对应方法处理字符的方式。
    private Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    // 检查下一个标记是否是预期的类型。
    // 如果是，它就会消费该标记，一切都很顺利。如果是其它的标记，那么我们就遇到了错误。
    private Token consume(TokenType type, String message) {
        if (check(type)) return advance();

        throw error(peek(), message);
    }

    // isAtEnd()检查我们是否处理完了待解析的标记。
    private boolean isAtEnd() {
        return peek().type == TokenType.EOF;
    }

    // peek()方法返回我们还未消费的当前标记。
    private Token peek() {
        return tokens.get(current);
    }

    // previous()会返回最近消费的标记。
    private Token previous() {
        return tokens.get(current - 1);
    }

    private ParseError error(Token token, String message) {
        Salmon.error(token, message);
        return new ParseError();
    }

    // 我们想要丢弃标记，直至达到下一条语句的开头。这个边界很容易发现——这也是我们选其作为边界的原因。
    // 在分号之后，我们可能就结束了一条语句。大多数语句都通过一个关键字开头——for、if、return、var等等。
    // 当下一个标记是其中之一时，我们可能就要开始一条新语句了。
    // 该方法会不断丢弃标记，直到它发现一个语句的边界。在捕获一个ParseError后，我们会调用该方法，然后我们就有望回到同步状态。
    // 当它工作顺利时，我们就已经丢弃了无论如何都可能会引起级联错误的语法标记，现在我们可以从下一条语句开始解析文件的其余部分。
    private void synchronize() {
        advance();

        while (!isAtEnd()) {
            if (previous().type == TokenType.SEMICOLON) return;

            switch (peek().type) {
                case CLASS:
                case FUN:
                case VAR:
                case FOR:
                case IF:
                case WHILE:
                case PRINT:
                case RETURN:
                    return;
            }

            advance();
        }
    }
}
