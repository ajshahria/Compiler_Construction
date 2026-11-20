import Editor, { type OnMount } from "@monaco-editor/react";
import type { Example, Language } from "./api";

interface Props {
  languages: Language[];
  language: Language;
  onLanguage: (id: string) => void;
  source: string;
  onSource: (s: string) => void;
  examples: Example[];
  onExample: (path: string) => void;
  onMount: OnMount;
  auto: boolean;
  onAuto: (v: boolean) => void;
  onCompile: () => void;
  busy: boolean;
}

const LABELS: Record<string, string> = { c: "C", cpp: "C++", java: "Java" };

export function EditorPane(p: Props) {
  const groups = [...new Set(p.examples.map((e) => e.group))];
  return (
    <section className="pane">
      <div className="pane-title">
        <span>source{p.language.extension}</span>
      </div>
      <div className="toolbar">
        <select
          className="examples"
          value=""
          onChange={(e) => e.target.value && p.onExample(e.target.value)}
          title="Load an example program"
        >
          <option value="">Load example…</option>
          {groups.map((g) => (
            <optgroup key={g} label={g}>
              {p.examples
                .filter((e) => e.group === g)
                .map((e) => (
                  <option key={e.path} value={e.path}>
                    {e.name}
                  </option>
                ))}
            </optgroup>
          ))}
        </select>
        <span className="spacer" />
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
        <button className="btn" onClick={p.onCompile} disabled={p.busy} title="Compile (Ctrl+Enter)">
          ▶ Compile
        </button>
        <label className="check">
          <input type="checkbox" checked={p.auto} onChange={(e) => p.onAuto(e.target.checked)} /> Compile on change
        </label>
        <span className="spacer" />
        <span className="muted">Ctrl+Enter to compile</span>
      </div>
    </section>
  );
}
