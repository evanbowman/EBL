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

Environment::Environment() : heap_(8192),
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

} // lisp
