import { useRef } from "react";
import Editor, { type OnMount } from "@monaco-editor/react";
import type { Language } from "./api";

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
}

const LABELS: Record<string, string> = { c: "C", cpp: "C++", java: "Java" };

export function EditorPane(p: Props) {
  const fileInput = useRef<HTMLInputElement>(null);
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
        <button className="btn compile-btn" onClick={p.onCompile} disabled={p.busy} title="Compile (Ctrl+Enter)">
          ▶ Compile
        </button>
      </div>
      <div className="editor-wrap">
        <Editor
          theme="explorer-dark"
          language={p.language.monaco}
          path={`source-${p.language.id}${p.language.extension}`}
          value={p.source}
          onChange={(v) => p.onSource(v ?? "")}
          onMount={p.onMount}
          options={{
            fontSize: 14,
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            automaticLayout: true,
            tabSize: 4,
          }}
        />
      </div>
      <div className="statusbar">
        <span className="spacer" />
        <span className="muted">Ctrl+Enter to compile</span>
      </div>
    </section>
  );
}
