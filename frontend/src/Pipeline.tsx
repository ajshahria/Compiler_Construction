// Pipeline.tsx - the compiler phases in the top bar, lit up after Compile:
// green = the phase passed, red = the phase that reported errors, grey = not
// reached (compilation stops at the first phase with errors). While a
// compile runs the steps pulse. Clicking a step opens the tab that shows
// that phase's output.
import type { CompileResult, TabId } from "./api";

type State = "idle" | "ok" | "bad" | "skip" | "busy";

const STEPS: { id: string; label: string; tab: TabId; kind?: string }[] = [
  { id: "lexer", label: "Lexer", tab: "tokens", kind: "lexical" },
  { id: "parser", label: "Parser", tab: "ast", kind: "syntax" },
  { id: "semantic", label: "Semantic", tab: "symtab", kind: "semantic" },
  { id: "tac", label: "TAC", tab: "tac" },
  { id: "run", label: "Run", tab: "run", kind: "runtime" },
];

/* The state of every step for a compile result. */
function states(result: CompileResult | null, busy: boolean, failure: string): State[] {
  if (busy) return STEPS.map(() => "busy");
  if (!result || failure) return STEPS.map(() => "idle");
  const kinds = new Set(result.errors.map((e) => e.kind));
  // other server messages ("error") count as a failure of the first phase
  const firstBad = STEPS.findIndex((s) => s.kind && kinds.has(s.kind));
  const failedAt = firstBad >= 0 ? firstBad : kinds.has("error") ? 0 : -1;
  return STEPS.map((_, i) => (failedAt < 0 || i < failedAt ? "ok" : i === failedAt ? "bad" : "skip"));
}

const TITLE: Record<State, string> = {
  idle: "not compiled yet",
  ok: "passed",
  bad: "reported errors",
  skip: "not reached (an earlier phase failed)",
  busy: "compiling…",
};

interface Props {
  result: CompileResult | null;
  busy: boolean;
  failure: string;
  available: TabId[];          // tabs the current compiler produces
  onOpen: (tab: TabId) => void;
}

export function Pipeline({ result, busy, failure, available, onOpen }: Props) {
  const s = states(result, busy, failure);
  const failed = s.indexOf("bad");
  return (
    <nav className="pipeline" aria-label="Compiler phases">
      {STEPS.map((step, i) => (
        <span key={step.id} className="pipe-item">
          {i > 0 && <span className={`pipe-arrow ${s[i] === "ok" ? "ok" : ""}`}>→</span>}
          <button
            className={`pipe-step ${s[i]}`}
            onClick={() => onOpen(s[i] === "bad" ? "errors" : step.tab)}
            disabled={!available.includes(step.tab)}
            title={`${step.label}: ${TITLE[s[i]]}${s[i] === "bad" ? " - click to see the errors" : ""}`}
          >
            {s[i] === "ok" && <span aria-hidden="true">✓ </span>}
            {s[i] === "bad" && <span aria-hidden="true">✕ </span>}
            {step.label}
          </button>
        </span>
      ))}
      {failed >= 0 && <span className="pipe-note">stopped at {STEPS[failed].label}</span>}
    </nav>
  );
}
