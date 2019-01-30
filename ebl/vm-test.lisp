
;; (def a 5)

;; (def line-feed (character 10))

;; (if (equal? a 6)
;;     (print "good" line-feed)
;;     (print "bad" line-feed))

;; (print "done" line-feed)


(def test
     (lambda (a b c)
       (print (+ a b c) (character 10))))

(test 1 2 3)
