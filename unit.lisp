
(def closure-test
     (let ((a 41))
       (lambda (b)
         (+ a b))))

(println (closure-test 1))
