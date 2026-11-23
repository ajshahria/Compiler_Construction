// An unknown package, a malformed import and an import after the class are
// all lexical errors, reported with their line numbers.
import com.example.Tools;
import java.util.;

class BadImport {
    public static void main(String[] args) {
        int x;
        x = 1;
        System.out.println(x);
    }
}

import java.io.*;
