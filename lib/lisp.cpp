#include <algorithm>
#include <fstream>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "lisp.hpp"
#include "listBuilder.hpp"

namespace lisp {

// FIXME: dolist isn't memory safe right now. Anything that causes an
// allocation of lisp objects could trigger the GC, and possibly move
// the list out from under our feet! So until it's fixed, don't use
// Environment::create or call any lisp functions from here.
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
    case typeId<Pair>(): {
        auto pair = obj.cast<Pair>();
        out << "(";
        print(env, pair->getCar(), out, true);
        if (not isType<Pair>(pair->getCdr())) {
            out << " . ";
            print(env, pair->getCdr(), out, true);
        } else {
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
        }
        out << ")";
    } break;

    case typeId<Integer>():
        out << obj.cast<Integer>()->value();
        break;

    case typeId<Null>():
        out << "null";
        break;

    case typeId<Boolean>():
        out << ((obj == env.getBool(true)) ? "true" : "false");
        break;

    case typeId<Function>():
        out << "lambda<" << obj.cast<Function>()->argCount() << ">";
        break;

    case typeId<String>():
        if (showQuotes) {
            out << '"' << *obj.cast<String>() << '"';
        } else {
            out << *obj.cast<String>();
        }
        break;

    case typeId<Float>():
        out << obj.cast<Float>()->value();
        break;

    case typeId<Complex>():
        out << obj.cast<Complex>()->value();
        break;

    case typeId<Symbol>(): {
        out << *obj.cast<Symbol>()->value();
    } break;

    case typeId<RawPointer>():
        out << obj.cast<RawPointer>()->value();
        break;

    case typeId<Character>():
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
    {"cons", "[car cdr] -> create a pair from car and cdr", 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
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
    {"collect-garbage", "[] -> run the gc", 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         env.getContext()->runGC(env);
         return env.getNull();
     }},
    {"sizeof", "[obj] -> number of bytes that obj occupies in memory", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         return env.create<Integer>(Integer::Rep(typeInfo(args[0]).size_));
     }},
    {"error", "[string] -> raise error string and terminate", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         throw std::runtime_error(
             checkedCast<String>(args[0])->value().toAscii());
     }},
    {"length", "[obj] -> get the length of a list or string", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeId<Pair>(): {
             Integer::Rep length = 0;
             if (not isType<Null>(args[0])) {
                 dolist(args[0], [&](ObjectPtr) { ++length; });
             }
             return env.create<Integer>(length);
         }
         case typeId<String>():
             return env.create<Integer>(
                 (Integer::Rep)args[0].cast<String>()->value().length());
         default:
             throw TypeError(args[0]->typeId(), "invalid type");
         }
     }},
#define LISP_TYPE_PROC(NAME, T)                                                \
    {                                                                          \
        NAME, nullptr, 1, [](Environment& env, const Arguments& args) {        \
            return env.getBool(isType<T>(args[0]));                            \
        }                                                                      \
    }
    LISP_TYPE_PROC("null?", Null),
    LISP_TYPE_PROC("pair?", Pair),
    LISP_TYPE_PROC("boolean?", Boolean),
    LISP_TYPE_PROC("integer?", Integer),
    LISP_TYPE_PROC("float?", Float),
    LISP_TYPE_PROC("complex?", Complex),
    LISP_TYPE_PROC("string?", String),
    LISP_TYPE_PROC("character?", Character),
    LISP_TYPE_PROC("symbol?", Symbol),
    LISP_TYPE_PROC("pointer?", RawPointer),
    LISP_TYPE_PROC("function?", Function),
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
         return env.getBool(args[0] == env.getBool(false));
     }},
    {"apply", nullptr, 2,
     [](Environment& env, const Arguments& args) {
         Arguments params(env);
         if (not isType<Null>(args[1])) {
             dolist(args[1], [&](ObjectPtr elem) { params.push(elem); });
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
         auto out = env.getGlobal("sys::stdout");
         auto write = env.getGlobal("fs::write");
         Arguments params(env);
         params.push(out);
         for (const auto& arg : args) {
             params.push(arg);
         }
         checkedCast<Function>(write)->call(params);
         return env.getNull();
     }},
    {"mod", "[integer] -> the modulus of integer", 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         return env.create<Integer>(checkedCast<Integer>(args[0])->value() %
                                    checkedCast<Integer>(args[0])->value());
     }},
    {"+", "[...] -> the result of adding each arg in ...", 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iSum = 0;
         Complex::Rep cSum;
         Float::Rep dSum = 0.0;
         for (size_t i = 0; i < args.count(); ++i) {
             switch (args[i]->typeId()) {
             case typeId<Integer>():
                 iSum += args[i].cast<Integer>()->value();
                 break;

             case typeId<Float>():
                 dSum += args[i].cast<Float>()->value();
                 break;

             case typeId<Complex>():
                 cSum += args[i].cast<Complex>()->value();
                 break;

             default:
                 throw TypeError(args[i]->typeId(), "not a number");
             }
         }
         if (cSum not_eq Complex::Rep{0.0, 0.0}) {
             return env.create<Complex>(cSum + dSum +
                                        static_cast<Float::Rep>(iSum));
         } else if (dSum) {
             return env.create<Float>(dSum + iSum);
         }
         return env.create<Integer>(iSum);
     }},
    {"-", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeId<Integer>():
             switch (args[1]->typeId()) {
             case typeId<Integer>():
                 return env.create<Integer>(args[0].cast<Integer>()->value() -
                                            args[1].cast<Integer>()->value());
             case typeId<Float>():
                 return env.create<Float>(args[0].cast<Integer>()->value() -
                                           args[1].cast<Float>()->value());
             default:
                 throw std::runtime_error("issue during subtraction");
             }

         case typeId<Float>():
             switch (args[1]->typeId()) {
             case typeId<Integer>():
                 return env.create<Float>(args[0].cast<Float>()->value() -
                                           args[1].cast<Integer>()->value());
             case typeId<Float>():
                 return env.create<Float>(args[0].cast<Float>()->value() -
                                           args[1].cast<Float>()->value());
             default:
                 throw std::runtime_error("issue during subtraction");
             }
             return env.create<Float>(args[0].cast<Float>()->value() -
                                       checkedCast<Float>(args[1])->value());

         case typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() -
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"*", "[...] -> the result of multiplying each arg in ...", 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         Integer::Rep iProd = 1;
         Float::Rep dProd = 1.0;
         Complex::Rep cProd(1.0);
         for (size_t i = 0; i < args.count(); ++i) {
             switch (args[i]->typeId()) {
             case typeId<Integer>():
                 iProd *= args[i].cast<Integer>()->value();
                 break;

             case typeId<Float>():
                 dProd *= args[i].cast<Float>()->value();
                 break;

             case typeId<Complex>():
                 cProd *= args[i].cast<Complex>()->value();
                 break;
             }
         }
         if (cProd not_eq Complex::Rep{1.0}) {
             return env.create<Complex>(cProd * dProd *
                                        static_cast<Float::Rep>(iProd));
         } else if (dProd not_eq 1.0) {
             return env.create<Float>(dProd * iProd);
         }
         return env.create<Integer>(iProd);
     }},
    {"/", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         // FIXME: this isn't as flexible as it should be
         switch (args[0]->typeId()) {
         case typeId<Integer>():
             return env.create<Integer>(args[0].cast<Integer>()->value() /
                                        checkedCast<Integer>(args[1])->value());

         case typeId<Float>():
             return env.create<Float>(args[0].cast<Float>()->value() /
                                       checkedCast<Float>(args[1])->value());

         case typeId<Complex>():
             return env.create<Complex>(args[0].cast<Complex>()->value() /
                                        checkedCast<Complex>(args[1])->value());
         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {">", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() >
                                checkedCast<Integer>(args[1])->value());

         case typeId<Float>():
             return env.getBool(args[0].cast<Float>()->value() >
                                checkedCast<Float>(args[1])->value());

         case typeId<Complex>():
             throw TypeError(typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"<", nullptr, 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeId<Integer>():
             return env.getBool(args[0].cast<Integer>()->value() <
                                checkedCast<Integer>(args[1])->value());

         case typeId<Float>():
             return env.getBool(args[0].cast<Float>()->value() <
                                checkedCast<Float>(args[1])->value());

         case typeId<Complex>():
             throw TypeError(typeId<Complex>(),
                             "Comparison unsupported for complex numbers. "
                             "Why not try comparing the magnitude?");

         default:
             throw TypeError(args[0]->typeId(), "not a number");
         }
     }},
    {"abs", "[number] -> absolute value of number", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         auto inp = args[0];
         switch (inp->typeId()) {
         case typeId<Integer>():
             if (inp.cast<Integer>()->value() > 0) {
                 return inp;
             } else {
                 const auto result = inp.cast<Integer>()->value() * -1;
                 return env.create<Integer>(result);
             }
             break;

         case typeId<Float>():
             if (inp.cast<Float>()->value() > 0.0) {
                 return inp;
             } else {
                 const auto result = std::abs(inp.cast<Float>()->value());
                 return env.create<Float>(result);
             }
             break;

         case typeId<Complex>(): {
             const auto result = std::abs(inp.cast<Complex>()->value());
             return env.create<Float>(result);
         }

         default:
             throw TypeError(inp->typeId(), "not a number");
         }
     }},
    {"complex", "[real imag] -> complex number from real + (b x imag)", 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         const auto real = checkedCast<Float>(args[0])->value();
         const auto imag = checkedCast<Float>(args[1])->value();
         return env.create<Complex>(Complex::Rep(real, imag));
     }},
    {"string-ref", "[str index] -> character at index in str", 2,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         return (*checkedCast<String>(
             args[0]))[checkedCast<Integer>(args[1])->value()];
     }},
    {"string", "[...] -> string constructed from all the args", 0,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         std::stringstream builder;
         for (auto& arg : args) {
             print(env, arg, builder);
         }
         return env.create<String>(builder.str());
     }},
    {"integer", "[string-or-float] -> integer conversion of the input", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeId<Integer>():
             return args[0];
         case typeId<String>(): {
             Integer::Rep i = std::stoi(args[0].cast<String>()->toAscii());
             return env.create<Integer>(i);
         }
         case typeId<Float>():
             return env.create<Integer>(
                 Integer::Rep(args[0].cast<Float>()->value()));
         default:
             throw ConversionError(args[0]->typeId(), typeId<Integer>());
         }
     }},
    {"float", "[integer-or-string] -> double precision float", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         switch (args[0]->typeId()) {
         case typeId<Float>():
             return args[0];
         case typeId<String>(): {
             Float::Rep d = std::stod(args[0].cast<String>()->toAscii());
             return env.create<Float>(d);
         }
         case typeId<Integer>():
             return env.create<Float>(
                 Float::Rep(args[0].cast<Integer>()->value()));
         default:
             throw ConversionError(args[0]->typeId(), typeId<Float>());
         }
     }},
    {"character", "[ascii-integer-value] -> character", 1,
     [](Environment& env, const Arguments& args) -> ObjectPtr {
         const auto val = checkedCast<Integer>(args[0])->value();
         if (val > -127 and val < 127) {
             return env.create<Character>(Character::Rep{(char)val, 0, 0, 0});
         }
         return env.getNull();
     }},
    {"load", "[file-path] -> load lisp code from file-path", 1,
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
