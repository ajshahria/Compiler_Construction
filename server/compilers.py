"""Table of the three compilers in backend/ and how to call them."""

from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BACKEND = ROOT / "backend"


@dataclass(frozen=True)
class Compiler:
    id: str
    name: str
    folder: Path
    exe: Path
    extension: str
    monaco: str
    # (tab id, tab label, command-line flag); the order is the tab order
    phases: list[tuple[str, str, str]]
    # extra flags a user may pass in the "Compiler options" box
    allowed_options: list[str] = field(default_factory=list)
    # source file to show when the language is first picked
    default_example: str = ""


TOKENS = ("tokens", "Tokens", "--tokens")
AST = ("ast", "AST", "--ast")
SYMTAB = ("symtab", "Symbol Table", "--symtab")
TAC = ("tac", "TAC", "--tac")

COMPILERS: dict[str, Compiler] = {
    "c": Compiler(
        id="c",
        name="tricora (C)",
        folder=BACKEND / "c-compiler",
        exe=BACKEND / "c-compiler" / "build" / "tricora.exe",
        extension=".tri",
        monaco="c",
        phases=[TOKENS, AST, SYMTAB, TAC],
        default_example="examples/valid/manual_sample.tri",
    ),
    "cpp": Compiler(
        id="cpp",
        name="compiler (C++)",
        folder=BACKEND / "c++_compiler",
        exe=BACKEND / "c++_compiler" / "build" / "compiler.exe",
        extension=".cpp",
        monaco="cpp",
        phases=[
            ("headers", "Headers", "--headers"),
            TOKENS,
            AST,
            SYMTAB,
            TAC,
            ("opt", "Optimized TAC", "--opt"),
            ("asm", "Assembly", "--asm"),
            ("run", "Output", "--run"),
        ],
        allowed_options=["--manual", "--allow-no-header", "--allow-no-main"],
        default_example="examples/headers_demo.cpp",
    ),
    "java": Compiler(
        id="java",
        name="minijavac (Java)",
        folder=BACKEND / "javacompiler",
        exe=BACKEND / "javacompiler" / "build" / "minijavac.exe",
        extension=".java",
        monaco="java",
        phases=[TOKENS, AST, SYMTAB, TAC],
        allowed_options=["--manual"],
        default_example="examples/Example.java",
    ),
}

# Every tab the UI can show, in order. "errors" is always available.
ALL_TABS = [
    ("headers", "Headers"),
    ("tokens", "Tokens"),
    ("ast", "AST"),
    ("symtab", "Symbol Table"),
    ("tac", "TAC"),
    ("opt", "Optimized TAC"),
    ("asm", "Assembly"),
    ("run", "Output"),
    ("errors", "Errors"),
]
