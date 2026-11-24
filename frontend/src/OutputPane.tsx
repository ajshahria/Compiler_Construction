import { useEffect, useRef } from "react";
import Editor, { DiffEditor } from "@monaco-editor/react";
import type { CompileError, CompileResult, Language, TabId } from "./api";
import { AstTree } from "./AstTree";

export type AstView = "tree" | "text";
export type OptView = "optimized" | "compare";

interface Props {
  language: Language;
  tabs: { id: TabId; label: string }[];
  tab: TabId;
  onTab: (t: TabId) => void;
  result: CompileResult | null;
  failure: string;
  busy: boolean;
  needsInput: boolean;
  stdin: string;
  onStdin: (s: string) => void;
  onJump: (e: CompileError) => void;
  onCompile: () => void;
  editorTheme: string;      // Monaco theme for the read-only views
  fontSize: number;
  astView: AstView;
  onAstView: (v: AstView) => void;
  optView: OptView;
  onOptView: (v: OptView) => void;
  phone: boolean;           // phone layout: one row of tabs, compare shown inline
  visible: boolean;         // false while a phone shows the code pane instead
}

/* The tabs in the order of the pipeline: the front end the lab manual
 * requires, the back end added as a bonus, then the errors. */
const GROUPS: { name: string; title: string; ids: TabId[] }[] = [
  { name: "Front end", title: "Front end: the phases the lab manual requires", ids: ["headers", "tokens", "ast", "symtab", "tac"] },
  { name: "Back end", title: "Back end (bonus): optimizer, x86 assembly and the program's output", ids: ["opt", "asm", "run"] },
  { name: "Diagnostics", title: "Every lexical, syntax, semantic and runtime error", ids: ["errors"] },
];

const VIEW_LANGUAGE: Partial<Record<TabId, string>> = { tac: "tac", opt: "tac", asm: "gas" };

const EMPTY: Partial<Record<TabId, string>> = {
  headers: "<no #include headers found>",
  run: "<program printed nothing>",
};

/* TAC instructions in a listing (labels and blank lines are not instructions). */
function instructionCount(tac: string) {
  return tac.split(/\r?\n/).filter((l) => l.trim() && !/^\s*[\w.@]+:\s*$/.test(l)).length;
}

/* Shown before the first compile: what to do and the keyboard shortcuts. */
function Welcome({ language, onCompile }: { language: Language; onCompile: () => void }) {
  return (
    <div className="welcome">
      <div className="welcome-card">
        <h2>Ready to compile</h2>
        <p className="muted">{language.name} · every phase of the compiler, one tab each</p>
        <ol>
          <li>Write a program, or pick one from <b>Examples ▾</b>.</li>
          <li>Press <b>▶ Compile</b> (or <kbd>Ctrl</kbd>+<kbd>Enter</kbd>).</li>
          <li>
            Follow the program through the tabs: the <b>front end</b> (Headers → Tokens → AST → Symbol Table → TAC),
            the <b>back end</b> (Optimized TAC → Assembly → Output), and <b>Errors</b>.
          </li>
        </ol>
        <button className="btn welcome-btn" onClick={onCompile}>▶ Compile now</button>
        <div className="shortcuts">
          <span><kbd>Ctrl</kbd>+<kbd>Enter</kbd> compile</span>
          <span><kbd>Ctrl</kbd>+<kbd>S</kbd> save</span>
          <span><kbd>Ctrl</kbd>+<kbd>Z</kbd> undo an example</span>
        </div>
      </div>
    </div>
  );
}

/* A two-button switch floating at the top right of a view. */
function ViewSwitch<T extends string>(props: { label: string; value: T; options: [T, string][]; onChange: (v: T) => void }) {
  return (
    <div className="view-switch" role="radiogroup" aria-label={props.label}>
      {props.options.map(([v, text]) => (
        <button key={v} role="radio" aria-checked={props.value === v}
                className={props.value === v ? "on" : ""} onClick={() => props.onChange(v)}>
          {text}
        </button>
      ))}
    </div>
  );
}

export function OutputPane(p: Props) {
  const errors = p.result?.errors ?? [];
  const supported = new Set(p.language.tabs);
  const text = p.result?.phases[p.tab] ?? "";
  const tacText = p.result?.phases.tac ?? "";
  const optText = p.result?.phases.opt ?? "";
  const canCompare = p.tab === "opt" && !!tacText && !!optText;
  const notCompiled = !p.result && !p.busy && !p.failure;

  // On a phone the tabs are one scrolling row: keep the selected tab in view.
  const tabsRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!p.visible) return;    // a hidden row cannot scroll
    const row = tabsRef.current;
    const on = row?.querySelector<HTMLElement>(".tab.on");
    if (row && on) row.scrollLeft = on.offsetLeft - (row.clientWidth - on.offsetWidth) / 2;
  }, [p.tab, p.phone, p.visible]);

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

  const viewerOptions = {
    readOnly: true,
    domReadOnly: true,
    fontSize: p.fontSize,
    minimap: { enabled: false },
    scrollBeyondLastLine: false,
    automaticLayout: true,
    renderLineHighlight: "none" as const,
  };

  let body;
  if (notCompiled) {
    body = <Welcome language={p.language} onCompile={p.onCompile} />;
  } else if (p.tab === "ast" && text && p.astView === "tree") {
    body = <AstTree text={text} onJump={(line) => p.onJump({ line, kind: "", message: "" })} />;
  } else if (canCompare && p.optView === "compare") {
    const before = instructionCount(tacText);
    const after = instructionCount(optText);
    body = (
      <div className={p.phone ? "compare inline" : "compare"}>
        <div className="compare-head">
          <span>TAC (before)</span>
          <span className="compare-sum">
            {before} → {after} instructions
            {before > after && <b className="good"> (−{before - after}, {Math.round(((before - after) / before) * 100)}% fewer)</b>}
          </span>
          <span>Optimized TAC (after)</span>
        </div>
        <div className="compare-body">
          <DiffEditor
            theme={p.editorTheme}
            language="tac"
            original={tacText}
            modified={optText}
            originalModelPath="compare-before.tac"
            modifiedModelPath="compare-after.tac"
            options={{
              ...viewerOptions,
              // two columns under the headers; one column on a phone
              renderSideBySide: !p.phone,
              useInlineViewWhenSpaceIsLimited: false,
              originalEditable: false,
              ignoreTrimWhitespace: true,
              renderOverviewRuler: false,
              enableSplitViewResizing: false,
            }}
          />
        </div>
      </div>
    );
  } else if (p.tab === "errors") {
    body = (
      <div className="error-list" style={{ fontSize: p.fontSize - 1 }}>
        {p.failure && <div className="error-row server">{p.failure}</div>}
        {!p.failure && errors.length === 0 && (
          <div className="muted pad">{p.result ? "No errors" : "Compiling…"}</div>
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
    );
  } else {
    body = (
      <Editor
        theme={p.editorTheme}
        language={VIEW_LANGUAGE[p.tab] ?? "phase"}
        value={
          text ||
          (errors.length || p.failure
            ? "<Compilation failed. See the Errors tab.>"
            : p.result
              ? EMPTY[p.tab] ?? "<no output>"
              : "")
        }
        options={viewerOptions}
      />
    );
  }

  return (
    <section className="pane">
      <div className="pane-title">
        <span>{p.language.name}</span>
      </div>
      <div className="tabs" role="tablist" ref={tabsRef}>
        {GROUPS.map((g) => (
          <div key={g.name} className="tab-group" role="group" aria-label={g.name} title={g.title}>
            {g.ids.map((id) => {
              const t = p.tabs.find((x) => x.id === id);
              if (!t) return null;
              const ok = supported.has(id);
              return (
                <button
                  key={id}
                  role="tab"
                  aria-selected={p.tab === id}
                  className={`tab ${p.tab === id ? "on" : ""}`}
                  disabled={!ok}
                  title={ok ? t.label : `${t.label} is not produced by ${p.language.name}`}
                  onClick={() => p.onTab(id)}
                >
                  {t.label}
                  {id === "errors" && errors.length > 0 && <span className="badge">{errors.length}</span>}
                </button>
              );
            })}
          </div>
        ))}
      </div>

      <div className="output-body">
        {!notCompiled && p.tab === "ast" && text && (
          <ViewSwitch label="AST view" value={p.astView} onChange={p.onAstView}
                      options={[["tree", "Tree"], ["text", "Text"]]} />
        )}
        {!notCompiled && canCompare && (
          <ViewSwitch label="Optimized TAC view" value={p.optView} onChange={p.onOptView}
                      options={[["optimized", "Optimized"], ["compare", "Compare with TAC"]]} />
        )}
        {body}
      </div>

      {p.tab === "run" && p.needsInput && (
        <div className="stdin">
          <label htmlFor="stdin">Program input (values separated by spaces or new lines): type it, then press Compile again</label>
          <textarea
            id="stdin"
            value={p.stdin}
            onChange={(e) => p.onStdin(e.target.value)}
            placeholder="5 7"
            rows={2}
            style={{ fontSize: p.fontSize }}
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
