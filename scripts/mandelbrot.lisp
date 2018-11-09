
(def iters 500)

(def mandelbrot
     (lambda (z c iter-depth)
       (if (> iter-depth 0)
           (if (< (abs z) 2.0)
               (mandelbrot (+ (* z z) c) c (- iter-depth 1))
               false)
           true)))

(def make-matrix
     (lambda (xl yl n)
       (if (< n 1)
           (cons xl yl)
           (begin
             (def list-x (cons (- 1.0 (* n (/ 3.5 100.0))) xl))
             (def list-y (if (< n 50)
                             (cons (- 1.0 (* n (/ 2.0 50.0))) yl)
                             yl))
             (make-matrix list-x list-y (- n 1))))))

(def data-mat (make-matrix null null 100))

(dolist
 (lambda (i)
   (dolist
    (lambda (r)
      (if (mandelbrot (complex 0.0 0.0) (complex r i) iters)
          (print "*")
          (print " ")))
    (car data-mat))
   (println ""))
 (cdr data-mat))
