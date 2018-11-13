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
         (do-reverse list null))))
