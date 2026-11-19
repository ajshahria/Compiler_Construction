"""Runs several programs through build/compiler.exe and writes build/demo.html
(one card per program, one tab per compiler phase). Run from backend/c++_compiler:
    python examples/demo_programs.py
"""
import html
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "build", "compiler.exe")

PROGRAMS = [
    ("1. Sum loop (manual sample)", "valid", """int x;
int y;
bool flag;
x = 10;
y = 0;
flag = true;
while (x > 0) {
    y = y + x;
    x = x - 1;
}
if (flag == true) { print y; } else { print x; }
"""),
    ("2. Floats and mixed int/float", "valid", """int n = 3;
float f = 2.5;
float g = f * n + 1;
bool big = g > 8.0;
print g;
print big;
print -f;
"""),
    ("3. Nested scopes and shadowing", "valid", """int n = 1;
{
    int n = 7;
    {
        int n = 9;
        print n;
    }
    print n;
}
print n;
"""),
    ("4. C++ style: headers, for, cout", "valid", """#include <iostream>
#include <vector>
using namespace std;

int fact = 1;
for (int i = 1; i <= 5; i++) {
    fact *= i;
}
cout << fact << endl;
"""),
    ("5. Dangling else + logic", "valid", """int i = 0;
int evens = 0;
while (i < 10) {
    if (i % 2 == 0)
        if (i > 4) evens = evens + 10; else evens = evens + 1;
    i = i + 1;
}
print evens;
print evens > 5 && !(evens == 0);
"""),
    ("5b. Typical C++ main program", "valid", """#include <iostream>
using namespace std;

int main() {
    int a = 7, b = 3;
    cout << "a + b = " << a + b << endl;
    for (int i = 1; i <= 6; i++) {
        if (i == 2) continue;
        if (i == 5) break;
        cout << i << " ";
    }
    cout << endl;
    int n = 4, fact = 1;
    do { fact *= n; n--; } while (n > 0);
    cout << "4! = " << fact << endl;
    switch (a % 3) {
        case 0: cout << "zero"; break;
        case 1: cout << "one"; break;
        default: cout << "two";
    }
    cout << endl << "max = " << (a > b ? a : b) << endl;
    return 0;
}
"""),
    ("6. Lexical errors", "error", """#include <notaheader>
int x;
x = 5 @ 3;
int 2abc;
"""),
    ("6b. Missing header", "error", """int x = 1;
print x;
"""),
    ("6c. Include after code", "error", """#include <iostream>
int x = 1;
#include <vector>
print x;
"""),
    ("6d. Missing main()", "nomain", """#include <iostream>
int x = 1;
cout << x;
"""),
    ("7. Syntax errors", "error", """int x;
x = 5
print x;
while (x < 3 {
    x = x + 1;
}
"""),
    ("8. Semantic errors", "error", """int a;
bool b = 5 + 3.2;
a = c + 1;
a = true;
{ int inner = 1; }
a = inner;
int a;
if (a) { print a; }
"""),
]

PHASES = [("Headers", "--headers"), ("Tokens", "--tokens"), ("AST", "--ast"),
          ("Symbols", "--symtab"), ("TAC", "--tac"), ("Optimized", "--opt"),
          ("Assembly", "--asm"), ("Output", "--run")]


def wrap_in_main(src):
    """Put everything after the leading #include/using lines inside 'int main() { ... }'."""
    head, body = [], []
    for line in src.splitlines():
        if not body and (line.startswith("#include") or line.startswith("using namespace") or not line.strip()):
            head.append(line)
        else:
            body.append(line)
    nl = chr(10)
    return nl.join(head + ["int main() {"] + ["    " + l for l in body] + ["    return 0;", "}"]) + nl


def run(flag, path):
    r = subprocess.run([EXE, flag, path], input="", capture_output=True, text=True, timeout=20)
    out = "\n".join(l for l in r.stdout.splitlines() if not l.startswith("==="))
    return out, r.stderr.strip(), r.returncode


def main():
    tmp = os.path.join(ROOT, "build", "_demo.cpp")
    cards = []
    for title, kind, src in PROGRAMS:
        if "6b." not in title and "include <iostream>" not in src:
            src = "#include <iostream>" + chr(10) + src   # every program needs <iostream> at the top
        if kind != "nomain" and "int main" not in src:
            src = wrap_in_main(src)          # every program needs int main()
        with open(tmp, "w") as f:
            f.write(src)
        tabs, errors, status = [], "", "ok"
        for name, flag in PHASES:
            out, err, code = run(flag, tmp)
            if code != 0:
                status = "error"
                errors = errors or err
            tabs.append((name, out.strip() or "(nothing)"))
        tabs.append(("Errors", errors or "No errors."))
        cards.append({"title": title, "status": status, "src": src, "tabs": tabs})
    os.remove(tmp)

    page = TEMPLATE.replace("__DATA__", json.dumps(cards))
    out_path = os.path.join(ROOT, "build", "demo.html")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(page)
    print("wrote", out_path)


TEMPLATE = """<!doctype html><html><head><meta charset="utf-8"><title>Tricora Compiler - C++ version Demo</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--ink:#1c2330;--mut:#667085;--line:#e3e6eb;--acc:#2f6feb;--ok:#18794e;--bad:#c0352b;--code:#0f172a}
@media (prefers-color-scheme:dark){:root{--bg:#0f1319;--card:#171d26;--ink:#e6e9ef;--mut:#98a2b3;--line:#2a3340;--acc:#6ea0ff;--ok:#4cc38a;--bad:#ff7b72;--code:#0b0f14}}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.45 system-ui,Segoe UI,sans-serif}
header{padding:18px 20px;border-bottom:1px solid var(--line)}h1{margin:0;font-size:18px}
header p{margin:4px 0 0;color:var(--mut)}
main{max-width:1100px;margin:0 auto;padding:16px;display:grid;gap:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;overflow:hidden}
.top{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;border-bottom:1px solid var(--line)}
.top b{font-size:14px}.badge{font-size:12px;padding:2px 9px;border-radius:99px;border:1px solid}
.ok{color:var(--ok)}.error{color:var(--bad)}
.body{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1.2fr)}
@media(max-width:760px){.body{grid-template-columns:1fr}}
pre{margin:0;padding:12px 14px;font:12.5px/1.5 Consolas,monospace;background:var(--code);color:#d7e0ee;overflow:auto;height:300px;white-space:pre}
.tabs{display:flex;flex-wrap:wrap;gap:2px;padding:6px 8px 0;border-bottom:1px solid var(--line)}
.tab{border:0;background:none;color:var(--mut);padding:6px 10px;border-radius:6px 6px 0 0;cursor:pointer;font:inherit}
.tab.on{color:var(--acc);box-shadow:inset 0 -2px var(--acc)}
.lbl{padding:6px 14px;color:var(--mut);font-size:12px;border-bottom:1px solid var(--line)}
</style></head><body>
<header><h1>Tricora Compiler - C++ version - live results</h1>
<p>Each program was run through the real compiler. Pick a tab to see that phase.</p></header>
<main id="m"></main>
<script>
const data=__DATA__;
const m=document.getElementById('m');
data.forEach(c=>{
  const d=document.createElement('div');d.className='card';
  d.innerHTML=`<div class="top"><b></b><span class="badge ${c.status}">${c.status=='ok'?'compiled':'errors reported'}</span></div>
  <div class="body"><div><div class="lbl">Source</div><pre class="src"></pre></div>
  <div><div class="tabs"></div><pre class="out"></pre></div></div>`;
  d.querySelector('b').textContent=c.title;
  d.querySelector('.src').textContent=c.src;
  const tabs=d.querySelector('.tabs'),out=d.querySelector('.out');
  const show=(i)=>{[...tabs.children].forEach((t,j)=>t.classList.toggle('on',i==j));out.textContent=c.tabs[i][1]};
  c.tabs.forEach((t,i)=>{const b=document.createElement('button');b.className='tab';b.textContent=t[0];b.onclick=()=>show(i);tabs.appendChild(b)});
  show(c.status=='ok'?c.tabs.findIndex(t=>t[0]=='Output'):c.tabs.length-1);
  m.appendChild(d);
});
</script></body></html>"""

if __name__ == "__main__":
    main()
