import { useCallback, useEffect, useRef, useState } from "react";
import type { OnMount } from "@monaco-editor/react";
import { monaco } from "./monacoSetup";
import {
  compile,
  getLanguages,
  type CompileError,
  type CompileResult,
  type LanguagesResponse,
  type TabId,
} from "./api";
import { EditorPane } from "./EditorPane";
import { OutputPane } from "./OutputPane";

/** True if the source reads input with `cin >>` (ignoring comments and string literals). */
function readsInput(source: string): boolean {
  const code = source
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/.*$/gm, " ")
    .replace(/"(?:\\.|[^"\\\n])*"/g, '""');
  return /\bcin\s*>>/.test(code);
}

type IEditor = Parameters<OnMount>[0];

/* The browser's "Save as" dialog (File System Access API, Chrome and Edge). */
type SaveFilePicker = (options: { suggestedName: string }) => Promise<{
  name: string;
  createWritable: () => Promise<{ write: (data: Blob) => Promise<void>; close: () => Promise<void> }>;
}>;

const MAX_FILE_BYTES = 200_000;   // the server's limit on a source file

/* Which compiler a file opens in, by its extension. */
const EXTENSION_LANGUAGE: Record<string, string> = {
  ".tri": "c", ".c": "c",
  ".cpp": "cpp", ".cc": "cpp", ".cxx": "cpp", ".h": "cpp", ".hpp": "cpp",
  ".java": "java",
};
const OPEN_TYPES = ".tri,.c,.cpp,.cc,.cxx,.h,.hpp,.java,.txt";

function load<T>(key: string, fallback: T): T {
  try {
    const raw = localStorage.getItem(key);
    return raw == null ? fallback : (JSON.parse(raw) as T);
  } catch {
    return fallback;
  }
}

function save(key: string, value: unknown) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    /* storage unavailable: keep working without it */
  }
}

export default function App() {
  const [meta, setMeta] = useState<LanguagesResponse | null>(null);
  const [metaError, setMetaError] = useState("");
  const [lang, setLang] = useState<string>(() => load("ce.lang", "cpp"));
  const [sources, setSources] = useState<Record<string, string>>(() => load("ce.sources", {}));
  const [stdin, setStdin] = useState<string>(() => load("ce.stdin", ""));
  const [tab, setTab] = useState<TabId>("run");   // the Output tab is shown first
  const [split, setSplit] = useState<number>(() => load("ce.split", 50));
  const [result, setResult] = useState<CompileResult | null>(null);
  const [failure, setFailure] = useState("");
  const [busy, setBusy] = useState(false);
  // (a new storage key, so names saved when C files were called .tri start fresh)
  const [fileNames, setFileNames] = useState<Record<string, string>>(() => load("ce.fileNames", {}));

  const editorRef = useRef<IEditor | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const compileRef = useRef<() => void>(() => {});
  const saveFileRef = useRef<() => void>(() => {});

  const language = meta?.languages.find((l) => l.id === lang) ?? meta?.languages[0];
  const source = language ? sources[language.id] : undefined;
  const fileName = language ? fileNames[language.id] ?? `source${language.extension}` : "";

  useEffect(() => {
    getLanguages()
      .then(setMeta)
      .catch((e) => setMetaError(String(e.message ?? e)));
  }, []);

  useEffect(() => save("ce.lang", lang), [lang]);
  useEffect(() => save("ce.sources", sources), [sources]);
  useEffect(() => save("ce.stdin", stdin), [stdin]);

  // The program reads input with `cin >>`: only then is the input text sent.
  const needsInput = !!language?.input && source !== undefined && readsInput(source);
  // The input box appears only after Compile, and only if the compiled code reads input.
  const [showInput, setShowInput] = useState(false);
  useEffect(() => save("ce.split", split), [split]);
  useEffect(() => save("ce.fileNames", fileNames), [fileNames]);

  // The starter program the first time a language is picked.
  useEffect(() => {
    if (!language) return;
    if (sources[language.id] === undefined)
      setSources((s) => (s[language.id] === undefined ? { ...s, [language.id]: language.starter } : s));
    if (!language.tabs.includes(tab)) setTab(language.tabs.includes("run") ? "run" : "tac");
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [language?.id]);

  const runCompile = useCallback(() => {
    if (!language || source === undefined) return;
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setBusy(true);
    setShowInput(needsInput);
    compile({ language: language.id, source, options: [], stdin: needsInput ? stdin : "" }, ctrl.signal)
      .then((r) => {
        setResult(r);
        setFailure("");
      })
      .catch((e) => {
        if (ctrl.signal.aborted) return;
        setResult(null);
        setFailure(String(e.message ?? e));
      })
      .finally(() => {
        if (abortRef.current === ctrl) setBusy(false);
      });
  }, [language, source, stdin, needsInput]);

  compileRef.current = runCompile;

  // Code is compiled only when the user asks (Compile button or Ctrl+Enter).
  // Output that belongs to other code is cleared: when the language changes
  // (a compile still running for the old language is cancelled too).
  const clearOutput = () => {
    abortRef.current?.abort();
    abortRef.current = null;
    setBusy(false);
    setResult(null);
    setFailure("");
    setShowInput(false);
  };

  useEffect(clearOutput, [language?.id]);

  // Red squiggles in the editor for every error with a line number.
  useEffect(() => {
    const model = editorRef.current?.getModel();
    if (!model) return;
    const markers = (result?.errors ?? [])
      .filter((e) => e.line != null)
      .map((e) => {
        const line = Math.min(Math.max(e.line!, 1), model.getLineCount());
        return {
          severity: monaco.MarkerSeverity.Error,
          message: `${e.kind} error: ${e.message}`,
          startLineNumber: line,
          startColumn: model.getLineFirstNonWhitespaceColumn(line) || 1,
          endLineNumber: line,
          endColumn: model.getLineMaxColumn(line),
        };
      });
    monaco.editor.setModelMarkers(model, "compiler", markers);
  }, [result, language?.id]);

  const onMount: OnMount = (editor) => {
    editorRef.current = editor;
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, () => compileRef.current());
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => saveFileRef.current());
  };

  // Ctrl+S outside the editor saves the program too (instead of the browser's "Save page").
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
        e.preventDefault();
        saveFileRef.current();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  const setFileName = (name: string) => {
    if (!language) return;
    // characters that no file name may contain
    setFileNames((all) => ({ ...all, [language.id]: name.replace(/[\\/:*?"<>|]/g, "") }));
  };

  // Opening a file picks the language from its extension and loads it into the editor.
  const openFile = async (file: File) => {
    if (!meta || !language) return;
    if (file.size > MAX_FILE_BYTES) {
      setFailure(`${file.name} is larger than ${MAX_FILE_BYTES / 1000} KB, the most the compiler accepts.`);
      return;
    }
    try {
      const text = await file.text();
      const dot = file.name.lastIndexOf(".");
      const ext = dot >= 0 ? file.name.slice(dot).toLowerCase() : "";
      const id = EXTENSION_LANGUAGE[ext] ?? language.id;
      const target = meta.languages.find((l) => l.id === id) ?? language;
      setSources((s) => ({ ...s, [target.id]: text }));
      setFileNames((all) => ({ ...all, [target.id]: file.name }));
      setLang(target.id);
      clearOutput();
    } catch (e) {
      setFailure(`Couldn't read ${file.name}: ${String((e as Error).message ?? e)}`);
    }
  };

  // Saving asks where to save when the browser supports it (Chrome, Edge),
  // and otherwise downloads the file under its name.
  const saveFile = async () => {
    if (!language) return;
    let name = fileName.trim() || "source";
    if (!/\.[A-Za-z0-9]+$/.test(name)) name += language.extension;
    const blob = new Blob([source ?? ""], { type: "text/plain" });
    const picker = (window as unknown as { showSaveFilePicker?: SaveFilePicker }).showSaveFilePicker;
    if (picker) {
      try {
        const handle = await picker({ suggestedName: name });
        const writable = await handle.createWritable();
        await writable.write(blob);
        await writable.close();
        setFileName(handle.name);
        return;
      } catch (e) {
        if ((e as Error).name === "AbortError") return;   // the user closed the dialog
        // not allowed here (for example inside a frame): fall back to a download
      }
    }
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = name;
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    setFileName(name);
  };
  saveFileRef.current = saveFile;

  const jump = (e: CompileError) => {
    const ed = editorRef.current;
    if (!ed || e.line == null) return;
    ed.revealLineInCenter(e.line);
    ed.setPosition({ lineNumber: e.line, column: 1 });
    ed.focus();
  };


  const startDrag = (e: React.PointerEvent) => {
    const main = (e.currentTarget as HTMLElement).parentElement!;
    const rect = main.getBoundingClientRect();
    const move = (ev: PointerEvent) =>
      setSplit(Math.min(80, Math.max(20, ((ev.clientX - rect.left) / rect.width) * 100)));
    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
      document.body.classList.remove("dragging");
    };
    document.body.classList.add("dragging");
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  };

  return (
    <div className="app">
      <header className="topbar">
        <div className="logo" aria-label="Tricora">
          <img src="/tricora-mark.png" alt="" className="logo-mark" />
          <img src="/tricora-wordmark.png" alt="Tricora" className="logo-word" />
        </div>
        <nav className="menu">
          <span>C · C++ · Java</span>
        </nav>
        <span className="spacer" />
        <span className="muted small">Lexer → Parser → AST → Symbol table → TAC</span>
      </header>

      {metaError ? (
        <div className="fatal">
          Can't reach the compiler server ({metaError}). Start it with <code>uvicorn main:app --port 8000</code> in{" "}
          <code>server/</code>.
        </div>
      ) : !meta || !language ? (
        <div className="fatal muted">Loading…</div>
      ) : (
        <main className="main" style={{ gridTemplateColumns: `${split}% 6px 1fr` }}>
          <EditorPane
            languages={meta.languages}
            language={language}
            onLanguage={setLang}
            source={source ?? ""}
            onSource={(s) => setSources((all) => ({ ...all, [language.id]: s }))}
            onMount={onMount}
            onCompile={runCompile}
            busy={busy}
            fileName={fileName}
            onFileName={setFileName}
            onOpenFile={openFile}
            onSaveFile={saveFile}
            openTypes={OPEN_TYPES}
          />
          <div className="splitter" onPointerDown={startDrag} role="separator" aria-orientation="vertical" />
          <OutputPane
            language={language}
            tabs={meta.tabs}
            tab={tab}
            onTab={setTab}
            result={result}
            failure={failure}
            busy={busy}
            needsInput={showInput}
            stdin={stdin}
            onStdin={setStdin}
            onJump={jump}
          />
        </main>
      )}
    </div>
  );
}
