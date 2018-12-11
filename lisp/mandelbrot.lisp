(open-dll "libfs.dylib")
(load "lisp/stdlib.lisp")
(def newline (let ((lf (character 10))) (lambda () (print lf))))

(def iters 500)

(def mandelbrot
     (lambda (z c iter-depth)
       (if (> iter-depth 0)
           (if (< (abs z) 2.0)
               (recur (+ (* z z) c) c (- iter-depth 1))
               false)
           true)))

(def make-matrix
     (let ((x-coeff (/ 3.5 100.0))
           (y-coeff (/ 2.0 50.0)))
       (lambda (xl yl n)
         (if (< n 1)
             (cons xl yl)
             (begin
               (def list-x (cons (- 1.0 (* n x-coeff)) xl))
               (def list-y (if (< n 50)
                               (cons (- 1.0 (* n y-coeff)) yl)
                               yl))
               (recur list-x list-y (- n 1)))))))


(def data-mat (make-matrix null null 100))

(dolist
 (lambda (i)
   (dolist
    (lambda (r)
      (if (mandelbrot (complex 0.0 0.0) (complex r i) iters)
          (print "*")
          (print " ")))
    (car data-mat))
   (newline))
 (cdr data-mat))
