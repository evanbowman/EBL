;;;
;;; A small standard library
;;;

(defn dolist (proc lat)
  (if (null? lat)
      null
      (begin
        (proc (car lat))
        (recur proc (cdr lat)))))

(namespace std
  (def reverse
       (let ((reverse-helper
              (lambda (lat result)
                (if (null? lat)
                    result
                    (recur (cdr lat) (cons (car lat) result))))))
         (lambda (lat)
           "[list] -> reversed list"
           (reverse-helper lat null))))

  (defn some (pred lat)
    "[pred list] -> first list element that satisfies pred, otherwise false"
    (if (null? lat)
        false
        (if (pred (car lat))
            (car lat)
            (recur pred (cdr lat)))))

  (defn every (pred lat)
    "[pred list] -> true if pred is true for all elements, otherwise false"
    (if (null? lat)
        true
        (if (not (pred (car lat)))
            false
            (recur pred (cdr lat)))))

  (defn assoc (lat key)
    "[key list] -> element associated with key in list, otherwise false"
    (some (lambda (elem)
            (equal? (car elem) key))
          lat))

  (defn identity (x) x)

  (def append
       (let ((append-impl
              (lambda (from to)
                (if (null? from)
                    to
                    (recur (cdr from) (cons (car from) to))))))
         (lambda (l1 l2)
           "[l1 l2] -> l2 appended to l1"
           (append-impl (reverse l1) l2))))

  (defn substr (str first last)
    "[str begin end] -> substring from [begin, end)"
    (apply string
           ((lambda (index lat)
              (if (equal? index first)
                  lat
                  (begin
                    (def next (- index 1))
                    (recur next (cons (string-ref str next) lat)))))
            last null)))

  ;; FIXME! Tokenizing negative numbers is broken right now, because '-', with
  ;; nothing after it, has to also be a valid lvalue.
  (def -1 (- 0 1))

  (defn split (str delim)
    "[str delim] -> list of substrings, by cleaving str at delim"
    ((lambda (index partial result)
       (if (equal? index -1)
           (if partial
               (cons (apply string partial) result)
               result)
           (let ((current (string-ref str index)))
             (if (equal? current delim)
                 (recur (- index 1) null (cons (apply string partial) result))
                 (recur (- index 1) (cons current partial) result)))))
     (- (length str) 1) null null))

  (defn join (lat delim)
    "[lat delim] -> string, by concatenating each elem in lat, with delim in between"
    ((lambda (lat result)
       (if (null? lat)
           result
           (recur (cdr lat)
                  (if result
                      (string result delim (car lat))
                      (string (car lat))))))
     lat false)))
