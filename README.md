# EB LISP

## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient (but not yet optimized) bytecode. The GC uses a mark-compact algorithm, but the collector is modular and you can override it with your own (in C++). EBL supports first class lexical closures.

This particular lisp dialect enables efficient tail recursion through the `recur` special form (like clojure). I prefer a `recur` keyword to automatic TCO for a number of reasons, foremost that it's explicit and allows recursion in anonomous lambdas.

Mutability: data itself (numbers, lists, strings, etc.) is immutable, but you can rebind new data to variables.

#### Builtin unicode
```scheme
(def 범위 length)

(범위 "여보세요 세계") ;; 7

```

#### Namespaces
```scheme
(namespace math
  (defn clamp (x lower upper)
    (cond
     ((< x lower) lower)
     ((> x upper) upper)
     (true x)))

  (defn smoothstep (edge0 edge1 x)
    (let ((clamped (clamp (/ (- x edge0) (- edge1 edge0)) 0.0 1.0)))
      (* clamped clamped (- 3.0 (* clamped 2.0))))))

(math::smoothstep 0.0 100.0 25.0) ;; 0.15625
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