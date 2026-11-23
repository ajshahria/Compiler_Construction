export type TabId = "headers" | "tokens" | "ast" | "symtab" | "tac" | "opt" | "asm" | "run" | "errors";

export interface Language {
  id: string;
  name: string;
  extension: string;
  monaco: string;
  tabs: TabId[];
  options: string[];
  built: boolean;
  starter: string;  // program shown the first time the language is picked
  input: boolean;   // the language can read input (cin, scanf, Scanner)
}

export interface LanguagesResponse {
  tabs: { id: TabId; label: string }[];
  languages: Language[];
}

export interface CompileError {
  line: number | null;
  kind: string;
  message: string;
}

export interface CompileResult {
  language: string;
  phases: Partial<Record<TabId, string>>;
  errors: CompileError[];
  summary: string;
  exitCode: number;
  ms: number;
}

async function request<T>(url: string, init?: RequestInit): Promise<T> {
  const res = await fetch(url, init);
  if (!res.ok) {
    let detail = `${res.status} ${res.statusText}`;
    try {
      const body = await res.json();
      if (body.detail) detail = typeof body.detail === "string" ? body.detail : JSON.stringify(body.detail);
    } catch {
      /* keep the status text */
    }
    throw new Error(detail);
  }
  return res.json();
}

export const getLanguages = () => request<LanguagesResponse>("/api/languages");

export const compile = (body: { language: string; source: string; options: string[]; stdin: string }, signal?: AbortSignal) =>
  request<CompileResult>("/api/compile", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
    signal,
  });
