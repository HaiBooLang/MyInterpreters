package lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

    // 用来告诉我们是否已消费完所有字符。
    private boolean isAtEnd() {
        return current >= source.length();
    }

}
