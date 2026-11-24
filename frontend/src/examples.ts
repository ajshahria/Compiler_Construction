// Ready-made programs for the Examples menu: one set per compiler, covering
// what the lab manual asks a demo to show (section 13): a valid program,
// lexical, syntax and semantic errors, intermediate code, plus input and
// optimization. The programs are plain source files in ./examples, so each
// can also be compiled on the command line.
import type { TabId } from "./api";

import cManual from "./examples/c/manual_sample.c?raw";
import cFull from "./examples/c/full_program.c?raw";
import cInput from "./examples/c/read_input.c?raw";
import cLexical from "./examples/c/lexical_errors.c?raw";
import cSyntax from "./examples/c/syntax_errors.c?raw";
import cSemantic from "./examples/c/semantic_errors.c?raw";
import cOpt from "./examples/c/optimization.c?raw";

import cppManual from "./examples/cpp/manual_sample.cpp?raw";
import cppFull from "./examples/cpp/full_program.cpp?raw";
import cppInput from "./examples/cpp/read_input.cpp?raw";
import cppLexical from "./examples/cpp/lexical_errors.cpp?raw";
import cppSyntax from "./examples/cpp/syntax_errors.cpp?raw";
import cppSemantic from "./examples/cpp/semantic_errors.cpp?raw";
import cppOpt from "./examples/cpp/optimization.cpp?raw";

import javaManual from "./examples/java/manual_sample.java?raw";
import javaFull from "./examples/java/full_program.java?raw";
import javaInput from "./examples/java/read_input.java?raw";
import javaLexical from "./examples/java/lexical_errors.java?raw";
import javaSyntax from "./examples/java/syntax_errors.java?raw";
import javaSemantic from "./examples/java/semantic_errors.java?raw";
import javaOpt from "./examples/java/optimization.java?raw";

export interface Example {
  id: string;
  title: string;
  group: "Valid programs" | "Error handling";
  source: string;
  tab: TabId;        // the output tab shown after the example compiles
  stdin?: string;    // program input, for an example that reads input
}

/* The same seven examples for each language, in menu order. */
function set(sources: string[], fileTitle: string): Example[] {
  const [manual, full, input, lexical, syntax, semantic, opt] = sources;
  return [
    { id: "manual", title: "Manual sample (section 5.5) → TAC", group: "Valid programs", source: manual, tab: "tac" },
    { id: "full", title: `Full ${fileTitle} program → Output`, group: "Valid programs", source: full, tab: "run" },
    { id: "input", title: "Reading input → Output", group: "Valid programs", source: input, tab: "run", stdin: "3 2.5" },
    { id: "opt", title: "Constant folding → Optimized TAC", group: "Valid programs", source: opt, tab: "opt" },
    { id: "lexical", title: "Lexical errors", group: "Error handling", source: lexical, tab: "errors" },
    { id: "syntax", title: "Syntax errors", group: "Error handling", source: syntax, tab: "errors" },
    { id: "semantic", title: "Semantic errors (all six rules)", group: "Error handling", source: semantic, tab: "errors" },
  ];
}

export const EXAMPLES: Record<string, Example[]> = {
  c: set([cManual, cFull, cInput, cLexical, cSyntax, cSemantic, cOpt], "C"),
  cpp: set([cppManual, cppFull, cppInput, cppLexical, cppSyntax, cppSemantic, cppOpt], "C++"),
  java: set([javaManual, javaFull, javaInput, javaLexical, javaSyntax, javaSemantic, javaOpt], "Java"),
};
