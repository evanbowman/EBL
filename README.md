# EB LISP

### NOTE: I abandoned this project years ago.

## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient bytecode. If you'd like to give the system a try, build with cmake, then run `ebl-dofile ebl/repl.ebl` to start an interactive shell.

#### Implementation status
- [x] First-class functions and closures [-->](#S-closures)
- [x] Unicode Chars, and Strings, and Identifiers [-->](#S-unicode)
- [x] Docstrings [-->](#S-docstrings)
- [x] Namespaces [-->](#S-namespaces)
- [x] Variadic Functions [-->](#S-variadic-functions)
- [x] Eval
- [x] Complex numbers
- [x] Manual TCO
- [x] Compacting GC
- [x] Bytecode VM
- [ ] Macros

#### Library status
- [x] Basic File I/O
- [x] Streams
- [x] JSON
- [ ] Filesystem Library
- [ ] Network Library


## Examples

### <a name="S-closures"></a>Closures
```clojure
(def test
     (let-mut ((a 0))
       (lambda ()
         (set a (incr a))
         a)))

(test) ;; 1
(test) ;; 2
(test) ;; 3
;; etc
```
### <a name="S-unicode"></a>Unicode
```clojure
(def 범위 length)

(범위 "여보세요 세계") ;; 7
```

### <a name="S-docstrings"></a>Docstrings
```clojure
(defn foo ()
  "(foo) -> the number one"
  1)

(help foo)           ;; "[foo] -> the number one"
(string? (help foo)) ;; true
```

### <a name="S-namespaces"></a>Namespaces
```clojure
(namespace math
  (defn square (n)
    (* n n)))

(math::square 5) ;; 25
```

### <a name="S-variadic-functions"></a>Variadic Functions
```clojure
;; the runtime actually defines list this way!
(defn list (...)
  ...)

(defn count-args (...)
  (length ...))

(count-args 1 2 3) ;; 3
```

## Syntax Highlighting
For Emacs users, there's a hack of scheme-mode in the emacs/ directory that highlights EBL keywords

## Further Reading
For more examples, see the code in the ebl/ directory. The standard library in file stdlib.ebl is a good place to start, and there's another example in mandelbrot.ebl, which computes mandelbrot sets.
