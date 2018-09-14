#include <functional>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include "lisp.hpp"
#include "lexer.hpp"
#include <chrono>

namespace lisp {

template <typename Proc>
void dolist(Environment& env, ObjectPtr list, Proc&& proc) {
    Heap::Ptr<Pair> current = checkedCast<Pair>(env, list);
    proc(current.deref(env).getCar());
    while (not isType<Null>(env, current.deref(env).getCdr())) {
        current = checkedCast<Pair>(env, current.deref(env).getCdr());
        proc(current.deref(env).getCar());
    }
}

Environment::Environment() : heap_(4096),
                     booleans_{{create<Boolean>(*this, false)},
                               {create<Boolean>(*this, true)}},
                     nullValue_{create<Null>(*this)},
                     topLevel_{{
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
                                                   env.nullValue_.get());
                          Local<Pair> list(env, tail);
                          for (Subr::ArgCount pos = argc - 2; pos > -1; --pos) {
                              list = create<Pair>(env, argv[pos], list.get());
                          }
                          return list.get();
                      } else {
                          return env.nullValue_.get();
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
                      return env.booleans_[isType<Null>(env, argv[0])].get();
                  })},
    {"pair?",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[isType<Pair>(env, argv[0])].get();
                  })},
    {"bool?",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[isType<Boolean>(env, argv[0])].get();
                  })},
    {"eq?",
     create<Subr>(*this, nullptr, 2,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return env.booleans_[argv[0] == argv[1]].get();
                  })},
    {"identity",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      return argv[0];
                  })},
    {"length",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      FixNum::Rep length = 0;
                      if (not isType<Null>(env, argv[0])) {
                          dolist(env, argv[0], [&](ObjectPtr) { ++length; });
                      }
                      return create<FixNum>(env, length);
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
                          Local<Pair> result(env,
                                             create<Pair>(env,
                                                          subr.deref(env).
                                                          call(env, paramVec),
                                                          env.nullValue_.get()));
                          auto current = result.get();
                          while (not isType<Null>(env, lst.deref(env).getCdr())) {
                              lst = checkedCast<Pair>(env, lst.deref(env).getCdr());
                              paramVec = {lst.deref(env).getCar()};
                              auto next = create<Pair>(env, subr.deref(env).call(env, paramVec),
                                                       env.nullValue_.get());
                              current.deref(env).setCdr(next);
                              current = next;
                          }
                          return result.get();
                      }
                      return env.nullValue_.get();
                  })},
    {"apply",
     create<Subr>(*this, nullptr, 2,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      std::vector<ObjectPtr> params;
                      if (not isType<Null>(env, argv[1])) {
                          dolist(env, argv[1], [&](ObjectPtr elem) {
                                                   params.push_back(elem);
                                               });
                      }
                      auto subr = checkedCast<Subr>(env, argv[0]);
                      return subr.deref(env).call(env, params);
                  })},
    {"+",
     create<Subr>(*this, nullptr, 0,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      FixNum::Rep result = 0;
                      for (Subr::ArgCount i = 0; i < argc; ++i) {
                          result += checkedCast<FixNum>(env, argv[i])
                              .deref(env).value();
                      }
                      return create<FixNum>(env, result);
                  })},
    {"*",
     create<Subr>(*this, nullptr, 0,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv) {
                      FixNum::Rep result = 1;
                      for (Subr::ArgCount i = 0; i < argc; ++i) {
                          result *= checkedCast<FixNum>(env, argv[i])
                              .deref(env).value();
                      }
                      return create<FixNum>(env, result);
                  })},
    {"help",
     create<Subr>(*this, nullptr, 1,
                  [](Environment& env, Subr::ArgCount argc, Subr::ArgVec argv)
                  -> ObjectPtr {
                      auto subr = checkedCast<Subr>(env, argv[0]);
                      if (auto doc = subr.deref(env).getDocstring()) {
                          std::cout << doc << std::endl;
                      }
                      return env.nullValue_.get();
                  })}
}} {}

namespace TEST {

auto call(lisp::Environment& env,
          const std::string& fn,
          std::vector<ObjectPtr>& argv) {
    auto found = env.topLevel_.find(fn);
    if (found != env.topLevel_.end()) {
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
            auto found = env.topLevel_.find(lexer.rdbuf());
            if (found != env.topLevel_.end()) {
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
    } else if (obj == env.booleans_[1].get()) {
        std::cout << "#t";
    } else if (obj == env.booleans_[0].get()) {
        std::cout << "#f";
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
            if (not input.empty()) {
                try {
                    lisp::TEST::display(env, lisp::TEST::eval(env, input));
                    std::cout << std::endl;
                    std::cout << "heap usage: " << env.heap_.size() << std::endl;
                } catch (const std::exception& ex) {
                    std::cout << "Error: " << ex.what() << std::endl;
                }
            }
        }
    }
}
