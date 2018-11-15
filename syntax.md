# EB Lisp Programming Guide



## Data Types

### Null
You can express null (the empty list) with the `null` keyword:
``` scheme
null 
```

### Boolean
Create a boolean with the `true` and `false` keywords:
``` scheme
true
false
```

### Pair
One of the most important data types for programming in lisp! You can create a pair with the `cons` function:
``` scheme
(cons 1 2) ;; => (1 2)
```
A list is merely a chain of pairs:
``` scheme
(cons 1 (cons 2 (cons 3 (cons 4 null)))) ;; => (1 2 3 4)
```
But you can create a list more easily by using the list function:
``` scheme
(list 1 2 3 4) ;; => (1 2 3 4)
```
The first and second elements of a pair can be accessed by the `car` and `cdr` functions, respectively:
``` scheme
(car (cons 1 2)) ;; => 1

(cdr (cons 1 2)) ;; => 2

(cdr (list 1 2 3 4) ;; => (2 3 4)
```

### Lambda
A lambda is a nameless function. To define a lambda, use the lambda keyword, followed by an argument list, followed by any number of expressions in the function body.
``` scheme
(lambda ()   ;; No arguments
  (print 5)) ;; call print with argument 5
 ```
To call a function, surround the function, followed by the arguments, with parentheses. A function implicitly returns the result of the last expression in the function body:
``` scheme
;; Bind a lambda to the variable foo:
(def foo
  (lambda (a b)
    (cons a b)))
    
;; Apply foo with two arguments
(foo 1 2) ;; => (1 2)

;; Call a lambda without binding it to a variable
((lambda (a b) (+ a b)) 1 2) ;; => 3
 ```
