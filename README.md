# EB LISP

## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient bytecode. Language specification.

#### Implementation status
- [x] First-class functions and closures
- [x] Compacting GC
- [x] Bytecode VM
- [x] Unicode Chars, and Strings, and Identifiers
- [x] Complex numbers
- [x] Eval
- [x] Namespaces
- [x] Docstrings
- [x] Variadic Functions
- [x] Manual TCO
- [ ] Macros

#### Library status
- [x] Basic File I/O
- [x] Streams
- [x] JSON
- [ ] Filesystem Library
- [ ] Network Library


## Syntax Highlighting
For Emacs users, there's a hack of scheme-mode in the emacs/ directory that highlights EBL keywords

## Further Reading
For more examples, see the code in the lisp/ directory. The standard library in file stdlib.ebl is a good place to start, and there's another example in mandelbrot.lisp, which computes mandelbrot sets.
