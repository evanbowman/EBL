
(namespace csv
  (defn read (file-name)
    (fs::open file-name "r"
              (lambda (file)
                (defn impl (result)
                  (def line (fs::get-line file))
                  (if (not line)
                      (std::reverse result)
                      (impl (cons (std::split line \,) result))))
                (impl null)))))
