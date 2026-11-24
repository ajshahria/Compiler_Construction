import { useEffect, useRef, useState } from "react";
import Editor, { type OnMount } from "@monaco-editor/react";
import type { Language } from "./api";
import type { Example } from "./examples";

interface Props {
  fileName: string;
  onFileName: (name: string) => void;
  onOpenFile: (file: File) => void;
  onSaveFile: () => void;
  openTypes: string;   // file types the Open dialog offers, e.g. ".c,.cpp,.java"
  languages: Language[];
  language: Language;
  onLanguage: (id: string) => void;
  source: string;
  onSource: (s: string) => void;
  onMount: OnMount;
  onCompile: () => void;
  busy: boolean;
  examples: Example[];
  onExample: (e: Example) => void;
  editorTheme: string;
  /* The code looks like another language: offer to move it there. */
  notice: { label: string; onSwitch: () => void; onDismiss: () => void } | null;
  fontSize: number;
}

/* "Examples ▾": a drop-down of ready-made programs for the current language.
 * Picking one only loads it (Ctrl+Z brings the old code back); it runs when
 * the user presses Compile. */
function ExamplesMenu({ examples, onPick }: { examples: Example[]; onPick: (e: Example) => void }) {
  const [open, setOpen] = useState(false);
  const wrap = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open) return;
    const close = (e: Event) => {
      if (e instanceof KeyboardEvent ? e.key === "Escape" : !wrap.current?.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("pointerdown", close);
    document.addEventListener("keydown", close);
    return () => {
      document.removeEventListener("pointerdown", close);
      document.removeEventListener("keydown", close);
    };
  }, [open]);

  const groups = ["Valid programs", "Error handling"] as const;
  return (
    <div className="menu-wrap" ref={wrap}>
      <button
        className={`file-btn ${open ? "open" : ""}`}
        onClick={() => setOpen((o) => !o)}
        aria-haspopup="menu"
        aria-expanded={open}
        title="Load a ready-made program"
      >
        Examples ▾
      </button>
      {open && (
        <div className="examples-menu" role="menu">
          {groups.map((g) => (
            <div key={g}>
              <h4>{g}</h4>
              {examples.filter((e) => e.group === g).map((e) => (
                <button key={e.id} role="menuitem" onClick={() => { setOpen(false); onPick(e); }}>
                  <span className={`dot ${g === "Error handling" ? "dot-error" : "dot-valid"}`} />
                  {e.title}
                </button>
              ))}
            </div>
          ))}
          <p className="note">Loads it into the editor; press Compile to run it. Ctrl+Z restores your code.</p>
        </div>
      )}
    </div>
  );
}

const LABELS: Record<string, string> = { c: "C", cpp: "C++", java: "Java" };

export function EditorPane(p: Props) {
  const fileInput = useRef<HTMLInputElement>(null);
  const [cursor, setCursor] = useState({ line: 1, column: 1 });
  const onMount: OnMount = (editor, monacoApi) => {
    editor.onDidChangeCursorPosition((e) => setCursor({ line: e.position.lineNumber, column: e.position.column }));
    p.onMount(editor, monacoApi);
  };
  return (
    <section className="pane">
      <div className="pane-title file-bar">
        <input
          className="file-name"
          value={p.fileName}
          onChange={(e) => p.onFileName(e.target.value)}
          spellCheck={false}
          aria-label="File name"
          title="File name (used when you save)"
          size={Math.max(8, p.fileName.length + 1)}
        />
      </div>
      <div className="toolbar editor-toolbar">
        <div className="file-actions">
          <button className="file-btn" onClick={() => fileInput.current?.click()} title="Open a file from your device">
            Open
          </button>
          <button className="file-btn" onClick={p.onSaveFile} title="Save to your device (Ctrl+S)">
            Save
          </button>
          <ExamplesMenu examples={p.examples} onPick={p.onExample} />
          <input
            ref={fileInput}
            type="file"
            accept={p.openTypes}
            hidden
            onChange={(e) => {
              const file = e.target.files?.[0];
              if (file) p.onOpenFile(file);
              e.target.value = "";   // opening the same file again still fires onChange
            }}
          />
        </div>
        <div className="lang-toggle" role="radiogroup" aria-label="Language">
          {p.languages.map((l) => (
            <button
              key={l.id}
              role="radio"
              aria-checked={l.id === p.language.id}
              className={l.id === p.language.id ? "on" : ""}
              onClick={() => p.onLanguage(l.id)}
              title={l.built ? l.name : `${l.name} is not built yet`}
            >
              {LABELS[l.id] ?? l.id}
              {!l.built && <span className="dot-warn" />}
            </button>
          ))}
        </div>
        <div className="compile-wrap">
          <button className="btn compile-btn" onClick={p.onCompile} disabled={p.busy} title="Compile (Ctrl+Enter)">
            ▶ Compile
          </button>
        </div>
      </div>
      {p.notice && (
        <div className="lang-notice" role="status">
          <span>
            This looks like <b>{p.notice.label}</b> code, but the <b>{LABELS[p.language.id] ?? p.language.id}</b> compiler is selected.
          </span>
          <button className="notice-btn" onClick={p.notice.onSwitch}>Move it to {p.notice.label}</button>
          <button className="notice-close" onClick={p.notice.onDismiss} title="Keep it here" aria-label="Dismiss">×</button>
        </div>
      )}
      <div className="editor-wrap">
        <Editor
          theme={p.editorTheme}
          language={p.language.monaco}
          path={`source-${p.language.id}${p.language.extension}`}
          value={p.source}
          onChange={(v) => p.onSource(v ?? "")}
          onMount={onMount}
          options={{
            fontSize: p.fontSize,
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            automaticLayout: true,
            tabSize: 4,
          }}
        />
      </div>
      <div className="statusbar">
        <span className="spacer" />
        <span className="cursor-pos">Ln {cursor.line}, Col {cursor.column}</span>
      </div>
    </section>
  );
}
