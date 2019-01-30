
(print (((lambda (f)
           ((lambda (g)
              (g g))
            (lambda (g)
              (f (lambda (...)
                   (apply (g g) ...))))))
         (lambda (fact)
           (lambda (n)
             (if (equal? n 0)
                 1
                 (* n (fact (- n 1)))))))
        12))
