package lox;

import java.util.ArrayList;
import java.util.List;

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

    Scanner(String source) {
        this.source = source;
    }

    // 扫描器通过自己的方式遍历源代码，添加标记，直到遍历完所有字符。
    // 然后，它在最后附加一个的 "end of file "标记。
    List<Token> scanTokens(){
        while (!isAtEnd()){
            // 我们正处于下一个词汇的开头。
            start = current;
            scanToken();
        }

        tokens.add(new Token(TokenType.EOF,"",null,line));
        return tokens;
    }

    private void scanToken() {
        char c = advance();
        switch (c) {
            // 如果每个词素只有一个字符长。您所需要做的就是消费下一个字符并为其选择一个 token 类型。
            case '(': addToken(TokenType.LEFT_PAREN); break;
            case ')': addToken(TokenType.RIGHT_PAREN); break;
            case '{': addToken(TokenType.LEFT_BRACE); break;
            case '}': addToken(TokenType.RIGHT_BRACE); break;
            case ',': addToken(TokenType.COMMA); break;
            case '.': addToken(TokenType.DOT); break;
            case '-': addToken(TokenType.MINUS); break;
            case '+': addToken(TokenType.PLUS); break;
            case ';': addToken(TokenType.SEMICOLON); break;
            case '*': addToken(TokenType.STAR); break;
            // ！、<、>和=都可以与后面跟随的=来组合成其他相等和比较操作符。对于所有这些情况，我们都需要查看第二个字符。
            case '!':
                addToken(nextMatch('=') ? TokenType.BANG_EQUAL : TokenType.BANG);
                break;
            case '=':
                addToken(nextMatch('=') ? TokenType.EQUAL_EQUAL : TokenType.EQUAL);
                break;
            case '<':
                addToken(nextMatch('=') ? TokenType.LESS_EQUAL : TokenType.LESS);
                break;
            case '>':
                addToken(nextMatch('=') ? TokenType.GREATER_EQUAL : TokenType.GREATER);
                break;

            default:
                // 错误的字符仍然会被前面调用的advance()方法消费。这一点很重要，这样我们就不会陷入无限循环了。
                Lox.error(line, "Unexpected character.");
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

    private boolean nextMatch(char expected) {
        if (isAtEnd()) return false;
        if (source.charAt(current) != expected) return false;

        current++;
        return true;
    }
}
