
(def iters
     (lambda (n)
       (if (< n 1)
           null
           (begin
             (println n)
             (iters (- n 1))))))

(iters 5)

(memory-statistics)
