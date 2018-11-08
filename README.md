# EB LISP

A lisp interpreter. Lexically scoped, with proper closures, and easily embeddable.

``` lisp
(def closure-test
     (let ((a 41))
       (lambda (b)
         (+ a b))))

(println (closure-test 1)) ;; 42
```


### Embedding

Example:
``` c++
#include "lisp.hpp"


lisp::ObjectPtr foo(lisp::Environment& env, const lisp::Arguments& args)
{
    std::cout << "printing from foo: "
              << lisp::checkedCast<lisp::Integer>(env, args[0])->value()
              << std::endl;

    return env.getNull();
}


int main()
{
    // create an execution context
    lisp::Context context({});

    // get a reference to the top level environment (i.e. the global scope)
    auto env = context.topLevel();

    // wrap our custom function in a lisp Function object
    static const int argc = 1;
    auto lfoo = env->create<lisp::Function>("my docstring", argc, foo);

    // store the lisp function in the top level environment
    env->store("foo", lfoo);

    // exec some lisp code that calls our function
    lisp::exec(*env, "(foo 42)");
}
```
