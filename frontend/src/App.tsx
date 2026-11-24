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
import { OutputPane, type AstView, type OptView } from "./OutputPane";
import { EXAMPLES, type Example } from "./examples";
import { Pipeline } from "./Pipeline";

/** Which compiler the code is written for, from features only one language has
 *  (comments and strings ignored), or null for the manual's plain form, which
 *  every compiler accepts. */
function detectLanguage(source: string): string | null {
  const code = source
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/.*$/gm, " ")
    .replace(/"(?:\\.|[^"\\\n])*"/g, '""');
  if (/\bclass\s+\w+\s*\{|System\.out\.print|public\s+static\s+void\s+main|\bimport\s+java\./.test(code)) return "java";
  if (/\bcout\b|\bcin\b|using\s+namespace|std::|#include\s*<\s*(iostream|vector|string|cmath|cstdlib|cstdio|algorithm|bits\/stdc\+\+\.h)\s*>/.test(code))
    return "cpp";
  if (/#include\s*<\s*\w+\.h\s*>|\bprintf\s*\(|\bscanf\s*\(/.test(code)) return "c";
  return null;
}

const LANGUAGE_LABELS: Record<string, string> = { c: "C", cpp: "C++", java: "Java" };

type Theme = "dark" | "light";
type MobilePane = "code" | "output";

const PHONE_QUERY = "(max-width: 800px)";

/** True while the window is phone-sized (the layout then shows one pane at a time). */
function usePhoneLayout(): boolean {
  const [phone, setPhone] = useState(() => window.matchMedia(PHONE_QUERY).matches);
  useEffect(() => {
    const mq = window.matchMedia(PHONE_QUERY);
    const update = () => setPhone(mq.matches);
    mq.addEventListener("change", update);
    return () => mq.removeEventListener("change", update);
  }, []);
  return phone;
}
const MIN_FONT = 11;
const MAX_FONT = 26;

/** True if the source reads input (ignoring comments and string literals):
 *  `cin >>` (C++), `scanf(` (C) or a Scanner's `.nextInt()` etc. (Java). */
function readsInput(source: string): boolean {
  const code = source
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/.*$/gm, " ")
    .replace(/"(?:\\.|[^"\\\n])*"/g, '""');
  return /\bcin\s*>>|\bscanf\s*\(|\.next(Int|Float|Double|Boolean)\s*\(/.test(code);
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
  // Projector mode: light theme and a bigger font for the code and every output view.
  const [theme, setTheme] = useState<Theme>(() => load("ce.theme", "dark"));
  const [fontSize, setFontSize] = useState<number>(() => load("ce.fontSize", 14));
  const [astView, setAstView] = useState<AstView>(() => load("ce.astView", "tree"));
  const [optView, setOptView] = useState<OptView>(() => load("ce.optView", "optimized"));
  // On a phone only one pane is shown: the code or the output.
  const phone = usePhoneLayout();
  const [mobilePane, setMobilePane] = useState<MobilePane>("code");
  const phoneRef = useRef(phone);
  phoneRef.current = phone;
  // "lang:detected" pairs the user chose to keep (the notice stays hidden for them)
  const [keptHere, setKeptHere] = useState<string[]>([]);

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
  useEffect(() => save("ce.fontSize", fontSize), [fontSize]);
  useEffect(() => save("ce.astView", astView), [astView]);
  useEffect(() => save("ce.optView", optView), [optView]);
  useEffect(() => {
    save("ce.theme", theme);
    document.documentElement.dataset.theme = theme;
  }, [theme]);
  const editorTheme = theme === "light" ? "explorer-light" : "explorer-dark";

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

  // Compiles the editor's code, or `override` (a just-loaded example, before
  // the editor state has caught up).
  const runCompile = useCallback((override?: { source: string; stdin: string }) => {
    const code = override?.source ?? source;
    const input = override?.stdin ?? stdin;
    if (!language || code === undefined) return;
    const reads = !!language.input && readsInput(code);
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setBusy(true);
    setShowInput(reads);
    if (phoneRef.current) setMobilePane("output");   // on a phone, show the result
    compile({ language: language.id, source: code, options: [], stdin: reads ? input : "" }, ctrl.signal)
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
  }, [language, source, stdin]);

  compileRef.current = runCompile;

  // An example replaces the code (the editor keeps it on its undo stack, so
  // Ctrl+Z brings the old code back), fills the program input it needs and
  // selects the tab that demonstrates it. It does not compile: the old output
  // is cleared and nothing runs until Compile or Ctrl+Enter.
  const loadExample = (ex: Example) => {
    if (!language) return;
    const editor = editorRef.current;
    const model = editor?.getModel();
    if (editor && model) {
      editor.pushUndoStop();
      editor.executeEdits("examples", [{ range: model.getFullModelRange(), text: ex.source }]);
      editor.pushUndoStop();
      editor.setPosition({ lineNumber: 1, column: 1 });
      editor.revealLine(1);
    }
    setSources((all) => ({ ...all, [language.id]: ex.source }));
    if (ex.stdin !== undefined) setStdin(ex.stdin);
    setTab(ex.tab);
    clearOutput();
  };

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

  // The code looks like another language (e.g. C code with Java selected).
  const detected = source !== undefined ? detectLanguage(source) : null;
  const mismatch =
    language && detected && detected !== language.id && meta?.languages.some((l) => l.id === detected)
    && !keptHere.includes(`${language.id}:${detected}`)
      ? detected
      : null;

  // Selects the other language, then (once its editor shows that language's
  // own code) replaces the text with this code as one edit, so a single
  // Ctrl+Z there brings that language's old code back.
  const pendingMove = useRef<string | null>(null);
  const moveCodeTo = (target: string) => {
    if (source === undefined || !meta?.languages.some((l) => l.id === target)) return;
    pendingMove.current = source;
    setLang(target);
  };
  useEffect(() => {
    if (pendingMove.current === null || !language) return;
    // The editor switches to the new language's file in its own effects, so
    // wait (a few short ticks at most) until it really shows that file.
    let timer = 0;
    let tries = 0;
    const apply = () => {
      const code = pendingMove.current;
      const editor = editorRef.current;
      const model = editor?.getModel();
      if (code === null || !editor || !model) return;
      if (!model.uri.path.endsWith(`source-${language.id}${language.extension}`) && tries++ < 20) {
        timer = window.setTimeout(apply, 15);
        return;
      }
      pendingMove.current = null;
      editor.pushUndoStop();
      editor.executeEdits("move-language", [{ range: model.getFullModelRange(), text: code }]);
      editor.pushUndoStop();
      // store it under the new language here too: the editor's change event may
      // still be wired to the previous language for this one edit
      setSources((all) => ({ ...all, [language.id]: code }));
    };
    timer = window.setTimeout(apply, 0);
    return () => window.clearTimeout(timer);
  }, [language?.id]);   // eslint-disable-line react-hooks/exhaustive-deps

  const jump = (e: CompileError) => {
    const ed = editorRef.current;
    if (!ed || e.line == null) return;
    const line = e.line;
    const go = () => {
      ed.layout();
      ed.revealLineInCenter(line);
      ed.setPosition({ lineNumber: line, column: 1 });
      ed.focus();
    };
    if (phoneRef.current && mobilePane !== "code") {
      setMobilePane("code");          // the editor is hidden on a phone: show it first
      window.setTimeout(go, 50);
    } else {
      go();
    }
  };

  // A pipeline step opens its tab (and, on a phone, the output pane).
  const openTab = (t: TabId) => {
    setTab(t);
    if (phoneRef.current) setMobilePane("output");
  };
  const errorCount = result?.errors.length ?? 0;


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
        {language && (
          <Pipeline result={result} busy={busy} failure={failure} available={language.tabs} onOpen={openTab} />
        )}
        <div className="view-controls" aria-label="View">
          <button className="view-btn" onClick={() => setFontSize((f) => Math.max(MIN_FONT, f - 1))}
                  disabled={fontSize <= MIN_FONT} title="Smaller text" aria-label="Smaller text">A−</button>
          <span className="font-size small" title="Code font size">{fontSize}</span>
          <button className="view-btn" onClick={() => setFontSize((f) => Math.min(MAX_FONT, f + 1))}
                  disabled={fontSize >= MAX_FONT} title="Bigger text (for a projector)" aria-label="Bigger text">A+</button>
          <button className="view-btn" onClick={() => setTheme((t) => (t === "dark" ? "light" : "dark"))}
                  title={theme === "dark" ? "Light theme (easier to read on a projector)" : "Dark theme"}
                  aria-label={theme === "dark" ? "Switch to the light theme" : "Switch to the dark theme"}>
            {theme === "dark" ? "☀" : "☾"}
            <span className="theme-label">{theme === "dark" ? " Light" : " Dark"}</span>
          </button>
        </div>
      </header>

      {metaError ? (
        <div className="fatal">
          Can't reach the compiler server ({metaError}). Start it with <code>uvicorn main:app --port 8000</code> in{" "}
          <code>server/</code>.
        </div>
      ) : !meta || !language ? (
        <div className="fatal muted">Loading…</div>
      ) : (
        <>
        <main
          className={phone ? `main phone show-${mobilePane}` : "main"}
          style={phone ? undefined : { gridTemplateColumns: `${split}% 6px 1fr` }}
        >
          <EditorPane
            languages={meta.languages}
            language={language}
            onLanguage={setLang}
            source={source ?? ""}
            onSource={(s) => setSources((all) => ({ ...all, [language.id]: s }))}
            onMount={onMount}
            onCompile={() => runCompile()}
            busy={busy}
            examples={EXAMPLES[language.id] ?? []}
            onExample={loadExample}
            notice={
              mismatch
                ? {
                    label: LANGUAGE_LABELS[mismatch] ?? mismatch,
                    onSwitch: () => moveCodeTo(mismatch),
                    onDismiss: () => setKeptHere((k) => [...k, `${language.id}:${mismatch}`]),
                  }
                : null
            }
            editorTheme={editorTheme}
            fontSize={fontSize}
            phone={phone}
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
            editorTheme={editorTheme}
            fontSize={fontSize}
            astView={astView}
            onAstView={setAstView}
            optView={optView}
            onOptView={setOptView}
            onCompile={() => runCompile()}
            phone={phone}
            visible={!phone || mobilePane === "output"}
          />
        </main>
        {phone && (
          <nav className="mobile-nav" aria-label="Pane">
            <button className={mobilePane === "code" ? "on" : ""} onClick={() => setMobilePane("code")}
                    aria-pressed={mobilePane === "code"}>
              <span aria-hidden="true">{"</>"}</span> Code
            </button>
            <button className="btn mobile-compile compile-btn" onClick={() => runCompile()} disabled={busy}>
              {busy ? "Compiling…" : "▶ Compile"}
            </button>
            <button className={mobilePane === "output" ? "on" : ""} onClick={() => setMobilePane("output")}
                    aria-pressed={mobilePane === "output"}>
              Output
              {errorCount > 0 && <span className="badge">{errorCount}</span>}
            </button>
          </nav>
        )}
        </>
      )}
    </div>
  );
}
