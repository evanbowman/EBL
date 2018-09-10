#include <functional>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include "lisp.hpp"
#include "lexer.hpp"
#include <chrono>

namespace lisp {

Environment::Environment() : heap_(4096),
                     booleans_{{create<Boolean>(*this, false)},
                               {create<Boolean>(*this, true)}},
                     nullValue_{create<Null>(*this)},
                     variables_{{
    {"cons",
     create<Subr>(*this, "(cons CAR CDR)", 2,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return create<Pair>(env, argv[0], argv[1]);
                  })},
    {"list",
     create<Subr>(*this, nullptr, 0,
                  [](Environment& env, Subr::ArgCount argc,
                     Subr::ArgVec argv) -> ObjectPtr {
                      if (argc) {
                          auto tail = create<Pair>(env, argv[argc - 1],
                                                   env.nullValue_.value_);
                          Local<Pair> list(env, tail);
                          for (Subr::ArgCount pos = argc - 2; pos > -1; --pos) {
                              list = create<Pair>(env, argv[pos], list.get());
                          }
                          return list.get();
                      } else {
                          return env.nullValue_.value_;
                      }
                  })},
    {"car",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return checkedCast<Pair>(env, argv[0]).deref(env).getCar();
                  })},
    {"cdr",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return checkedCast<Pair>(env, argv[0]).deref(env).getCdr();
                  })},
    {"null?",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[isType<Null>(env, argv[0])].value_;
                  })},
    {"pair?",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[isType<Pair>(env, argv[0])].value_;
                  })},
    {"bool?",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[isType<Boolean>(env, argv[0])].value_;
                  })},
    {"eq?",
     create<Subr>(*this, nullptr, 2,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[argv[0] == argv[1]].value_;
                  })},
    {"len",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      if (not isType<Null>(env, argv[0])) {
                          FixNum::Rep length = 1;
                          Heap::Ptr<Pair> lst = checkedCast<Pair>(env, argv[0]);
                          while (not isType<Null>(env, lst.deref(env).getCdr())) {
                              length += 1;
                              lst = checkedCast<Pair>(env,
                                                      lst.deref(env).getCdr());
                          }
                          return create<FixNum>(env, length);
                      } else {
                          return create<FixNum>(env, FixNum::Rep(0));
                      }
                  })},
    {"map",
     create<Subr>(*this, nullptr, 2,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv)
                  -> ObjectPtr {
                      std::vector<ObjectPtr> paramVec;
                      if (not isType<Null>(env, argv[1])) {
                          auto subr = checkedCast<Subr>(env, argv[0]);
                          Heap::Ptr<Pair> lst = checkedCast<Pair>(env, argv[1]);
                          paramVec = {lst.deref(env).getCar()};
                          Local<Pair> result(env, create<Pair>(env,
                                                               subr.deref(env).
                                                               call(env, paramVec),
                                                               env.nullValue_.value_));
                          while (not isType<Null>(env, lst.deref(env).getCdr())) {
                              lst = checkedCast<Pair>(env, lst.deref(env).getCdr());
                              paramVec = {lst.deref(env).getCar()};
                              result = create<Pair>(env, subr.deref(env).call(env, paramVec),
                                                    result.get());
                          }
                          return result.get();
                      }
                      return env.nullValue_.value_;
                  })},
    {"help",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv)
                  -> ObjectPtr {
                      auto subr = checkedCast<Subr>(env, argv[0]);
                      if (auto doc = subr.deref(env).getDocstring()) {
                          return create<String>(env, doc);
                      }
                      return env.nullValue_.value_;
                  })}
}} {}

namespace TEST {

auto call(lisp::Environment& env,
          const std::string& fn,
          std::vector<ObjectPtr>& argv) {
    auto found = env.variables_.find(fn);
    if (found != env.variables_.end()) {
        auto subr = checkedCast<Subr>(env, found->second);
        return subr.deref(env).call(env, argv);
    }
    throw std::runtime_error("not found: " + std::string(fn));
}

template <Lexer::Token TOK>
Lexer::Token expect(Lexer& lexer) {
    const auto result = lexer.lex();
    if (result not_eq TOK) {
        throw std::runtime_error("bad input");
    }
    return result;
}

ObjectPtr evalExpr(Environment& env, Lexer& lexer) {
    expect<Lexer::Token::SYMBOL>(lexer);
    const std::string subrName = lexer.rdbuf();
    std::vector<ObjectPtr> params;
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::LPAREN:
            params.push_back(evalExpr(env, lexer));
            break;

        case Lexer::Token::SYMBOL: {
            auto found = env.variables_.find(lexer.rdbuf());
            if (found != env.variables_.end()) {
                params.push_back(found->second);
            } else {
                throw std::runtime_error("variable lookup failed!");
            }
            break;
        }

        case Lexer::Token::RPAREN:
            goto DO_CALL;

        case Lexer::Token::NONE:
            throw std::runtime_error("bad input");

        case Lexer::Token::FIXNUM: {
            auto num = FixNum::Rep(std::stoi(lexer.rdbuf()));
            params.push_back(create<FixNum>(env, num));
            break;
        }

        default:
            throw std::runtime_error("invalid token");
        }
    }
 DO_CALL:
    return call(env, subrName, params);
}

ObjectPtr eval(Environment& env, const std::string& code) {
    Lexer lexer(code);
    expect<Lexer::Token::LPAREN>(lexer);
    return evalExpr(env, lexer);
}

void display(Environment& env, ObjectPtr obj) {
    if (isType<Pair>(env, obj)) {
        auto pair = obj.cast<Pair>();
        std::cout << "(";
        display(env, pair.deref(env).getCar());
        std::cout << " ";
        display(env, pair.deref(env).getCdr());
        std::cout << ")";
    } else if (isType<FixNum>(env, obj)) {
        std::cout << obj.cast<FixNum>().deref(env).value();
    } else if (isType<Null>(env, obj)) {
        std::cout << "null";
    } else if (obj == env.booleans_[1].value_) {
        std::cout << "#t";
    } else if (obj == env.booleans_[0].value_) {
        std::cout << "#f";
    } else if (isType<String>(env, obj)) {
        std::cout << "\"" <<
            obj.cast<String>().deref(env).value() << "\"";
    }
}

} // TEST

} // lisp


////////////////////////////////////////////////////////////////////////////////
// TEST
////////////////////////////////////////////////////////////////////////////////

#include <iostream>

int main() {
    lisp::Environment env;
    while (true) {
        std::string input;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, input);
            lisp::TEST::display(env, lisp::TEST::eval(env, input));
            std::cout << std::endl;
            std::cout << "heap usage: " << env.heap_.size() << std::endl;
        }
    }
}
