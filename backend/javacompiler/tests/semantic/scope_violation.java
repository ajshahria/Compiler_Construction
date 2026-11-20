class ScopeViolation {
    public static void main(String[] args) {
        {
            int local = 3;
        }
        print local;
    }
}
