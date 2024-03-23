package salmon;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static salmon.TokenType.*;

public class Parser {
    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    // program        → statement* EOF ;
    // program是语法的起点，代表一个完整的Lox脚本或REPL输入项。程序是一个语句列表，后面跟着特殊的“文件结束”(EOF)标记。
    // 强制性的结束标记可以确保解析器能够消费所有输入内容，而不会默默地忽略脚本结尾处错误的、未消耗的标记。
    List<Stmt> parse() {
        List<Stmt> statements = new ArrayList<>();
        while (!isAtEnd()) {
            statements.add(declaration());
        }
        return statements;
    }

    // program        → declaration* EOF ;

    // declaration    → funDecl
    //                | varDecl
    //                | statement ;
    private Stmt declaration() {
        try {
            if (match(FUN)) return function("function");
            if (match(VAR)) return varDeclaration();
            return statement();
        } catch (ParseError error) {
            // 当解析器进入恐慌模式时，它就是进行同步的正确位置。
            // 该方法的整个主体都封装在一个try块中，以捕获解析器开始错误恢复时抛出的异常。这样可以让解析器跳转到解析下一个语句或声明的开头。
            synchronize();
            return null;
        }
    }

    // funDecl        → "fun" function ;
    // function       → IDENTIFIER "(" parameters? ")" block ;
    // parameters     → IDENTIFIER ( "," IDENTIFIER )* ;
    // 你可能会对这里的kind参数感到疑惑。就像我们复用语法规则一样，稍后我们也会复用function()方法来解析类中的方法。
    private Stmt.Function function(String kind) {
        // 消费了标识符标记作为函数名称。
        Token name = consume(IDENTIFIER, "Expect " + kind + " name.");

        // 解析参数列表和包裹着它们的一对小括号。
        consume(LEFT_PAREN, "Expect '(' after " + kind + " name.");
        List<Token> parameters = new ArrayList<>();
        if (!check(RIGHT_PAREN)) {
            do {
                if (parameters.size() >= 255) {
                    error(peek(), "Can't have more than 255 parameters.");
                }

                parameters.add(consume(IDENTIFIER, "Expect parameter name."));
            } while (match(COMMA));
        }
        consume(RIGHT_PAREN, "Expect ')' after parameters.");

        // 解析函数主体，并将其封装为一个函数节点。
        consume(LEFT_BRACE, "Expect '{' before " + kind + " body.");
        List<Stmt> body = block();
        return new Stmt.Function(name, parameters, body);
    }

    // varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;
    private Stmt varDeclaration() {
        Token name = consume(IDENTIFIER, "Expect variable name.");

        Expr initializer = null;
        if (match(EQUAL)) {
            initializer = expression();
        }

        consume(SEMICOLON, "Expect ';' after variable declaration.");
        return new Stmt.Var(name, initializer);
    }

    // statement      → exprStmt
    //                | forStmt
    //                | ifStmt
    //                | printStmt
    //                | returnStmt
    //                | whileStmt
    //                | block ;
    // 如果下一个标记看起来不像任何已知类型的语句，我们就认为它一定是一个表达式语句。
    // 这是解析语句时典型的最终失败分支，因为我们很难通过第一个标记主动识别出一个表达式。
    private Stmt statement() {
        if (match(FOR)) return forStatement();
        if (match(IF)) return ifStatement();
        if (match(PRINT)) return printStatement();
        if (match(RETURN)) return returnStatement();
        if (match(WHILE)) return whileStatement();
        if (match(LEFT_BRACE)) return new Stmt.Block(block());

        return expressionStatement();
    }

    // forStmt        → "for" "(" ( varDecl | exprStmt | ";" )
    //                  expression? ";"
    //                  expression? ")" statement ;
    private Stmt forStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'for'.");

        // 接下来的第一个子句是初始化式。
        Stmt initializer;
        if (match(SEMICOLON)) {
            initializer = null;
        } else if (match(VAR)) {
            initializer = varDeclaration();
        } else {
            initializer = expressionStatement();
        }

        // 接下来是条件表达式。
        Expr condition = null;
        if (!check(SEMICOLON)) {
            condition = expression();
        }
        consume(SEMICOLON, "Expect ';' after loop condition.");

        // 最后一个子句是增量语句。
        Expr increment = null;
        if (!check(RIGHT_PAREN)) {
            increment = expression();
        }
        consume(RIGHT_PAREN, "Expect ')' after for clauses.");

        // 剩下的就是循环主体了。
        Stmt body = statement();

        // 我们利用这些变量来合成表示for循环语义的语法树节点。
        if (increment != null) {
            // 如果存在增量子句的话，会在循环的每个迭代中在循环体结束之后执行。
            // 我们用一个代码块来代替循环体，这个代码块中包含原始的循环体，后面跟一个执行增量子语句的表达式语句。
            body = new Stmt.Block(Arrays.asList(body, new Stmt.Expression(increment)));
        }

        // 接下来，我们获取条件式和循环体，并通过基本的while语句构建对应的循环。
        // 如果条件式被省略了，我们就使用true来创建一个无限循环。
        if (condition == null) condition = new Expr.Literal(true);
        body = new Stmt.While(condition, body);

        // 最后，如果有初始化式，它会在整个循环之前运行一次。
        if (initializer != null) {
            body = new Stmt.Block(Arrays.asList(initializer, body));
        }

        return body;
    }

    // ifStmt         → "if" "(" expression ")" statement
    //                ( "else" statement )? ;
    private Stmt ifStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'if'.");
        Expr condition = expression();
        consume(RIGHT_PAREN, "Expect ')' after if condition.");

        Stmt thenBranch = statement();
        Stmt elseBranch = null;
        if (match(ELSE)) {
            elseBranch = statement();
        }

        return new Stmt.If(condition, thenBranch, elseBranch);
    }

    // printStmt      → "print" expression ";" ;
    // 因为我们已经匹配并消费了print标记本身，所以这里不需要重复消费。我们先解析随后的表达式，消费表示语句终止的分号，并生成语法树。
    private Stmt printStatement() {
        Expr value = expression();
        consume(SEMICOLON, "Expect ';' after value.");
        return new Stmt.Print(value);
    }

    // returnStmt     → "return" expression? ";" ;
    private Stmt returnStatement() {
        Token keyword = previous();
        Expr value = null;
        // 因为很多不同的标记都可以引出一个表达式，所以很难判断是否存在返回值。
        // 相反，我们检查它是否不存在。因为分号不能作为表达式的开始，如果下一个标记是分号，我们就知道一定没有返回值。
        if (!check(SEMICOLON)) {
            value = expression();
        }

        consume(SEMICOLON, "Expect ';' after return value.");
        return new Stmt.Return(keyword, value);
    }


    // whileStmt      → "while" "(" expression ")" statement ;
    private Stmt whileStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'while'.");
        Expr condition = expression();
        consume(RIGHT_PAREN, "Expect ')' after condition.");
        Stmt body = statement();

        return new Stmt.While(condition, body);
    }

    private List<Stmt> block() {
        List<Stmt> statements = new ArrayList<>();

        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            statements.add(declaration());
        }

        consume(RIGHT_BRACE, "Expect '}' after block.");
        return statements;
    }

    // exprStmt       → expression ";" ;
    private Stmt expressionStatement() {
        Expr expr = expression();
        consume(SEMICOLON, "Expect ';' after expression.");
        return new Stmt.Expression(expr);
    }

    // expression     → assignment ;
    private Expr expression() {
        return assignment();
    }

    // assignment     → IDENTIFIER "=" assignment
    //                | logic_or ;
    // 在创建赋值表达式节点之前，我们先查看左边的表达式，弄清楚它是什么类型的赋值目标。然后我们将右值表达式节点转换为左值的表示形式。
    private Expr assignment() {
        Expr expr = or();

        if (match(EQUAL)) {
            Token equals = previous();
            Expr value = assignment();

            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable) expr).name;
                return new Expr.Assign(name, value);
            }

            error(equals, "Invalid assignment target.");
        }

        return expr;
    }

    // logic_or       → logic_and ( "or" logic_and )* ;
    private Expr or() {
        Expr expr = and();

        while (match(OR)) {
            Token operator = previous();
            Expr right = and();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }

    // logic_and      → equality ( "and" equality )* ;
    private Expr and() {
        Expr expr = equality();

        while (match(AND)) {
            Token operator = previous();
            Expr right = equality();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
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
        while (match(BANG_EQUAL, EQUAL_EQUAL)) {
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

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // term           → factor ( ( "-" | "+" ) factor )* ;
    private Expr term() {
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // factor         → unary ( ( "/" | "*" ) unary )* ;
    private Expr factor() {
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    // unary          → ( "!" | "-" ) unary | call ;
    // 我们先检查当前的标记以确认要如何进行解析。
    // 如果是!或-，我们一定有一个一元表达式。在这种情况下，我们使用当前的标记递归调用unary()来解析操作数。
    // 将所有这些都包装到一元表达式语法树中，我们就完成了。
    // 否则，我们就达到了最高级别的优先级，即基本表达式。
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return call();
    }

    // call           → primary ( "(" arguments? ")" )* ;
    private Expr call() {
        Expr expr = primary();

        while (true) {
            if (match(LEFT_PAREN)) {
                expr = finishCall(expr);
            } else {
                break;
            }
        }

        return expr;
    }

    // arguments      → expression ( "," expression )* ;
    private Expr finishCall(Expr callee) {
        List<Expr> arguments = new ArrayList<>();
        if (!check(RIGHT_PAREN)) {
            do {
                // Lox的Java解释器实际上并不需要限制，但是设置一个最大的参数数量限制可以简化第三部分中的字节码解释器。
                if (arguments.size() >= 255) {
                    // 如果发现参数过多，这里的代码会报告一个错误，但是不会抛出该错误。
                    // 在这里，解析器仍然处于完全有效的状态，只是发现了太多的参数。所以它会报告这个错误，并继续执行解析。
                    error(peek(), "Can't have more than 255 arguments.");
                }
                arguments.add(expression());
            } while (match(COMMA));
        }

        Token paren = consume(RIGHT_PAREN, "Expect ')' after arguments.");

        return new Expr.Call(callee, paren, arguments);
    }

    // primary        → NUMBER | STRING | "true" | "false" | "nil" | "(" expression ")" ;
    // 该规则中大部分都是终止符，可以直接进行解析。
    private Expr primary() {
        if (match(FALSE)) return new Expr.Literal(false);
        if (match(TRUE)) return new Expr.Literal(true);
        if (match(NIL)) return new Expr.Literal(null);

        if (match(NUMBER, STRING)) {
            return new Expr.Literal(previous().literal);
        }

        if (match(IDENTIFIER)) {
            return new Expr.Variable(previous());
        }

        // 当我们匹配了一个开头(并解析了里面的表达式后，我们必须找到一个)标记。如果没有找到，那就是一个错误。
        if (match(LEFT_PAREN)) {
            Expr expr = expression();
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        // 当解析器在每个语法规则的解析方法中下降时，它最终会进入primary()。
        // 如果该方法中的case都不匹配，就意味着我们正面对一个不是表达式开头的语法标记。我们也需要处理这个错误。
        throw error(peek(), "Expect expression.");
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
        return peek().type == EOF;
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
            if (previous().type == SEMICOLON) return;

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

    // 这是一个简单的哨兵类，我们用它来帮助解析器摆脱错误。
    // error()方法是返回错误而不是抛出错误，因为我们希望解析器内的调用方法决定是否要跳脱出该错误。
    // 有些解析错误发生在解析器不可能进入异常状态的地方，这时我们就不需要同步。在这些地方，我们只需要报告错误，然后继续解析。
    private static class ParseError extends RuntimeException {
    }
}
