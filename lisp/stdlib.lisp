;;;
;;; A small standard library
;;;

(def reverse
     (let ((do-reverse
            (lambda (list result)
              (if (null? list)
                  result
                  (do-reverse (cdr list) (cons (car list) result))))))
       (lambda (list)
         "[list] -> reversed list"
         (do-reverse list null))))

(def assoc
     (lambda (list key)
       "[key list] -> find element associated with key in list"
       (if (null? list)
           false
           (if (equal? (car (car list)) key)
               (cdr (car list))
               (assoc (cdr list) key)))))

(def atom?
     (lambda (x)
       "[x] -> false if x is a pair or if x is null"
       (and (not (pair? x)) (not (null? x)))))

(def some
     (lambda (pred list)
       (if (null? list)
           false
           (if (pred (car list))
               (car list)
               (some pred (cdr list))))))

(def every
     (lambda (pred list)
       (if (null? list)
           true
           (if (not (pred (car list)))
               false
               (every pred (cdr list))))))
