// Bundle Monaco locally (no CDN) and register small highlighters for the compiler output.
import * as monaco from "monaco-editor";
import editorWorker from "monaco-editor/esm/vs/editor/editor.worker?worker";
import { loader } from "@monaco-editor/react";

self.MonacoEnvironment = { getWorker: () => new editorWorker() };
loader.config({ monaco });

monaco.languages.register({ id: "tac" });
monaco.languages.setMonarchTokensProvider("tac", {
  tokenizer: {
    root: [
      [/^\s*[A-Za-z_]\w*:/, "type"],
      [/\b(goto|ifFalse|if|print|read|out|outnl|in|call|param|return|true|false)\b/, "keyword"],
      [/\bt\d+\b/, "variable.predefined"],
      [/\bL\d+\b/, "type"],
      [/-?\d+(\.\d+)?/, "number"],
      [/"[^"]*"/, "string"],
    ],
  },
});

monaco.languages.register({ id: "gas" });
monaco.languages.setMonarchTokensProvider("gas", {
  tokenizer: {
    root: [
      [/#.*$/, "comment"],
      [/^\s*[.\w]+:/, "type"],
      [/^\s*\.\w+/, "keyword"],
      [/%\w+/, "variable.predefined"],
      [/\$-?\w+/, "number"],
      [/-?\d+/, "number"],
      [/"[^"]*"/, "string"],
      [/^\s*\w+/, "keyword"],
    ],
  },
});

monaco.languages.register({ id: "phase" });
monaco.languages.setMonarchTokensProvider("phase", {
  tokenizer: {
    root: [
      [/^-{3,}.*$/, "comment"],
      [/\b(KW_\w+|KEYWORD\w*|IDENTIFIER|DELIMITER|OPERATOR\w*|NUMBER\w*|INT_LIT\w*|STRING\w*)\b/, "keyword"],
      [/\(line \d+\)|\bline \d+\b/, "comment"],
      [/\b\d+(\.\d+)?\b/, "number"],
      [/\b[A-Z][A-Za-z]+\b/, "type"],
    ],
  },
});

monaco.editor.defineTheme("explorer-dark", {
  base: "vs-dark",
  inherit: true,
  rules: [],
  colors: {
    "editor.background": "#1e1e1e",
    "editorGutter.background": "#1e1e1e",
  },
});

monaco.editor.defineTheme("explorer-light", {
  base: "vs",
  inherit: true,
  rules: [],
  colors: {
    "editor.background": "#ffffff",
    "editorGutter.background": "#ffffff",
  },
});

export { monaco };
