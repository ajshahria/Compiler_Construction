"""HTTP API in front of the three compilers in backend/.

Run from this folder:  uvicorn main:app --reload --port 8000
"""

import re
import subprocess
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from compilers import ALL_TABS, COMPILERS, ROOT, Compiler

TIMEOUT_SECONDS = 10
MAX_SOURCE_BYTES = 200_000

app = FastAPI(title="Compiler explorer")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# "line 3: semantic error: undeclared variable 'y'"          (C, Java)
ERROR_LINE_FIRST = re.compile(r"^line (\d+):\s*(\w+) error:\s*(.*)$", re.I)
# "Syntax error [line 7]: statement outside of main(): ..."  (C++)
ERROR_KIND_FIRST = re.compile(r"^(\w+) error \[line (\d+)\]:\s*(.*)$", re.I)
SUMMARY = re.compile(r"^compilation failed", re.I)
SECTION_HEADER = re.compile(r"^=== .* ===\s*$")


class CompileRequest(BaseModel):
    language: str
    source: str = Field(max_length=MAX_SOURCE_BYTES)
    options: list[str] = []
    stdin: str = ""


def get_compiler(language: str) -> Compiler:
    compiler = COMPILERS.get(language)
    if compiler is None:
        raise HTTPException(404, f"Unknown language '{language}'")
    return compiler


def parse_errors(stderr: str) -> tuple[list[dict], str]:
    errors, summary = [], ""
    for raw in stderr.splitlines():
        line = raw.strip()
        if not line:
            continue
        if m := ERROR_LINE_FIRST.match(line):
            errors.append({"line": int(m[1]), "kind": m[2].lower(), "message": m[3]})
        elif m := ERROR_KIND_FIRST.match(line):
            errors.append({"line": int(m[2]), "kind": m[1].lower(), "message": m[3]})
        elif SUMMARY.match(line):
            summary = line
        else:
            errors.append({"line": None, "kind": "error", "message": line})
    return errors, summary


def strip_section_header(text: str) -> str:
    lines = text.splitlines()
    if lines and SECTION_HEADER.match(lines[0]):
        lines = lines[1:]
    return "\n".join(lines).strip("\n")


def run_phase(compiler: Compiler, flag: str, source_path: Path, options: list[str], stdin: str) -> dict:
    try:
        result = subprocess.run(
            [str(compiler.exe), *options, flag, str(source_path)],
            input=stdin,
            capture_output=True,
            text=True,
            timeout=TIMEOUT_SECONDS,
            cwd=source_path.parent,
        )
    except subprocess.TimeoutExpired:
        return {"stdout": "", "stderr": f"Timed out after {TIMEOUT_SECONDS} s", "code": -1}
    return {"stdout": result.stdout, "stderr": result.stderr, "code": result.returncode}


@app.get("/api/languages")
def languages():
    return {
        "tabs": [{"id": tab_id, "label": label} for tab_id, label in ALL_TABS],
        "languages": [
            {
                "id": c.id,
                "name": c.name,
                "extension": c.extension,
                "monaco": c.monaco,
                "tabs": [p[0] for p in c.phases] + ["errors"],
                "options": c.allowed_options,
                "built": c.exe.exists(),
                "defaultExample": c.default_example,
            }
            for c in COMPILERS.values()
        ],
    }


@app.get("/api/examples/{language}")
def examples(language: str):
    compiler = get_compiler(language)
    found = []
    for top in ("examples", "tests"):
        base = compiler.folder / top
        if not base.is_dir():
            continue
        for path in sorted(base.rglob(f"*{compiler.extension}")):
            rel = path.relative_to(compiler.folder).as_posix()
            found.append({"path": rel, "group": path.parent.relative_to(compiler.folder).as_posix(), "name": path.stem})
    return found


@app.get("/api/examples/{language}/file")
def example_file(language: str, path: str):
    compiler = get_compiler(language)
    target = (compiler.folder / path).resolve()
    if not target.is_relative_to(compiler.folder.resolve()) or target.suffix != compiler.extension or not target.is_file():
        raise HTTPException(404, "Example not found")
    return {"path": path, "source": target.read_text(encoding="utf-8", errors="replace")}


@app.post("/api/compile")
def compile_source(req: CompileRequest):
    compiler = get_compiler(req.language)
    if not compiler.exe.exists():
        raise HTTPException(503, f"{compiler.exe.relative_to(ROOT)} is missing: run `make` in {compiler.folder.relative_to(ROOT)}")
    bad = [o for o in req.options if o not in compiler.allowed_options]
    if bad:
        raise HTTPException(400, f"Option(s) not allowed for {compiler.name}: {' '.join(bad)}")

    started = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="ce_") as tmp:
        source_path = Path(tmp) / f"source{compiler.extension}"
        source_path.write_text(req.source, encoding="utf-8")
        with ThreadPoolExecutor(max_workers=len(compiler.phases)) as pool:
            futures = {
                tab_id: pool.submit(run_phase, compiler, flag, source_path, req.options, req.stdin)
                for tab_id, _, flag in compiler.phases
            }
            results = {tab_id: f.result() for tab_id, f in futures.items()}

    phases, errors, seen, summary, exit_code = {}, [], set(), "", 0
    for tab_id, r in results.items():
        phases[tab_id] = strip_section_header(r["stdout"])
        phase_errors, phase_summary = parse_errors(r["stderr"])
        summary = summary or phase_summary
        exit_code = max(exit_code, r["code"]) if r["code"] >= 0 else r["code"]
        for e in phase_errors:
            key = (e["line"], e["kind"], e["message"])
            if key not in seen:
                seen.add(key)
                errors.append(e)
    errors.sort(key=lambda e: (e["line"] is None, e["line"] or 0))

    return {
        "language": compiler.id,
        "phases": phases,
        "errors": errors,
        "summary": summary,
        "exitCode": exit_code,
        "ms": round((time.perf_counter() - started) * 1000),
    }


# Serve the built frontend (frontend/dist) when it exists, so one process is enough.
DIST = ROOT / "frontend" / "dist"
if DIST.is_dir():
    app.mount("/assets", StaticFiles(directory=DIST / "assets"), name="assets")

    @app.get("/{full_path:path}")
    def spa(full_path: str):
        candidate = (DIST / full_path).resolve()
        if full_path and candidate.is_file() and candidate.is_relative_to(DIST.resolve()):
            return FileResponse(candidate)
        return FileResponse(DIST / "index.html")
