"""Table of the three compilers in backend/ and how to call them."""

import os
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BACKEND = ROOT / "backend"


def find_exe(folder: Path, windows_name: str, linux_name: str) -> Path:
    """The binary `make` writes into the compiler's build/ folder. The C and
    Java Makefiles add .exe only on Windows; the C++ one always does."""
    return folder / "build" / (windows_name if os.name == "nt" else linux_name)


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
    # the language can read input (cin, scanf, Scanner), so the Output tab offers an input box
    reads_input: bool = False


TOKENS = ("tokens", "Tokens", "--tokens")
AST = ("ast", "AST", "--ast")
SYMTAB = ("symtab", "Symbol Table", "--symtab")
TAC = ("tac", "TAC", "--tac")
OPT = ("opt", "Optimized TAC", "--opt")
ASM = ("asm", "Assembly", "--asm")
RUN = ("run", "Output", "--run")
HEADERS = ("headers", "Headers", "--headers")

COMPILERS: dict[str, Compiler] = {
    "c": Compiler(
        id="c",
        name="tricora (C)",
        folder=BACKEND / "c-compiler",
        exe=find_exe(BACKEND / "c-compiler", "tricora.exe", "tricora"),
        extension=".c",   # the compiler's own examples and tests use .tri
        monaco="c",
        phases=[HEADERS, TOKENS, AST, SYMTAB, TAC, OPT, ASM, RUN],
        default_example="examples/valid/manual_sample.tri",
        reads_input=True,   # scanf
    ),
    "cpp": Compiler(
        id="cpp",
        name="compiler (C++)",
        folder=BACKEND / "c++_compiler",
        exe=find_exe(BACKEND / "c++_compiler", "compiler.exe", "compiler.exe"),
        extension=".cpp",
        monaco="cpp",
        phases=[
            HEADERS,
            TOKENS,
            AST,
            SYMTAB,
            TAC,
            OPT,
            ASM,
            RUN,
        ],
        allowed_options=["--strict"],
        default_example="examples/headers_demo.cpp",
        reads_input=True,
    ),
    "java": Compiler(
        id="java",
        name="minijavac (Java)",
        folder=BACKEND / "javacompiler",
        exe=find_exe(BACKEND / "javacompiler", "minijavac.exe", "minijavac"),
        extension=".java",
        monaco="java",
        phases=[HEADERS, TOKENS, AST, SYMTAB, TAC, OPT, ASM, RUN],
        allowed_options=["--strict"],
        default_example="examples/Example.java",
        reads_input=True,   # Scanner
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
