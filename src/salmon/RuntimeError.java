package salmon;

// 与Java转换异常不同，我们的类会跟踪语法标记，可以指明用户代码中抛出运行时错误的位置。
class RuntimeError extends RuntimeException {
    final Token token;

    RuntimeError(Token token, String message) {
        super(message);
        this.token = token;
    }
}