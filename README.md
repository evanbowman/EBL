# EB LISP

Unit Tests: [![Build Status](https://travis-ci.org/evanbowman/EBL.svg?branch=master)](https://travis-ci.org/evanbowman/EBL)
## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient bytecode.

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

## Example
EBL is just as powerful as any other lisp, e.g. here's a recursive factorial computed from a y-combinator, without any direct recursion or local variables.
```scheme
(((lambda (f)
    ((lambda (g)
       (g g))
     (lambda (g)
       (f (lambda (...)
            (apply (g g) ...))))))
  (lambda (fact)
    (lambda (n)
      (if (equal? n 0)
          1
          (* n (fact (- n 1)))))))
 12)
```

## Syntax Highlighting
For Emacs users, there's a hack of scheme-mode in the emacs/ directory that highlights EBL keywords

## Further Reading
For more examples, see the code in the ebl/ directory. The standard library in file stdlib.ebl is a good place to start, and there's another example in mandelbrot.ebl, which computes mandelbrot sets.
