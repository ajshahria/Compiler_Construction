// One example of every semantic rule in section 4.5 of the lab manual.
class Semantic {
    public static void main(String[] args) {
        int x = 1;
        int x = 2;                 // redeclaration in the same scope
        boolean flag = 5 + 3.2;    // type mismatch
        y = 10;                    // undeclared variable
        {
            int inner = 1;
        }
        inner = 2;                 // scope violation
        x = true;                  // invalid assignment
        flag = x && 1;             // invalid expression
    }
}
