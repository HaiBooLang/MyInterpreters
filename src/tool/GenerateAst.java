package tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

// 与其繁琐地手写每个类的定义、字段声明、构造函数和初始化器，我们一起编写一个脚本来完成任务。
// 它具有每种树类型（名称和字段）的描述，并打印出定义具有该名称和状态的类所需的Java代码。
// 该脚本是一个微型Java命令行应用程序，它生成一个名为“ Expr.java”的文件：
public class GenerateAst {
    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDir = args[0];

        // 为简便起见，我将表达式类型的描述放入了字符串中。
        // 每一项都包括类的名称，后跟：和以逗号分隔的字段列表。 每个字段都有一个类型和一个名称。
        defineAst(outputDir, "Expr", Arrays.asList(
                "Binary   : Expr left, Token operator, Expr right",
                "Grouping : Expr expression",
                "Literal  : Object value",
                "Unary    : Token operator, Expr right"
        ));
    }

    private static void defineAst(String outputDir, String baseName, List<String> types) throws IOException {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter writer = new PrintWriter(path, StandardCharsets.UTF_8);

        // defineAst()需要做的第一件事是输出基类Expr。
        writer.println("package salmon;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseName + " {");

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
        writer.println("  static class " + className + " extends " + baseName + " {");

        // 为类定义了一个构造函数。
        writer.println("    " + className + "(" + fieldList + ") {");

        // 为每个字段提供参数。
        String[] fields = fieldList.split(", ");
        for (String field : fields) {
            String name = field.split(" ")[1];
            // 在类体中对其初始化。
            writer.println("      this." + name + " = " + name + ";");
        }

        writer.println("    }");

        // 在类体中声明了每个字段。
        writer.println();
        for (String field : fields) {
            writer.println("    final " + field + ";");
        }

        writer.println("  }");
    }
}
