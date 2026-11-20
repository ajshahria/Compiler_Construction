// flags: --asm
// x86 assembly of a small loop with int, float and boolean values.
class AsmSmall {
    public static void main(String[] args) {
        int i = 0;
        float sum = 0.5;
        while (i < 3) {
            sum = sum + i;
            i = i + 1;
        }
        boolean done = i == 3;
        System.out.println(sum);
        System.out.println(done);
    }
}
