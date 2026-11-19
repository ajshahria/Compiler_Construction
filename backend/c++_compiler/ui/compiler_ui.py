"""Simple Tkinter front-end for the Tricora Compiler - C++ version.

Type or open a program, press "Compile & Run", and see every compiler phase in its own tab.
It only calls build/compiler.exe with different flags, so build the compiler first (make).
Run:  python ui/compiler_ui.py
"""
import os
import subprocess
import tempfile
import tkinter as tk
from tkinter import filedialog, ttk

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPILER = os.path.join(ROOT, "build", "compiler.exe")

# tab title -> compiler flag
PHASES = [
    ("Headers", "--headers"),
    ("Tokens", "--tokens"),
    ("AST", "--ast"),
    ("Symbol Table", "--symtab"),
    ("TAC", "--tac"),
    ("Optimized TAC", "--opt"),
    ("Assembly", "--asm"),
    ("Output", "--run"),
]

SAMPLE = """#include <iostream>
using namespace std;

int main() {
    int x = 10, y = 0;
    while (x > 0) {
        y = y + x;
        x = x - 1;
    }
    cout << "Sum: " << y << endl;
    return 0;
}
"""


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Tricora Compiler - C++ version")
        self.geometry("1100x650")

        bar = ttk.Frame(self)
        bar.pack(fill="x", padx=6, pady=4)
        ttk.Button(bar, text="Open...", command=self.open_file).pack(side="left")
        ttk.Button(bar, text="Compile & Run", command=self.compile).pack(side="left", padx=6)

        # input for 'cin >> x;' (one box, values separated by spaces or new lines)
        box = ttk.LabelFrame(self, text="Program input (for cin)")
        box.pack(side="bottom", fill="x", padx=6, pady=4)
        self.stdin = tk.Text(box, height=2, font=("Consolas", 10))
        self.stdin.pack(fill="x")

        panes = ttk.PanedWindow(self, orient="horizontal")
        panes.pack(fill="both", expand=True, padx=6, pady=4)

        self.editor = tk.Text(self, font=("Consolas", 11), undo=True)
        self.editor.insert("1.0", SAMPLE)
        panes.add(self.editor, weight=1)

        self.tabs = ttk.Notebook(self)
        panes.add(self.tabs, weight=1)
        self.views = {}
        for title, _ in PHASES + [("Errors", None)]:
            frame = ttk.Frame(self.tabs)
            text = tk.Text(frame, font=("Consolas", 10), state="disabled", wrap="none")
            text.pack(fill="both", expand=True)
            self.tabs.add(frame, text=title)
            self.views[title] = text

    def open_file(self):
        path = filedialog.askopenfilename(filetypes=[("Source", "*.cpp *.txt"), ("All", "*.*")])
        if path:
            with open(path, encoding="utf-8") as f:
                self.editor.delete("1.0", "end")
                self.editor.insert("1.0", f.read())

    def show(self, title, content):
        view = self.views[title]
        view.config(state="normal")
        view.delete("1.0", "end")
        view.insert("1.0", content)
        view.config(state="disabled")

    def compile(self):
        if not os.path.exists(COMPILER):
            self.show("Errors", "build/compiler.exe not found - run 'make' first.")
            self.tabs.select(len(PHASES))
            return
        with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False, encoding="utf-8") as f:
            f.write(self.editor.get("1.0", "end"))
            path = f.name
        errors = ""
        try:
            for title, flag in PHASES:
                try:
                    r = subprocess.run([COMPILER, flag, path], input=self.stdin.get("1.0", "end"),
                                       capture_output=True, text=True, timeout=20)
                except subprocess.TimeoutExpired:
                    errors = errors or "Timed out after 20 s (infinite loop?)"
                    self.show(title, "(timed out)")
                    continue
                out = r.stdout
                # strip the "=== header ===" line the compiler prints
                out = "\n".join(l for l in out.splitlines() if not l.startswith("==="))
                self.show(title, out)
                if r.returncode != 0 and not errors:
                    errors = r.stderr
            self.show("Errors", errors or "No errors.")
            self.tabs.select(len(PHASES) if errors else len(PHASES) - 1)
        finally:
            os.unlink(path)


if __name__ == "__main__":
    App().mainloop()
