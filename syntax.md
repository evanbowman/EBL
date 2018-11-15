# EB Lisp Programming Guide

This guide is intended for programmers with some previous familiarity with lisp, but who want to learn about the syntax for this specific dialect.

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
This is the same old pair that we all know and love: 
``` scheme
(cons 1 2) ;; => (1 2)

(cons 1 (cons 2 (cons 3 (cons 4 null)))) ;; => (1 2 3 4)

(list 1 2 3 4) ;; => (1 2 3 4)

(car (cons 1 2)) ;; => 1

(cdr (cons 1 2)) ;; => 2

(cdr (list 1 2 3 4) ;; => (2 3 4)
```

### Lambda
Lambdas are mostly the same as in other lisp dialects, with a few details worth mentioning:
* EB Lisp has a single namespace for functions and data (like scheme), so you don't need funcall (like emacs)
* EB Lisp supports functions with an indefinite number of arguments. To define a variadic function, add an argument called `...` to the end of a lambda's argument list. If you pass extra arguments to your function, they will be available as a list bound to the `...` parameter:
* EB lisp supports docstrings. Add a docstring by placing a string after the argument list. Retrieve a docstring with the help function.
``` scheme
(help (lambda () "my documentation" (+ 1 2 3))) ;; => "my documenation"
```
