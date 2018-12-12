;; temporary...
(define (dolist proc list)
  (if (null? list)
      '()
      (begin
        (proc (car list))
        (dolist proc (cdr list)))))


(define iters 500)

(define mandelbrot
     (lambda (z c iter-depth)
       (if (> iter-depth 0)
           (if (< (real-part z) 2.0)
               (mandelbrot (+ (* z z) c) c (- iter-depth 1))
               #f)
           #t)))

(define make-matrix
     (let ((x-coeff (/ 3.5 100.0))
           (y-coeff (/ 2.0 50.0)))
       (lambda (xl yl n)
         (if (< n 1)
             (cons xl yl)
             (begin
               (let ((list-x (cons (- 1.0 (* n x-coeff)) xl))
                     (list-y (if (< n 50)
                                 (cons (- 1.0 (* n y-coeff)) yl)
                                 yl)))
                 (make-matrix list-x list-y (- n 1))))))))


(define data-mat (make-matrix '() '() 100))

(dolist
 (lambda (i)
   (dolist
    (lambda (r)
      (if (mandelbrot (make-rectangular 0.0 0.0) (make-rectangular r i) iters)
          (display "*")
          (display " ")))
    (car data-mat))
   (newline))
 (cdr data-mat))
