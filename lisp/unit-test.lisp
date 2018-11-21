;;;
;;; Unit tests for nightly regression
;;;

(load "lisp/stdlib.lisp")

(def test-case
     (lambda (name body)
       (if (not (body))
           (error name))))

(def test-data (list 1 2 3 4 5 6 7))

(test-case "list function returns a cons"
           (lambda () (pair? test-data)))

(test-case "list function return a list with correct length"
           (lambda () (equal? (length test-data) 7)))

(test-case "car and cdr work correctly"
           (lambda ()
             (and
              (equal? (car test-data) 1)
              (equal? (car (cdr test-data)) 2))))

(test-case "cons works correctly"
           (lambda ()
             (let ((appended (cons 0 test-data)))
               (and
                (equal? (car appended) 0)
                (identical? (cdr appended) test-data)))))

(test-case "closures"
           (lambda ()
             (def c (let ((a 0))
                      (lambda ()
                        (set a (+ a 1))
                        a)))
             (and
              (equal? (c) 1)
              (equal? (c) 2)
              (equal? (c) 3))))
