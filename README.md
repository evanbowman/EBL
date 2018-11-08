# EB LISP

A lisp interpreter. Lexically scoped, with proper closures, and easily embeddable.

``` lisp
(def closure-test
     (let ((a 41))
       (lambda (b)
         (+ a b))))

(println (closure-test 1)) ;; 42
```
