;;;
;;; A small standard library
;;;

(def println
     (lambda (...)
       "[...] -> print each arg in ... with print, then write a line break"
       (apply print ...)
       (newline)))

(def reverse
     (let ((do-reverse
            (lambda (list result)
              (if (null? list)
                  result
                  (do-reverse (cdr list) (cons (car list) result))))))
       (lambda (list)
         "[list] -> reversed list"
         (do-reverse list null))))

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
