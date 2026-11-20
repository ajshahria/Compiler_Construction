import Editor from "@monaco-editor/react";
import type { CompileError, CompileResult, Language, TabId } from "./api";

interface Props {
  language: Language;
  tabs: { id: TabId; label: string }[];
  tab: TabId;
  onTab: (t: TabId) => void;
  result: CompileResult | null;
  failure: string;
  busy: boolean;
  options: string[];
  onToggleOption: (o: string) => void;
  stdin: string;
  onStdin: (s: string) => void;
  onJump: (e: CompileError) => void;
}

const VIEW_LANGUAGE: Partial<Record<TabId, string>> = { tac: "tac", opt: "tac", asm: "gas" };

const NOT_COMPILED = "<Press Compile or Ctrl+Enter to run>";

const EMPTY: Partial<Record<TabId, string>> = {
  headers: "<no #include headers found>",
  run: "<program printed nothing>",
};

export function OutputPane(p: Props) {
  const errors = p.result?.errors ?? [];
  const supported = new Set(p.language.tabs);
  const text = p.result?.phases[p.tab] ?? "";

  let status;
  if (p.busy) status = <span className="muted">Compiling…</span>;
  else if (p.failure) status = <span className="bad">✕ {p.failure}</span>;
  else if (p.result)
    status =
      errors.length === 0 ? (
        <span className="good">✓ 0 errors · {p.result.ms} ms</span>
      ) : (
        <span className="bad">
          ✕ {errors.length} error{errors.length === 1 ? "" : "s"} · {p.result.ms} ms
        </span>
      );

  return (
    <section className="pane">
      <div className="pane-title">
        <span>{p.language.name}</span>
      </div>
      <div className="toolbar">
        <span className="compiler-name">{p.language.name}</span>
        <div className="options">
          {p.language.options.length === 0 ? (
            <span className="muted">No compiler options</span>
          ) : (
            p.language.options.map((o) => (
              <button
                key={o}
                className={`chip ${p.options.includes(o) ? "on" : ""}`}
                onClick={() => p.onToggleOption(o)}
                aria-pressed={p.options.includes(o)}
              >
                {o}
              </button>
            ))
          )}
        </div>
      </div>
      <div className="tabs" role="tablist">
        {p.tabs.map((t) => {
          const ok = supported.has(t.id);
          return (
            <button
              key={t.id}
              role="tab"
              aria-selected={p.tab === t.id}
              className={`tab ${p.tab === t.id ? "on" : ""}`}
              disabled={!ok}
              title={ok ? t.label : `${t.label} is not produced by ${p.language.name}`}
              onClick={() => p.onTab(t.id)}
            >
              {t.label}
              {t.id === "errors" && errors.length > 0 && <span className="badge">{errors.length}</span>}
            </button>
          );
        })}
      </div>

      <div className="output-body">
        {p.tab === "errors" ? (
          <div className="error-list">
            {p.failure && <div className="error-row server">{p.failure}</div>}
            {!p.failure && errors.length === 0 && (
              <div className="muted pad">{p.result ? "No errors" : p.busy ? "Compiling…" : NOT_COMPILED}</div>
            )}
            {errors.map((e, i) => (
              <button key={i} className="error-row" onClick={() => p.onJump(e)} disabled={e.line == null}>
                {e.line != null && <span className="loc">line {e.line}</span>}
                <span className={`kind kind-${e.kind}`}>{e.kind}</span>
                <span>{e.message}</span>
              </button>
            ))}
            {p.result?.summary && <div className="muted pad">{p.result.summary}</div>}
          </div>
        ) : (
          <Editor
            theme="explorer-dark"
            language={VIEW_LANGUAGE[p.tab] ?? "phase"}
            value={
              text ||
              (errors.length || p.failure
                ? "<Compilation failed. See the Errors tab.>"
                : p.result
                  ? EMPTY[p.tab] ?? "<no output>"
                  : p.busy
                    ? ""
                    : NOT_COMPILED)
            }
            options={{
              readOnly: true,
              domReadOnly: true,
              fontSize: 14,
              minimap: { enabled: false },
              scrollBeyondLastLine: false,
              automaticLayout: true,
              renderLineHighlight: "none",
            }}
          />
        )}
      </div>

      {p.tab === "run" && p.language.input && (
        <div className="stdin">
          <label htmlFor="stdin">Program input (cin)</label>
          <textarea
            id="stdin"
            value={p.stdin}
            onChange={(e) => p.onStdin(e.target.value)}
            placeholder="5 7"
            rows={2}
          />
        </div>
      )}

      <div className="statusbar">
        {status}
        <span className="spacer" />
        {p.result && <span className="muted">exit {p.result.exitCode}</span>}
      </div>
    </section>
  );
}
