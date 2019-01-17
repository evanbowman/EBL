# EB LISP

## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient (but not yet optimized) bytecode. In terms of mutability, data itself (numbers, lists, strings, etc.) is immutable, but you can rebind new data to certain variables. The language intentionally does not support automatic tail call optimization for a number of reasons, but still enables space-efficient manual optimization with the `recur` special form.

#### Implementation status
- [x] First-class functions and closures
- [x] Compacting GC
- [x] Bytecode VM
- [x] Unicode Chars, and Strings, and Identifiers
- [x] Complex numbers
- [x] Eval
- [x] Namespaces
- [x] Docstrings
- [ ] Variadic Functions
- [ ] Macros

#### Library status
- [x] Basic File I/O
- [x] Streams
- [x] JSON
- [ ] Filesystem Library
- [ ] Network Library

#### Builtin unicode
```scheme
(def 범위 length)

(범위 "여보세요 세계") ;; 7

```

#### Namespaces
```scheme
(namespace example
  (defn f1 ()
    1)

  (defn f2 ()
    ;; Within a namespace, you can reference other variables directly
    (f1)))

;; Outside the namespace, use the double-colon to refer to functions
(example::f2) ;; 1
```

#### Strings
Look how easy it is to build strings in EBL! The string function is variadic and accepts arguments of any type:
```scheme
(string " Hello, " \W \o \r \l \d " " 1 \, 2 \, 3) ;; "Hello, World 1,2,3"
```

## Syntax Highlighting
For Emacs users, there's a hack of scheme-mode in the emacs/ directory that highlights EBL keywords

## Further Reading
For more examples, see the code in the lisp/ directory. The standard library in file stdlib.ebl is a good place to start, and there's another example in mandelbrot.lisp, which computes mandelbrot sets.
