import { useCallback, useEffect, useRef, useState } from "react";
import type { OnMount } from "@monaco-editor/react";
import { monaco } from "./monacoSetup";
import {
  compile,
  getExample,
  getExamples,
  getLanguages,
  type CompileError,
  type CompileResult,
  type Example,
  type LanguagesResponse,
  type TabId,
} from "./api";
import { EditorPane } from "./EditorPane";
import { OutputPane } from "./OutputPane";

type IEditor = Parameters<OnMount>[0];

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
  const [options, setOptions] = useState<Record<string, string[]>>(() => load("ce.options", {}));
  const [stdin, setStdin] = useState<string>(() => load("ce.stdin", ""));
  const [auto, setAuto] = useState<boolean>(() => load("ce.auto", true));
  const [tab, setTab] = useState<TabId>("run");   // the Output tab is shown first
  const [split, setSplit] = useState<number>(() => load("ce.split", 50));
  const [examples, setExamples] = useState<Example[]>([]);
  const [result, setResult] = useState<CompileResult | null>(null);
  const [failure, setFailure] = useState("");
  const [busy, setBusy] = useState(false);

  const editorRef = useRef<IEditor | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const compileRef = useRef<() => void>(() => {});

  const language = meta?.languages.find((l) => l.id === lang) ?? meta?.languages[0];
  const source = language ? sources[language.id] : undefined;
  const langOptions = language ? options[language.id] ?? [] : [];

  useEffect(() => {
    getLanguages()
      .then(setMeta)
      .catch((e) => setMetaError(String(e.message ?? e)));
  }, []);

  useEffect(() => save("ce.lang", lang), [lang]);
  useEffect(() => save("ce.sources", sources), [sources]);
  useEffect(() => save("ce.options", options), [options]);
  useEffect(() => save("ce.stdin", stdin), [stdin]);
  useEffect(() => save("ce.auto", auto), [auto]);
  useEffect(() => save("ce.split", split), [split]);

  // Examples for the current language, and its default program on first use.
  useEffect(() => {
    if (!language) return;
    setExamples([]);
    getExamples(language.id).then(setExamples).catch(() => setExamples([]));
    if (sources[language.id] === undefined && language.defaultExample) {
      getExample(language.id, language.defaultExample)
        .then((r) => setSources((s) => (s[language.id] === undefined ? { ...s, [language.id]: r.source } : s)))
        .catch(() => setSources((s) => ({ ...s, [language.id]: "" })));
    }
    if (!language.tabs.includes(tab)) setTab(language.tabs.includes("run") ? "run" : "tac");
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [language?.id]);

  const runCompile = useCallback(() => {
    if (!language || source === undefined) return;
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setBusy(true);
    compile({ language: language.id, source, options: langOptions, stdin }, ctrl.signal)
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
  }, [language, source, langOptions, stdin]);

  compileRef.current = runCompile;

  // Compile on change (debounced) and whenever the language or options change.
  useEffect(() => {
    if (!auto || source === undefined) return;
    const t = setTimeout(runCompile, 700);
    return () => clearTimeout(t);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [auto, language?.id, source, JSON.stringify(langOptions), stdin]);

  // Drop results from another language as soon as the toggle moves.
  useEffect(() => {
    setResult(null);
    setFailure("");
  }, [language?.id]);

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
  };

  const jump = (e: CompileError) => {
    const ed = editorRef.current;
    if (!ed || e.line == null) return;
    ed.revealLineInCenter(e.line);
    ed.setPosition({ lineNumber: e.line, column: 1 });
    ed.focus();
  };

  const loadExample = (path: string) => {
    if (!language) return;
    getExample(language.id, path)
      .then((r) => setSources((s) => ({ ...s, [language.id]: r.source })))
      .catch((e) => setFailure(String(e.message ?? e)));
  };

  const toggleOption = (o: string) => {
    if (!language) return;
    setOptions((all) => {
      const cur = all[language.id] ?? [];
      return { ...all, [language.id]: cur.includes(o) ? cur.filter((x) => x !== o) : [...cur, o] };
    });
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
            examples={examples}
            onExample={loadExample}
            onMount={onMount}
            auto={auto}
            onAuto={setAuto}
            onCompile={runCompile}
            busy={busy}
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
            options={langOptions}
            onToggleOption={toggleOption}
            stdin={stdin}
            onStdin={setStdin}
            onJump={jump}
          />
        </main>
      )}
    </div>
  );
}
