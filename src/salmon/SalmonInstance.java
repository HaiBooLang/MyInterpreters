package salmon;

public class SalmonInstance {
    private SalmonClass klass;

    SalmonInstance(SalmonClass klass) {
        this.klass = klass;
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }
}
