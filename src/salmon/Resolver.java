package salmon;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {
    private final Interpreter interpreter;
    // 词法作用域在解释器和解析器中都有使用。它们的行为像一个栈。
    // 解释器是使用链表（Environment对象组成的链）来实现栈的，在解析器中，我们使用一个真正的Java Stack。
    // 这个字段会记录当前作用域内的栈。栈中的每个元素是代表一个块作用域的Map。
    // 作用域map中与key相关联的值代表的是我们是否已经结束了对变量初始化式的解析。
    // 作用域栈只用于局部块作用域。解析器不会跟踪在全局作用域的顶层声明的变量，因为它们在Lox中是更动态的。
    // 当解析一个变量时，如果我们在本地作用域栈中找不到它，我们就认为它一定是全局的。
    private final Stack<Map<String, Boolean>> scopes = new Stack<>();
    // 这里执行了一个return语句，但它甚至根本不在函数内部。这是一个顶层代码。
    // 我不知道用户认为会发生什么，但是我认为我们不希望Lox允许这种做法。
    // 就像我们遍历语法树时跟踪作用域一样，我们也可以跟踪当前访问的代码是否在一个函数声明内部。
    private FunctionType currentFunction = FunctionType.NONE;
    private ClassType currentClass = ClassType.NONE;


    Resolver(Interpreter interpreter) {
        this.interpreter = interpreter;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt) {
        // 这里会开始一个新的作用域，遍历块中的语句，然后丢弃该作用域。
        beginScope();
        resolve(stmt.statements);
        endScope();
        return null;
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt) {
        ClassType enclosingClass = currentClass;
        currentClass = ClassType.CLASS;

        declare(stmt.name);
        define(stmt.name);

        // 在我们开始分析方法体之前，我们推入一个新的作用域，并在其中像定义变量一样定义“this”。然后，当我们完成后，会丢弃这个外围作用域。
        // 现在，只要遇到this表达式（至少是在方法内部），它就会解析为一个“局部变量”，该变量定义在方法体块之外的隐含作用域中。
        beginScope();
        scopes.peek().put("this", true);
        for (Stmt.Function method : stmt.methods) {
            FunctionType declaration = FunctionType.METHOD;
            resolveFunction(method, declaration);
        }
        endScope();

        currentClass = enclosingClass;

        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt) {
        declare(stmt.name);
        if (stmt.initializer != null) {
            resolve(stmt.initializer);
        }
        define(stmt.name);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr) {
        // 首先，我们要检查变量是否在其自身的初始化式中被访问。这也就是作用域map中的值发挥作用的地方。
        // 如果当前作用域中存在该变量，但是它的值是false，意味着我们已经声明了它，但是还没有定义它。我们会报告一个错误出来。
        if (!scopes.isEmpty() && scopes.peek().get(expr.name.lexeme) == Boolean.FALSE) {
            Salmon.error(expr.name, "Can't read local variable in its own initializer.");
        }

        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitAssignExpr(Expr.Assign expr) {
        // 首先，我们解析右值的表达式，以防它还包含对其它变量的引用。
        resolve(expr.value);
        // 然后使用现有的 resolveLocal() 方法解析待赋值的变量。
        resolveLocal(expr, expr.name);
        return null;
    }

    // 函数既绑定名称又引入了作用域。函数本身的名称被绑定在函数声明时所在的作用域中。
    // 当我们进入函数体时，我们还需要将其参数绑定到函数内部作用域中。
    // 与变量不同的是，我们在解析函数体之前，就急切地定义了这个名称。这样函数就可以在自己的函数体中递归地使用自身。
    @Override
    public Void visitFunctionStmt(Stmt.Function stmt) {
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);
        return null;
    }

    // 一个表达式语句中包含一个需要遍历的表达式。
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt) {
        resolve(stmt.expression);
        return null;
    }

    // if语句包含一个条件表达式，以及一个或两个分支语句。
    // 当我们解析if语句时，没有控制流。我们会解析条件表达式和两个分支表达式。
    // 动态执行则只会进入正在执行的分支，而静态分析是保守的——它会分析所有可能执行的分支。
    // 因为任何一个分支在运行时都可能被触及，所以我们要对两者都进行解析。
    @Override
    public Void visitIfStmt(Stmt.If stmt) {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);
        if (stmt.elseBranch != null) resolve(stmt.elseBranch);
        return null;
    }

    // 与表达式语句类似，print语句也包含一个子表达式。
    @Override
    public Void visitPrintStmt(Stmt.Print stmt) {
        resolve(stmt.expression);
        return null;
    }

    // return语句也是相同的。
    @Override
    public Void visitReturnStmt(Stmt.Return stmt) {
        if (currentFunction == FunctionType.NONE) {
            Salmon.error(stmt.keyword, "Can't return from top-level code.");
        }

        if (stmt.value != null) {
            resolve(stmt.value);
        }

        return null;
    }

    // 与if语句一样，对于while语句，我们会解析其条件，并解析一次循环体。
    @Override
    public Void visitWhileStmt(Stmt.While stmt) {
        resolve(stmt.condition);
        resolve(stmt.body);
        return null;
    }

    // 我们的老朋友二元表达式。我们要遍历并解析两个操作数。
    @Override
    public Void visitBinaryExpr(Expr.Binary expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    // 调用也是类似的——我们遍历参数列表并解析它们。
    // 被调用的对象也是一个表达式（通常是一个变量表达式），所以它也会被解析。
    @Override
    public Void visitCallExpr(Expr.Call expr) {
        resolve(expr.callee);

        for (Expr argument : expr.arguments) {
            resolve(argument);
        }

        return null;
    }

    @Override
    public Void visitGetExpr(Expr.Get expr) {
        resolve(expr.object);
        return null;
    }

    // 括号表达式比较简单。
    @Override
    public Void visitGroupingExpr(Expr.Grouping expr) {
        resolve(expr.expression);
        return null;
    }

    // 字面表达式中没有使用任何变量，也不包含任何子表达式，所以也不需要做任何事情。
    @Override
    public Void visitLiteralExpr(Expr.Literal expr) {
        return null;
    }

    // 因为静态分析没有控制流或短路处理，逻辑表达式与其它的二元运算符是一样的。
    @Override
    public Void visitLogicalExpr(Expr.Logical expr) {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    public Void visitSetExpr(Expr.Set expr) {
        resolve(expr.value);
        resolve(expr.object);
        return null;
    }

    @Override
    public Void visitThisExpr(Expr.This expr) {
        if (currentClass == ClassType.NONE) {
            Salmon.error(expr.keyword, "Can't use 'this' outside of a class.");
            return null;
        }

        resolveLocal(expr, expr.keyword);
        return null;
    }

    // 接下来是最后一个节点，我们解析它的一个操作数。
    @Override
    public Void visitUnaryExpr(Expr.Unary expr) {
        resolve(expr.right);
        return null;
    }

    // 在运行时，声明一个函数不会对函数体做任何处理。
    // 直到后续函数被调用时，才会触及主体。在静态分析中，我们会立即遍历函数体。
    private void resolveFunction(Stmt.Function function, FunctionType type) {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type;
        beginScope();
        for (Token param : function.params) {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();
        currentFunction = enclosingFunction;
    }

    // 我们从最内层的作用域开始，向外扩展，在每个map中寻找一个可以匹配的名称。
    // 如果我们找到了这个变量，我们就对其解析，传入当前最内层作用域和变量所在作用域之间的作用域的数量。
    // 如果我们遍历了所有的作用域也没有找到这个变量，我们就不解析它，并假定它是一个全局变量。
    private void resolveLocal(Expr expr, Token name) {
        for (int i = scopes.size() - 1; i >= 0; i--) {
            if (scopes.get(i).containsKey(name.lexeme)) {
                interpreter.resolve(expr, scopes.size() - 1 - i);
                return;
            }
        }
    }

    // 声明将变量添加到最内层的作用域，这样它就会遮蔽任何外层作用域，我们也就知道了这个变量的存在。
    // 当局部变量的初始化式指向一个与当前声明变量名称相同的变量时，我们要将其作为一个编译错误而不是运行时错误。
    private void declare(Token name) {
        if (scopes.isEmpty()) return;
        Map<String, Boolean> scope = scopes.peek();

        // 我们确实允许在全局作用域内声明多个同名的变量，但在局部作用域内这样做可能是错误的。
        if (scope.containsKey(name.lexeme)) {
            Salmon.error(name, "Already variable with this name in this scope.");
        }

        // 我们通过在作用域map中将其名称绑定到false来表明该变量“尚未就绪”。
        scope.put(name.lexeme, false);
    }

    // 在声明完变量后，我们在变量当前存在但是不可用的作用域中解析变量的初始化表达式。
    // 一旦初始化表达式完成，变量也就绪了。我们通过define来实现。
    private void define(Token name) {
        if (scopes.isEmpty()) return;
        scopes.peek().put(name.lexeme, true);
    }

    private void beginScope() {
        scopes.push(new HashMap<String, Boolean>());
    }

    private void resolve(Stmt stmt) {
        stmt.accept(this);
    }

    private void resolve(Expr expr) {
        expr.accept(this);
    }

    void resolve(List<Stmt> statements) {
        for (Stmt statement : statements) {
            resolve(statement);
        }
    }

    private void endScope() {
        scopes.pop();
    }

    private enum FunctionType {
        NONE,
        FUNCTION,
        METHOD
    }

    private enum ClassType {
        NONE,
        CLASS
    }

}
