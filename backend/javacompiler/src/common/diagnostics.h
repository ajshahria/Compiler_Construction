/*
 * diagnostics.h - error counters shared by the compiler phases.
 *
 * Every phase prints its own messages to stderr in the form
 *     line N: <lexical|syntax|semantic> error: <description>
 * and counts them, so the driver can stop after the first failing phase.
 */
#pragma once

extern int lexicalErrors;   // defined in lexer.l
extern int syntaxErrors;    // defined in parser.y
