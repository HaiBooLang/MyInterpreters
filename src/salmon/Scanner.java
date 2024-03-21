package salmon;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static salmon.TokenType.*;

// 扫描器的核心是一个循环。从源码的第一个字符开始，扫描器计算出该字符属于哪个词素，并消费它和属于该词素的任何后续字符。
// 当到达该词素的末尾时，扫描器会输出一个标记（词法单元 token）。
// 然后再循环一次，它又循环回来，从源代码中的下一个字符开始再做一次。它一直这样做，吃掉字符，偶尔，呃，排出标记，直到它到达输入的终点。
// 决定一门语言如何将字符分组为词素的规则被称为它的词法语法。
// 在Lox中，和大多数编程语言一样，该语法的规则非常简单，可以将其归为正则语言。
// 如果你愿意，你可以非常精确地使用正则表达式来识别Lox的所有不同词组，而且还有一堆有趣的理论来支撑着为什么会这样以及它的意义。
public class Scanner {
    // 我们将原始的源代码存储为一个简单的字符串。
    private final String source;
    // 并且我们已经准备了一个列表来保存扫描时产生的标记。
    private final List<Token> tokens = new ArrayList<>();
    // start字段指向被扫描的词素中的第一个字符.
    private int start = 0;
    // current字段指向当前正在处理的字符。
    private int current = 0;
    // line字段跟踪的是current所在的源文件行数，这样我们产生的标记就可以知道其位置。
    private int line = 1;
    private static final Map<String, TokenType> keywords;

    // 为了处理关键字，我们要查看标识符的词素是否是保留字之一。
    // 如果是，我们就使用该关键字特有的标记类型。我们在map中定义保留字的集合。
    static {
        keywords = new HashMap<>();
        keywords.put("and", AND);
        keywords.put("class", CLASS);
        keywords.put("else", ELSE);
        keywords.put("false", FALSE);
        keywords.put("for", FOR);
        keywords.put("fun", FUN);
        keywords.put("if", IF);
        keywords.put("nil", NIL);
        keywords.put("or", OR);
        keywords.put("print", PRINT);
        keywords.put("return", RETURN);
        keywords.put("super", SUPER);
        keywords.put("this", THIS);
        keywords.put("true", TRUE);
        keywords.put("var", VAR);
        keywords.put("while", WHILE);
    }

    Scanner(String source) {
        this.source = source;
    }

    // 扫描器通过自己的方式遍历源代码，添加标记，直到遍历完所有字符。
    // 然后，它在最后附加一个的 "end of file "标记。
    public List<Token> scanTokens() {
        while (!isAtEnd()) {
            // 我们正处于下一个词汇的开头。
            start = current;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    private void scanToken() {
        char c = advance();
        switch (c) {
            // 如果每个词素只有一个字符长。您所需要做的就是消费下一个字符并为其选择一个 token 类型。
            case '(':
                addToken(LEFT_PAREN);
                break;
            case ')':
                addToken(RIGHT_PAREN);
                break;
            case '{':
                addToken(LEFT_BRACE);
                break;
            case '}':
                addToken(RIGHT_BRACE);
                break;
            case ',':
                addToken(COMMA);
                break;
            case '.':
                addToken(DOT);
                break;
            case '-':
                addToken(MINUS);
                break;
            case '+':
                addToken(PLUS);
                break;
            case ';':
                addToken(SEMICOLON);
                break;
            case '*':
                addToken(STAR);
                break;
            // ！、<、>和=都可以与后面跟随的=来组合成其他相等和比较操作符。对于所有这些情况，我们都需要查看第二个字符。
            case '!':
                addToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                addToken(match('=') ? LESS_EQUAL : LESS);
                break;
            case '>':
                addToken(match('=') ? GREATER_EQUAL : GREATER);
                break;
            case '/':
                // 这与其它的双字符操作符是类似的，区别在于我们找到第二个/时，还没有结束本次标记。相反，我们会继续消费字符直至行尾。
                // 注释是词素，但是它们没有含义，而且解析器也不想要处理它们。
                if (match('/')) {
                    // 这就是为什么我们使用peek()而不是match()来查找注释结尾的换行符。
                    // 我们到这里希望能读取到换行符，这样我们就可以更新行数了
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    addToken(SLASH);
                }
                break;
            case ' ':
            case '\r':
            case '\t':
                // 跳过其它无意义的字符：换行和空格。
                // 当遇到空白字符时，我们只需回到扫描循环的开头。这样就会在空白字符之后开始一个新的词素。
                break;
            case '\n':
                line++;
                break;
            case '"':
                processString();
                break;

            default:
                // 为了识别数字词素的开头，我们会寻找任何一位数字。
                // 为每个十进制数字添加case分支有点乏味，所以我们直接在默认分支中进行处理。
                // ----
                // 最大匹配原则——当两个语法规则都能匹配扫描器正在处理的一大块代码时，哪个规则相匹配的字符最多，就使用哪个规则。
                // 最大匹配原则意味着，我们只有扫描完一个可能是标识符的片段，才能确认是否一个保留字。
                // 毕竟，保留字也是一个标识符，只是一个已经被语言要求为自己所用的标识符。这也是保留字一词的由来。
                // 所以我们首先假设任何以字母或下划线开头的词素都是一个标识符。
                if (isDigit(c)) {
                    // 一旦我们知道当前在处理数字，我们就分支进入一个单独的方法消费剩余的字面量，跟字符串的处理类似。
                    processNumber();
                } else if (isAlpha(c)) {
                    processIdentifier();
                } else {
                    // 错误的字符仍然会被前面调用的advance()方法消费。这一点很重要，这样我们就不会陷入无限循环了。
                    Salmon.error(line, "Unexpected character.");
                }
                break;
        }
    }

    // 用来告诉我们是否已消费完所有字符。
    private boolean isAtEnd() {
        return current >= source.length();
    }

    // 获取源文件中的下一个字符并返回它。
    private char advance() {
        current++;
        return source.charAt(current - 1);
    }

    private void addToken(TokenType type) {
        addToken(type, null);
    }

    // 获取当前词素的文本并为其创建一个新 token。
    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }

    private boolean match(char expected) {
        if (isAtEnd()) return false;
        if (source.charAt(current) != expected) return false;

        current++;
        return true;
    }

    // 这有点像advance()方法，只是不会消费字符。这就是所谓的lookahead(前瞻)。
    // 因为它只关注当前未消费的字符，所以我们有一个前瞻字符。
    private char peek() {
        if (isAtEnd()) return '\0';
        return source.charAt(current);
    }

    private char peekNext() {
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '_';
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    // 我们会一直消费字符，直到"结束该字符串。如果输入内容耗尽，我们也会进行优雅的处理，并报告一个对应的错误。
    // Lox支持多行字符串。这有利有弊，但禁止换行比允许换行更复杂一些，所以我把它们保留了下来。
    // 这意味着当我们在字符串内遇到新行时，我们也需要更新line值。
    // 当我们创建标记时，我们也会产生实际的字符串值，该值稍后将被解释器使用。
    // 这里，值的转换只需要调用substring()剥离前后的引号。如果Lox支持转义序列，比如\n，我们会在这里取消转义。
    private void processString() {
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++;
            advance();
        }

        if (isAtEnd()) {
            Salmon.error(line, "Unterminated string.");
            return;
        }

        // 闭合的 "。
        advance();

        // 修剪周围的引号。
        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }

    // 我们在字面量的整数部分中尽可能多地获取数字。
    // 然后我们寻找小数部分，也就是一个小数点(.)后面至少跟一个数字。
    // 如果确实有小数部分，同样地，我们也尽可能多地获取数字。
    private void processNumber() {
        while (isDigit(peek())) advance();

        // 寻找小数部分。
        // 在定位到小数点之后需要继续前瞻第二个字符，因为我们只有确认其后有数字才会消费.。
        if (peek() == '.' && isDigit(peekNext())) {
            // 消费 "."
            advance();

            while (isDigit(peek())) advance();
        }

        // 最后，我们将词素转换为其对应的数值。我们的解释器使用Java的Double类型来表示数字，所以我们创建一个该类型的值。
        addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
    }

    private void processIdentifier() {
        while (isAlphaNumeric(peek())) advance();

        // 在我们扫描到标识符之后，要检查是否与map中的某些项匹配。
        // 如果匹配的话，就使用关键字的标记类型。否则，就是一个普通的用户定义的标识符。
        String text = source.substring(start, current);
        TokenType type = keywords.get(text);
        if (type == null) type = IDENTIFIER;
        addToken(type);
    }
}
