# EB LISP

## Introduction
EBL is a LISP dialect, inspired by Scheme and Clojure. Easily embeddable as a scripting language for C++, the environment also supports an interactive top level. EBL compiles fast, to efficient (but not yet optimized) bytecode. The GC uses a mark-compact algorithm, but the collector is modular and you can override it with your own (in C++). EBL also supports lexical closures.

This particular lisp dialect supports efficient tail recursion through the `recur` special form (like clojure). I prefer a `recur` keyword to automatic TCO for a number of reasons:
* It's explicit and obvious to readers of the code.
* A recur keyword allows recursion within anonomyous lambdas.
* Tail call optimization would need to happen at runtime, and could be somewhat expensive. Or at least more expensive than recur, which is essentially free.

EBL philosophy for mutability: data itself (lists, strings, etc.) is immutable, but, although it's discouraged, you can rebind new data to variables.

#### Lexical closures
``` scheme
(def closure-test
     (let ((a 1))
       (lambda (b)
         (set a (+ a b))
         a)))

(println (closure-test 3))  ;; 4
(println (closure-test 38)) ;; 42
```

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
Look how easy it is to build strings in EBL!
```scheme
(string \a \b \c " Hello, " 1 \, 2 \, 3) ;; "abc Hello, 1,2,2"
```

## Further Reading
For more examples, see the code in the lisp/ directory. The standard library in file stdlib.lisp is a good place to start, and there's another example in mandelbrot.lisp, which computes mandelbrot sets.