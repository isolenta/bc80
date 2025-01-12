# Current state of *Reduced C compiler*

## Preprocessor

Functions as the Standard C Preprocessor unless otherwise specified.

- Processes `#include` directives in the format `#include STRING_LITERAL` where `STRING_LITERAL`
  is a string enclosed in double quotes. `STRING_LITERAL` must represent a filesystem path
  that can be located within the list of directories specified by the `-I` command line arguments.
  Loads the content of the specified text file, processes it in the same manner,
  and appends the results of the preprocessor. Each individual file can be processed only once
  per session, functioning similarly to an implicit `#pragma once` in Standard C.

- Conditional preprocessing directives include `#ifdef`, `#ifndef`, `#else`, and `#endif`.
  Compile-time expression evaluation is not supported. Only the boolean state of the constant's
  definition (via the `-D` command line argument or the #define directive) is relevant.

- The ability to define or redefine constant values is provided. Only literal values (not macros)
  are supported. Built-in keywords and operators cannot be redefined.

- Adds (and leaves unprocessed at the preprocessor stage) helper directives `#file` and `#line`
  to enhance diagnostics in subsequent processing stages.

- Strips multi-line `/* ... */` comments and single-line `// ...` comments.

- Supports `#warning` and `#error` directives with a string literal argument.

- The output of the preprocessor can be saved using the `-e` and `--pp-filename` command line arguments.
