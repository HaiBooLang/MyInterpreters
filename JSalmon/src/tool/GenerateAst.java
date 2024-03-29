package tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

// 该脚本是一个微型Java命令行应用程序，它生成名为 “Expr.java” 和 “Stmt.java” 的文件：
// 与其繁琐地手写每个类的定义、字段声明、构造函数和初始化器，我们一起编写一个脚本来完成任务。
// 它具有每种树类型（名称和字段）的描述，并打印出定义具有该名称和状态的类所需的Java代码。
public class GenerateAst {
    // 控制代码缩进。
    private static final String tab = "    ";

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDir = args[0];

        // 为简便起见，我将表达式类型的描述放入了字符串中。
        // 每一项都包括类的名称，后跟：和以逗号分隔的字段列表。 每个字段都有一个类型和一个名称。
        defineAst(outputDir, "Expr", Arrays.asList(
                "Grouping : Expr expression",
                "Assign   : Token name, Expr value",
                "Logical  : Expr left, Token operator, Expr right",
                "Binary   : Expr left, Token operator, Expr right",
                "Unary    : Token operator, Expr right",
                "Call     : Expr callee, Token paren, List<Expr> arguments",
                "Get      : Expr object, Token name",
                "Set      : Expr object, Token name, Expr value",
                "This     : Token keyword",
                "Literal  : Object value",
                "Variable : Token name",
                "Super    : Token keyword, Token method"
        ));

        // 语法中没有地方既允许使用表达式，也允许使用语句。
        // 因为这两种语法是不相干的，所以我们不需要提供一个它们都继承的基类。
        // 将表达式和语句拆分为单独的类结构，可使Java编译器帮助我们发现一些愚蠢的错误。
        defineAst(outputDir, "Stmt", Arrays.asList(
                "Class      : Token name, Expr.Variable superclass, List<Stmt.Function> methods",
                "Function   : Token name, List<Token> params, List<Stmt> body",
                "Var        : Token name, Expr initializer",
                "Expression : Expr expression",
                "If         : Expr condition, Stmt thenBranch, Stmt elseBranch",
                "Print      : Expr expression",
                "Return     : Token keyword, Expr value",
                "While      : Expr condition, Stmt body",
                "Block      : List<Stmt> statements"
        ));
    }

    private static void defineAst(String outputDir, String baseName, List<String> types) throws IOException {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter writer = new PrintWriter(path, StandardCharsets.UTF_8);

        // 输出基类Expr。
        writer.println("package salmon;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseName + " {");
        defineVisitor(writer, baseName, types);

        // 定义抽象 accept() 方法。
        writer.println();
        writer.println(tab + "abstract <R> R accept(Visitor<R> visitor);");

        // 在基类内部，我们定义每个子类。
        for (String type : types) {
            String className = type.split(":")[0].trim();
            String fields = type.split(":")[1].trim();
            defineType(writer, baseName, className, fields);
        }

        writer.println("}");
        writer.close();
    }

    private static void defineType(PrintWriter writer, String baseName, String className, String fieldList) {
        writer.println(tab + "static class " + className + " extends " + baseName + " {");

        // 在类体中声明了每个字段。
        String[] fields = fieldList.split(", ");
        for (String field : fields) {
            writer.println(tab + tab + "final " + field + ";");
        }

        // 为类定义了一个构造函数。
        writer.println(tab + tab + className + "(" + fieldList + ") {");
        for (String field : fields) {
            // 在类体中对其初始化。
            String name = field.split(" ")[1];
            writer.println(tab + tab + tab + "this." + name + " = " + name + ";");
        }
        writer.println(tab + tab + "}");

        // 每个子类都实现accept()方法，并调用其类型对应的visit方法。
        writer.println();
        writer.println(tab + tab + "@Override");
        writer.println(tab + tab + "<R> R accept(Visitor<R> visitor) {");
        writer.println(tab + tab + tab + "return visitor.visit" + className + baseName + "(this);");
        writer.println(tab + tab + "}");

        writer.println(tab + "}");
    }

    // 这个函数会生成visitor接口。
    // 在这里，我们遍历所有的子类，并为每个子类声明一个visit方法。当我们以后定义新的表达式类型时，会自动包含这些内容。
    private static void defineVisitor(PrintWriter writer, String baseName, List<String> types) {
        writer.println(tab + "interface Visitor<R> {");

        for (String type : types) {
            String typeName = type.split(":")[0].trim();
            writer.println(tab + tab + "R visit" + typeName + baseName + "(" + typeName + " " + baseName.toLowerCase() + ");");
        }

        writer.println(tab + "}");
    }
}
