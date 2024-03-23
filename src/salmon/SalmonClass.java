package salmon;

public class SalmonClass {
    final String name;

    SalmonClass(String name) {
        this.name = name;
    }

    @Override
    public String toString() {
        return name;
    }
}
