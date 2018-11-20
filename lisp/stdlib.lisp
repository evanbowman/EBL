;;;
;;; A small standard library
;;;

(def list
     (lambda (...)
       "[...] -> construct a list from args in ..."
       ...)) ;; Well that was easy :)

(def println
     (lambda (...)
       "[...] -> print each arg in ... with print, then write a line break"
       (apply print ...)
       (newline)))

(def reverse
     (lambda (list)
       "[list] -> reversed list"
       (def up null)
       (dolist (lambda (n) (set up (cons n up))) list)
       up))

(def atom?
     (lambda (x)
       "[x] -> false if x is a pair or if x is null"
       (and (not (pair? x)) (not (null? x)))))

(def some
     (lambda (pred list)
       "[pred list] -> first list element that satisfies pred, otherwise false"
       (if (null? list)
           false
           (if (pred (car list))
               (car list)
               (some pred (cdr list))))))

(def every
     (lambda (pred list)
       "[pred list] -> true if pred is true for all elements, otherwise false"
       (if (null? list)
           true
           (if (not (pred (car list)))
               false
               (every pred (cdr list))))))

(def assoc
     (lambda (list key)
       "[key list] -> element associated with key in list, otherwise false"
       (some (lambda (elem)
               (equal? (car elem) key))
             list)))

(def identity (lambda (x) x))

(def min
     (lambda (first ...)
       "[list] -> find min element in list by calling <"
       (def up first)
       (dolist (lambda (e)
                 (if (< e up)
                     (set up e))) ...)
       up))

(def max
     (lambda (first ...)
       "[list] -> find max element in list by calling >"
       (def up first)
       (dolist (lambda (e)
                 (if (> e up)
                     (set up e))) ...)
       up))
