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
