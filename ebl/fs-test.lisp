
(open-dll "./libfs.so")

(fs::open "temp.txt" "r"
          (lambda (file)
            (get-line file)))
