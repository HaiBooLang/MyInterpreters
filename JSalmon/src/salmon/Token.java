package salmon;

// 我们将所有这些数据打包到一个类中。现在我们有了一个信息充分的对象，足以支撑解释器的所有后期阶段。
public class Token {
    final TokenType type;
    final String lexeme;
    final Object literal;
    final int line;

    Token(TokenType type, String lexeme, Object literal, int line) {
        this.type = type;
        this.lexeme = lexeme;
        this.literal = literal;
        this.line = line;
    }

    public String toString() {
        return type + " " + lexeme + " " + literal;
    }
}
