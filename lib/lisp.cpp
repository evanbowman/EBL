#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "lisp.hpp"
#include "listBuilder.hpp"

#ifdef __GNUC__
#define unlikely(COND) __builtin_expect(COND, false)
#else
#define unlikely(COND) COND
#endif

namespace lisp {

template <typename Proc> void dolist(ObjectPtr list, Proc&& proc)
{
    Heap::Ptr<Pair> current = checkedCast<Pair>(list);
    proc(current->getCar());
    while (not isType<Null>(current->getCdr())) {
        current = checkedCast<Pair>(current->getCdr());
        proc(current->getCar());
    }
}

void print(Environment& env, ObjectPtr obj, std::ostream& out,
           bool showQuotes = false)
{
    switch (obj->typeId()) {
    case typeInfo.typeId<Pair>(): {
        auto pair = obj.cast<Pair>();
        out << "(";
        print(env, pair->getCar(), out, true);
        while (true) {
            if (isType<Pair>(pair->getCdr())) {
                out << " ";
                print(env, pair->getCdr().cast<Pair>()->getCar(), out, true);
                pair = pair->getCdr().cast<Pair>();
            } else if (not isType<Null>(pair->getCdr())) {
                out << " ";
                print(env, pair->getCdr(), out, true);
                break;
            } else {
                break;
            }
        }
        out << ")";
    } break;

    case typeInfo.typeId<Integer>():
        out << obj.cast<Integer>()->value();
        break;

    case typeInfo.typeId<Null>():
        out << "null";
        break;

    case typeInfo.typeId<Boolean>():
        out << ((obj == env.getBool(true)) ? "true" : "false");
        break;

    case typeInfo.typeId<Function>():
        out << "lambda<" << obj.cast<Function>()->argCount() << ">";
        break;

    case typeInfo.typeId<String>():
        if (showQuotes) {
            out << '"' << *obj.cast<String>() << '"';
        } else {
            out << *obj.cast<String>();
        }
        break;

    case typeInfo.typeId<Double>():
        out << obj.cast<Double>()->value();
        break;

    case typeInfo.typeId<Complex>():
        out << obj.cast<Complex>()->value();
        break;

    case typeInfo.typeId<Symbol>(): {
        out << *obj.cast<Symbol>()->value();
    } break;

    case typeInfo.typeId<RawPointer>():
        out << obj.cast<RawPointer>()->value();
        break;

    case typeInfo.typeId<Character>():
        out << *obj.cast<Character>();
        break;

    default:
        out << "unknownObject";
        break;
    }
}

struct BuiltinFunctionInfo {
    const char* name;
    const char* docstring;
    size_t requiredArgs;
    CFunction impl;
};

static const BuiltinFunctionInfo builtins[] = {
    {"symbol", "[string] -> get symbol for string", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         const auto target = checkedCast<String>(args[0]);
         for (const auto& im : env.getContext()->immediates()) {
             if (isType<Symbol>(im)) {
                 if (im.cast<Symbol>()->value()->value() == target->value()) {
                     return im;
                 }
             }
         }
         auto symb = env.create<Symbol>(target);
         env.getContext()->immediates().push_back(symb);
         return symb;
     }},
    {"error", "[string] -> raise error string and terminate", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         throw std::runtime_error(
             checkedCast<String>(args[0])->value().toAscii());
     }},
    {"cons", "[car cdr] -> create a pair from car and cdr", 2,
     [](Environment& env, const Arguments& args) {
         return env.create<Pair>(args[0], args[1]);
     }},
    {"car", "[pair] -> get the first element of pair", 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(args[0])->getCar();
     }},
    {"cdr", "[pair] -> get the second element of pair", 1,
     [](Environment& env, const Arguments& args) {
         return checkedCast<Pair>(args[0])->getCdr();
     }},
    {"length", "[obj] -> get the length of a list or string", 1,
     [](Environment& env, const Arguments& args) {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Pair>(): {
             Integer::Rep length = 0;
             if (not isType<Null>(args[0])) {
                 dolist(args[0], [&](ObjectPtr) { ++length; });
             }
             return env.create<Integer>(length);
         }
         case typeInfo.typeId<String>():
             return env.create<Integer>(
                 (Integer::Rep)args[0].cast<String>()->value().length());
         default:
             throw TypeError(args[0]->typeId(), "invalid type");
         }
     }},
    {"map", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         if (not isType<Null>(args[1])) {
             auto fn = checkedCast<Function>(args[0]);
             // TODO: replace vectors with small size optimized
             // containers
             Arguments paramVec;
             std::vector<Heap::Ptr<Pair>> inputLists;
             for (size_t i = 1; i < args.size(); ++i) {
                 inputLists.push_back(checkedCast<Pair>(args[i]));
                 paramVec.push_back(inputLists.back()->getCar());
             }
             auto keepGoing = [&] {
                 for (auto lst : inputLists) {
                     if (isType<Null>(lst->getCdr())) {
                         return false;
                     }
                 }
                 return true;
             };
             ListBuilder builder(env, fn->call(paramVec));
             while (keepGoing()) {
                 paramVec.clear();
                 for (auto& lst : inputLists) {
                     lst = checkedCast<Pair>(lst->getCdr());
                     paramVec.push_back(lst->getCar());
                 }
                 builder.pushBack(fn->call(paramVec));
             }
             return builder.result();
         }
         return env.getNull();
     }},
    {"filter", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         LazyListBuilder builder(env);
         auto pred = checkedCast<Function>(args[0]);
         if (not isType<Null>(args[1])) {
             Arguments params;
             dolist(args[1], [&](ObjectPtr element) {
                 params.push_back(element);
                 if (pred->call(params) == env.getBool(true)) {
                     builder.pushBack(element);
                 }
                 params.clear();
             });
         }
         return builder.result();
     }},
    {"dolist", "[fn list] -> call fn on each element of list", 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(args[0]);
         if (not isType<Null>(args[1])) {
             Arguments params;
             dolist(args[1], [&](ObjectPtr element) {
                 params.push_back(element);
                 pred->call(params);
                 params.clear();
             });
         }
         return env.getNull();
     }},
    {"dotimes", "[fn n] -> call fn n times, passing each n as an arg", 2,
     [](Environment& env, const Arguments& args) {
         auto pred = checkedCast<Function>(args[0]);
         const auto times = checkedCast<Integer>(args[1])->value();
         Arguments params;
         for (Integer::Rep i = 0; i < times; ++i) {
             auto iObj = env.create<Integer>(i);
             params.push_back(iObj);
             pred->call(params);
             params.clear();
         }
         return env.getNull();
     }},
    {"null?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Null>(args[0]));
     }},
    {"pair?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Pair>(args[0]));
     }},
    {"bool?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Boolean>(args[0]));
     }},
    {"integer?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<Integer>(args[0]));
     }},
    {"string?", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         return env.getBool(isType<String>(args[0]));
     }},
    {"identical?", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         return env.getBool(args[0] == args[1]);
     }},
    {"equal?", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         EqualTo eq;
         return env.getBool(eq(args[0], args[1]));
     }},
    {"not", nullptr, 1,
     [](Environment& env, const Arguments& args) {
         // I was lazy, TODO simplify this
         if (args[0] == env.getBool(true)) {
             return env.getBool(false);
         } else if (args[0] == env.getBool(false)) {
             return env.getBool(true);
         }
         // TODO: raise error
         return env.getNull();
     }},
    {"apply", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         Arguments params;
         if (not isType<Null>(args[1])) {
             dolist(args[1], [&](ObjectPtr elem) { params.push_back(elem); });
         }
         auto fn = checkedCast<Function>(args[0]);
         return fn->call(params);
     }},
    {"help", "[fn] -> get the docstring for fn", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto fn = checkedCast<Function>(args[0]);
         auto doc = fn->getDocstring();
         if (not(doc == env.getNull())) {
             return doc;
         }
         return env.getNull();
     }},
    {"print", "[...] -> print each arg in ...", 0,
     [](Environment& env, const Arguments& args) {
         for (const auto& arg : args) {
             print(env, arg, std::cout);
         }
         return env.getNull();
     }},
    {"newline", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         std::cout << "\n";
         return env.getNull();
     }},
    {"space", nullptr, 0,
     [](Environment& env, const Arguments& args) {
         std::cout << " " << std::endl;
         return env.getNull();
     }},
    {"+", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iSum = 0;
         Complex::Rep cSum;
         Double::Rep dSum = 0.0;
         for (size_t i = 0; i < args.size(); ++i) {
             switch (args[i]->typeId()) {
             case typeInfo.typeId<Integer>():
                 iSum += args[i].cast<Integer>()->value();
                 break;

             case typeInfo.typeId<Double>():
                 dSum += args[i].cast<Double>()->value();
                 break;

             case typeInfo.typeId<Complex>():
                 cSum += args[i].cast<Complex>()->value();
                 break;

             default:
                 throw TypeError(args[i]->typeId(), "not a number");
             }
         }
         if (cSum not_eq Complex::Rep{0.0, 0.0}) {
             return env.create<Complex>(cSum + dSum +
                                        static_cast<Double::Rep>(iSum));
         } else if (dSum) {
             return env.create<Double>(dSum + iSum);
         }
         return env.create<Integer>(iSum);
     }},
    {"-", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.create<Integer>(args[0].cast<Integer>()->value() -
                                        checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(args[0].cast<Double>()->value() -
                                       checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() -
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"*", nullptr, 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iProd = 1;
         Double::Rep dProd = 1.0;
         Complex::Rep cProd(1.0);
         for (size_t i = 0; i < args.size(); ++i) {
             switch (args[i]->typeId()) {
             case typeInfo.typeId<Integer>():
                 iProd *= args[i].cast<Integer>()->value();
                 break;

             case typeInfo.typeId<Double>():
                 dProd *= args[i].cast<Double>()->value();
                 break;

             case typeInfo.typeId<Complex>():
                 cProd *= args[i].cast<Complex>()->value();
                 break;
             }
         }
         if (cProd not_eq Complex::Rep{1.0}) {
             return env.create<Complex>(cProd * dProd *
                                        static_cast<Double::Rep>(iProd));
         } else if (dProd not_eq 1.0) {
             return env.create<Double>(dProd * iProd);
         }
         return env.create<Integer>(iProd);
     }},
    {"/", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.create<Integer>(args[0].cast<Integer>()->value() /
                                        checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.create<Double>(args[0].cast<Double>()->value() /
                                       checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() /
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {">", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() >
                                checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() >
                                checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             throw TypeError(typeInfo.typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"<", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeInfo.typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() <
                                checkedCast<Integer>(args[1])->value());

         case typeInfo.typeId<Double>():
             return env.getBool(args[0].cast<Double>()->value() <
                                checkedCast<Double>(args[1])->value());

         case typeInfo.typeId<Complex>():
             throw TypeError(typeInfo.typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"abs", nullptr, 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto inp = args[0];
         switch (inp->typeId()) {
         case typeInfo.typeId<Integer>():
             if (inp.cast<Integer>()->value() > 0) {
                 return inp;
             } else {
                 const auto result = inp.cast<Integer>()->value() * -1;
                 return env.create<Integer>(result);
             }
             break;

         case typeInfo.typeId<Double>():
             if (inp.cast<Double>()->value() > 0.0) {
                 return inp;
             } else {
                 const auto result = std::abs(inp.cast<Double>()->value());
                 return env.create<Double>(result);
             }
             break;

         case typeInfo.typeId<Complex>(): {
             const auto result = std::abs(inp.cast<Complex>()->value());
             return env.create<Double>(result);
         }

         default:
             throw TypeError(inp->typeId(), "not a number");
         }
     }},
    {"complex", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         const auto real = checkedCast<Double>(args[0])->value();
         const auto imag = checkedCast<Double>(args[1])->value();
         return env.create<Complex>(Complex::Rep(real, imag));
     }},
    {"string-ref", "[str index] -> character at index in str", 2,
     [](Environment& env, const Arguments& args) {
         return (*checkedCast<String>(
             args[0]))[checkedCast<Integer>(args[1])->value()];
     }},
    // {"string", nullptr, 0,
    //  [](Environment& env, const Arguments& args) {
    //      std::stringstream builder;
    //      for (auto& arg : args) {
    //          print(env, arg, builder);
    //      }
    //      return env.create<String>(builder.str());
    //  }},
    {"load", "[file] -> load lisp code from a file", 1,
     [](Environment& env, const Arguments& args) {
         std::ifstream ifstream(
             checkedCast<String>(args[0])->value().toAscii());
         std::stringstream buffer;
         buffer << ifstream.rdbuf();
         return env.exec(buffer.str());
     }},
    {"eval", "[data] -> evaluate data as code", 1,
     [](Environment& env, const Arguments& args) {
         std::stringstream buffer;
         print(env, checkedCast<Pair>(args[0]), buffer);
         return env.exec(buffer.str());
     }},
    {"open-dll", "[dll] -> run dll in current environment", 1,
     [](Environment& env, const Arguments& args) {
         env.openDLL(checkedCast<String>(args[0])->value().toAscii());
         return env.getNull();
     }}};

void initBuiltins(Environment& env)
{
    std::for_each(
        std::begin(builtins), std::end(builtins),
        [&](const BuiltinFunctionInfo& info) {
            auto doc = env.getNull();
            if (info.docstring) {
                doc =
                    env.create<String>(info.docstring, strlen(info.docstring));
            }
            auto fn = env.create<Function>(doc, info.requiredArgs, info.impl);
            env.push(fn);
        });
}

std::vector<std::string> getBuiltinList()
{
    std::vector<std::string> ret;
    std::for_each(
        std::begin(builtins), std::end(builtins),
        [&](const BuiltinFunctionInfo& info) { ret.push_back(info.name); });
    return ret;
}

} // namespace lisp
