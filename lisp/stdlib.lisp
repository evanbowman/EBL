;;;
;;; A small standard library
;;;

(defn list (...)
  "[...] -> construct a list from args in ..."
  ...) ;; Well that was easy :)

(defn println (...)
  "[...] -> print each arg in ... with print, then write a line break"
  (apply print ...)
  (newline))

(defn reverse (list)
  "[list] -> reversed list"
  (def up null)
  (dolist (lambda (n) (set up (cons n up))) list)
  up)

(defn atom? (x)
  "[x] -> false if x is a pair or if x is null"
  (and (not (pair? x)) (not (null? x))))

(defn some (pred list)
  "[pred list] -> first list element that satisfies pred, otherwise false"
  (if (null? list)
      false
      (if (pred (car list))
          (car list)
          (some pred (cdr list)))))

(defn every (pred list)
  "[pred list] -> true if pred is true for all elements, otherwise false"
  (if (null? list)
      true
      (if (not (pred (car list)))
          false
          (every pred (cdr list)))))

(defn assoc (list key)
  "[key list] -> element associated with key in list, otherwise false"
  (some (lambda (elem)
          (equal? (car elem) key))
        list))

(defn identity (x) x)

(defn min (first ...)
  "[args] -> find min element in args by calling <"
  (def up first)
  (dolist (lambda (e)
            (if (< e up)
                (set up e))) ...)
  up)

(defn max (first ...)
  "[first ...] -> find max element in args by calling >"
  (def up first)
  (dolist (lambda (e)
            (if (> e up)
                (set up e))) ...)
  up)

(defn caar (l) (car (car l)))
(defn cadr (l) (car (cdr l)))
(defn cdar (l) (cdr (car l)))
(defn caddr (l) (car (cdr (cdr l))))
(defn cadddr (l) (car (cdr (cdr (cdr l)))))
