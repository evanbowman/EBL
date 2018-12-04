# EB LISP

## Introduction
A lisp dialect, runs standalone, or can be easily embedded in C++ applications. Some notable features listed below:

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

#### Variadic functions

If you create a variable called `...`, extra arguments will be passed as a list bound to `...`.
```scheme
(defn example* (factor ...)
  (if (not (null? ...))
      (begin
        (print (* factor (car ...)) " ")
        (apply example* (cdr ...)))))

(example* 10 1 2 3 4 5) ;; 10 20 30 40 50
```

## Non-trivial example:
Here's something a little more advanced, mandelbrot sets:

Code: https://github.com/evanbowman/lisp/blob/master/lisp/mandelbrot.lisp
Output:
```


                               *
                               *
                             *****
                             *****
                             *****
                              ****
                          * ********* *
                        **************    **
                  ***  *********************
                   ***********************
                    ************************
                   ****************************
                   ***************************
                  ****************************
                 ******************************
                  ******************************
                 ********************************     ***** *
                 ********************************   *********
                 ********************************  ************
                  ******************************* *************
                  ******************************* *************
                   ****************************** *****************
                     ****************************************************************
                   ****************************** *****************
                  ******************************* *************
                  ******************************* *************
                 ********************************  ************
                 ********************************   *********
                 ********************************     ***** *
                  ******************************
                 ******************************
                  ****************************
                   ***************************
                   ****************************
                    ************************
                   ***********************
                  ***  *********************
                        **************    **
                          * ********* *
                              ****
                             *****
                             *****
                             *****
                               *
                               *



```