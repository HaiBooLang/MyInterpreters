package salmon;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

// Lox是一种脚本语言，这意味着它直接从源代码执行。
public class Salmon {
    // 我们把这个字段设置为静态的，这样在一个REPL会话中连续调用run()时就会重复使用同一个解释器。
    private static final Interpreter interpreter = new Interpreter();
    // 我们将以此来确保我们不会尝试执行有已知错误的代码。
    static boolean hadError = false;
    // 我们使用与RuntimeError关联的标记来告诉用户错误发生时正在执行哪一行代码。展示错误之后，runtimeError()会设置以下字段.
    static boolean hadRuntimeError = false;

    public static void main(String[] args) throws IOException {
        if (args.length > 1) {
            System.out.println("Usage: jlox [script]");
            System.exit(64);
            // 我们的解释器支持两种运行代码的方式。
            // 如果从命令行启动jlox并为其提供文件路径，它将读取该文件并执行。
        } else if (args.length == 1) {
            runFile(args[0]);
            // 如果你想与你的解释器对话, 可以交互式的启动它。
            // 启动的时候不加任何参数就可以了，它会有一个提示符，你可以在提示符处一次输入并执行一行代码。
        } else {
            runPrompt();
        }
    }

    private static void runFile(String path) throws IOException {
        byte[] bytes = Files.readAllBytes(Paths.get(path));
        run(new String(bytes, Charset.defaultCharset()));

        // 在退出代码中指出错误
        if (hadError) System.exit(65);
        if (hadRuntimeError) System.exit(70);
    }

    private static void runPrompt() throws IOException {
        InputStreamReader input = new InputStreamReader(System.in);
        BufferedReader reader = new BufferedReader(input);

        while (true) {
            System.out.print("> ");
            // 读取用户在命令行上的一行输入，并返回结果。
            // 要终止交互式命令行应用程序，通常需要输入Control-D。这样做会向程序发出 "文件结束" 的信号。
            // 当这种情况发生时，readLine()就会返回null，所以我们检查一下是否存在null以退出循环。
            String line = reader.readLine();
            if (line != null) {
                run(line);
                // 我们需要在交互式循环中重置此标志。如果用户输入有误，也不应终止整个会话。
                hadError = false;
            }
        }
    }

    // 交互式提示符和文件运行工具都是对这个核心函数的简单包装。
    private static void run(String source) {
        Scanner scanner = new Scanner(source);
        List<Token> tokens = scanner.scanTokens();

//        for (Token token : tokens) {
//            System.out.println(token);
//        }

        Parser parser = new Parser(tokens);
        Expr expression = parser.parse();

        // 如果出现语法错误则停止。
        if (hadError) return;

        interpreter.interpret(expression);
//        System.out.println("AST: " + new AstPrinter().print(expression));
    }

    // 我们的语言提供的处理错误的工具构成了其用户界面的很大一部分。
    // 当用户的代码在工作时，他们根本不会考虑我们的语言——他们的脑子里都是他们的程序。
    // 通常只有当程序出现问题时，他们才会注意到我们的实现。
    // 当这种情况发生时，我们就需要向用户提供他们所需要的所有信息，让他们了解哪里出了问题，并引导他们慢慢达到他们想要去的地方。
    static void error(int line, String message) {
        report(line, "", message);
    }

    // 该方法会报告给定标记处的错误。它显示了标记的位置和标记本身。
    static void error(Token token, String message) {
        if (token.type == TokenType.EOF) {
            report(token.line, " at end", message);
        } else {
            report(token.line, " at '" + token.lexeme + "'", message);
        }
    }

    static void runtimeError(RuntimeError error) {
        System.err.println(error.getMessage() + "\n[line " + error.token.line + "]");
        hadRuntimeError = true;
    }

    private static void report(int line, String where, String message) {
        System.err.println("[line " + line + "] Error" + where + ": " + message);
        hadError = true;
    }
}
