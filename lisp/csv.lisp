
(namespace csv
  (defn read (file-name)
    (fs::open file-name "r"
              (lambda (file)
                ((lambda (result)
                   (def line (fs::get-line file))
                   (if (not line)
                       (std::reverse result)
                       (recur (cons (std::split line \,) result))))
                 null)))))
