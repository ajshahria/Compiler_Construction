"""Table of the three compilers in backend/ and how to call them."""

import os
import shutil
import tempfile
import threading
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BACKEND = ROOT / "backend"
# Static Linux builds made by scripts/build-linux-binaries.sh, used on Vercel.
LINUX_BIN = ROOT / "server" / "bin" / "linux"


def find_exe(folder: Path, windows_name: str, linux_name: str) -> Path:
    """The compiler binary for this OS: build/<name>.exe on Windows; on Linux
    the prebuilt static binary if present, else the compiler's own build/."""
    if os.name == "nt":
        return folder / "build" / windows_name
    prebuilt = LINUX_BIN / linux_name
    return prebuilt if prebuilt.exists() else folder / "build" / linux_name


_copy_lock = threading.Lock()


def runnable(exe: Path) -> Path:
    """A path to `exe` that can be executed. Deploy bundles may drop the
    executable bit and be read-only, so a copy in the temp folder is made
    executable when needed. The phases run in parallel threads, so the copy
    is written under a temporary name, made executable, then renamed into
    place: no thread can see a half-written copy."""
    if os.name == "nt" or os.access(exe, os.X_OK):
        return exe
    copy = Path(tempfile.gettempdir()) / "compiler-bin" / exe.name
    with _copy_lock:
        if not os.access(copy, os.X_OK):
            copy.parent.mkdir(parents=True, exist_ok=True)
            partial = copy.with_name(f"{copy.name}.{os.getpid()}.partial")
            shutil.copyfile(exe, partial)
            partial.chmod(0o755)
            os.replace(partial, copy)
    return copy


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
        exe=find_exe(BACKEND / "c-compiler", "tricora.exe", "tricora"),
        extension=".tri",
        monaco="c",
        phases=[TOKENS, AST, SYMTAB, TAC],
        default_example="examples/valid/manual_sample.tri",
    ),
    "cpp": Compiler(
        id="cpp",
        name="compiler (C++)",
        folder=BACKEND / "c++_compiler",
        exe=find_exe(BACKEND / "c++_compiler", "compiler.exe", "compiler"),
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
        exe=find_exe(BACKEND / "javacompiler", "minijavac.exe", "minijavac"),
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
