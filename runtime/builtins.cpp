#include <algorithm>
#include <fstream>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "ebl.hpp"
#include "listBuilder.hpp"

namespace ebl {

template <typename Proc> void dolist(Environment& env,
                                     ValuePtr list,
                                     Proc&& proc)
{
    Persistent<Pair> current(env, checkedCast<Pair>(list));
    Persistent<Value> car(env, env.getNull());
    car = current->getCar();
    proc(car);
    while (not isType<Null>(current->getCdr())) {
        current = checkedCast<Pair>(current->getCdr());
        car = current->getCar();
        proc(car);
    }
}

void failedToApply(Environment& env,
                   Function* function,
                   size_t suppliedArgs,
                   size_t expectedArgs)
{
    std::stringstream format;
    format << "failed to apply lambda\nsupplied argc: "
           << suppliedArgs
           << "\nexpected argc: "
           << expectedArgs
           << "\ndocstring: ";
    if (isType<String>(function->getDocstring())) {
        auto str = function->getDocstring().cast<String>();
        print(env, str, format, false);
    } else {
        format << "<Null>";
    }
    throw std::runtime_error(format.str());
}

void print(Environment& env, ValuePtr val, std::ostream& out,
           bool showQuotes = false)
{
    switch (val->typeId()) {
    case typeId<Pair>(): {
        auto pair = val.cast<Pair>();
        out << "(";
        print(env, pair->getCar(), out, true);
        if (not isType<Pair>(pair->getCdr())) {
            out << " . ";
            print(env, pair->getCdr(), out, true);
        } else {
            while (true) {
                if (isType<Pair>(pair->getCdr())) {
                    out << " ";
                    print(env, pair->getCdr().cast<Pair>()->getCar(), out,
                          true);
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

    case typeId<Box>():
        out << "Box{";
        print(env, val.cast<Box>()->get(), out);
        out << "}";
        break;

    case typeId<Integer>():
        out << val.cast<Integer>()->value();
        break;

    case typeId<Null>():
        out << "null";
        break;

    case typeId<Boolean>():
        out << ((val == env.getBool(true)) ? "true" : "false");
        break;

    case typeId<Function>():
        out << "lambda<" << val.cast<Function>()->argCount() << ">";
        break;

    case typeId<String>():
        if (showQuotes) {
            out << '"' << *val.cast<String>() << '"';
        } else {
            out << *val.cast<String>();
        }
        break;

    case typeId<Float>():
        out << val.cast<Float>()->value();
        break;

    case typeId<Complex>():
        out << val.cast<Complex>()->value();
        break;

    case typeId<Symbol>(): {
        out << *val.cast<Symbol>()->value();
    } break;

    case typeId<RawPointer>():
        out << val.cast<RawPointer>()->value();
        break;

    case typeId<Character>():
        out << *val.cast<Character>();
        break;

    default:
        out << "unknownValue";
        break;
    }
}

struct BuiltinFunctionInfo {
    const char* name;
    const char* docstring;
    size_t requiredArgs;
    CFunction impl;
};

static const BuiltinFunctionInfo builtins[] =
    {{"cons", "(cons car cdr) -> create a pair from car and cdr", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Pair>(args[0], args[1]);
      }},
     {"car", "(car pair) -> get the first element of pair", 1,
      [](Environment&, const Arguments& args) {
          return checkedCast<Pair>(args[0])->getCar();
      }},
     {"cdr", "(cdr pair) -> get the second element of pair", 1,
      [](Environment&, const Arguments& args) {
          return checkedCast<Pair>(args[0])->getCdr();
      }},
     {"box", "(box value) -> create box containing value", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Box>(args[0]);
      }},
     {"set-box!", "(set-box! box value) -> box with overwritten contents", 2,
      [](Environment&, const Arguments& args) -> ValuePtr {
          checkedCast<Box>(args[0])->set(args[1]);
          return args[0];
      }},
     {"unbox", "(unbox box) -> value stored in box", 1,
      [](Environment&, const Arguments& args) {
          return checkedCast<Box>(args[0])->get();
      }},
     {"symbol", "(symbol string) -> get symbol for string", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          const auto target = checkedCast<String>(args[0]);
          const auto symbLoc = storeI<Symbol>(*env.getContext(), target);
          return env.getContext()->immediates()[symbLoc];
      }},
     {"error", "(error string) -> raise error string and terminate", 1,
      [](Environment&, const Arguments& args) -> ValuePtr {
          throw std::runtime_error(
              checkedCast<String>(args[0])->value().toAscii());
      }},
     {"length", "(length val) -> get the length of a list or string", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          switch (args[0]->typeId()) {
          case typeId<Pair>(): {
              Integer::Rep length = 0;
              if (not isType<Null>(args[0])) {
                  dolist(env, args[0], [&](ValuePtr) { ++length; });
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
     {"get", "(get val index) -> get element at index in list or string", 2,
      [](Environment&, const Arguments& args) -> ValuePtr {
          switch (args[0]->typeId()) {
          case typeId<String>():
              return (
                  *args[0]
                       .cast<String>())[checkedCast<Integer>(args[1])->value()];

          case typeId<Pair>():
              return listRef(args[0].cast<Pair>(),
                             checkedCast<Integer>(args[1])->value());

          default:
              throw TypeError(args[0]->typeId(), "invalid type");
          }
      }},
#define EBL_TYPE_PROC(NAME, T)                                                \
    {                                                                          \
        NAME, nullptr, 1, [](Environment& env, const Arguments& args) {        \
            return env.getBool(isType<T>(args[0]));                            \
        }                                                                      \
    }
     EBL_TYPE_PROC("null?", Null),
     EBL_TYPE_PROC("pair?", Pair),
     EBL_TYPE_PROC("boolean?", Boolean),
     EBL_TYPE_PROC("integer?", Integer),
     EBL_TYPE_PROC("float?", Float),
     EBL_TYPE_PROC("complex?", Complex),
     EBL_TYPE_PROC("string?", String),
     EBL_TYPE_PROC("character?", Character),
     EBL_TYPE_PROC("symbol?", Symbol),
     EBL_TYPE_PROC("pointer?", RawPointer),
     EBL_TYPE_PROC("function?", Function),
     {"identical?", "(identical o1 o2) -> "
                    "true if o1 and o2 are the same value", 2,
      [](Environment& env, const Arguments& args) {
          return env.getBool(args[0] == args[1]);
      }},
     {"equal?", "(equal o1 o2) -> true if o1 and o2 have the same value", 2,
      [](Environment& env, const Arguments& args) {
          EqualTo eq;
          return env.getBool(eq(args[0], args[1]));
      }},
     {"not", "(not val) -> true if val is false, otherwise fales", 1,
      [](Environment& env, const Arguments& args) {
          return env.getBool(args[0] == env.getBool(false));
      }},
     {"apply", "(apply fn list) -> call fn with list as arguments", 2,
      [](Environment& env, const Arguments& args) {
          Arguments params(env);
          if (not isType<Null>(args[1])) {
              dolist(env, args[1], [&](ValuePtr elem) { params.push(elem); });
          }
          auto fn = checkedCast<Function>(args[0]);
          return fn->call(params);
      }},
     {"arity", "(arity fn) -> number of required arguments for fn", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Integer>((Integer::Rep)checkedCast<Function>(args[0])->argCount());
      }},
     {"help", "(help fn) -> get the docstring for fn", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          auto fn = checkedCast<Function>(args[0]);
          auto doc = fn->getDocstring();
          if (not(doc == env.getNull())) {
              return doc;
          }
          return env.getNull();
      }},
     {"print", "(print ...) -> print each arg in ... to sys::stdout", 0,
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
     {"clone", "(clone val) -> deep copy of val", 1,
      [](Environment& env, const Arguments& args) {
          return clone(env, args[0]);
      }},
     {"mod", "(mod integer) -> the modulus of integer", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Integer>(checkedCast<Integer>(args[0])->value() %
                                     checkedCast<Integer>(args[1])->value());
      }},
     {"f+", "(f+ f-1 f-2) -> add floats f-1 and f-2", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Float>(checkedCast<Float>(args[0])->value() +
                                   checkedCast<Float>(args[1])->value());
      }},
     {"f-", "(f- f-1 f-2) -> subtract floats f-1 and f-2", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Float>(checkedCast<Float>(args[0])->value() -
                                   checkedCast<Float>(args[1])->value());
      }},
     {"f*", "(f* f-1 f-2) -> multiply floats f-1 and f-2", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Float>(checkedCast<Float>(args[0])->value() *
                                   checkedCast<Float>(args[1])->value());
      }},
     {"f/", "(f/ f-1 f-2) -> divide floats f-1 and f-2", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Float>(checkedCast<Float>(args[0])->value() /
                                   checkedCast<Float>(args[1])->value());
      }},
     {"incr", "(incr int) -> int + 1", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Integer>(checkedCast<Integer>(args[0])->value() +
                                     1);
      }},
     {"decr", "(decr int) -> int - 1", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          return env.create<Integer>(checkedCast<Integer>(args[0])->value() -
                                     1);
      }},
     {"+", "(+ ...) -> the result of adding each arg in ...", 0,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
              return env.create<Complex>(
                  args[0].cast<Complex>()->value() -
                  checkedCast<Complex>(args[1])->value());
          default:
              throw TypeError(args[0]->typeId(), "not a number");
          }
      }},
     {"*", "(* ...) -> the result of multiplying each arg in ...", 0,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
      [](Environment& env, const Arguments& args) -> ValuePtr {
          // FIXME: this isn't as flexible as it should be
          switch (args[0]->typeId()) {
          case typeId<Integer>():
              return env.create<Integer>(
                  args[0].cast<Integer>()->value() /
                  checkedCast<Integer>(args[1])->value());

          case typeId<Float>():
              return env.create<Float>(args[0].cast<Float>()->value() /
                                       checkedCast<Float>(args[1])->value());

          case typeId<Complex>():
              return env.create<Complex>(
                  args[0].cast<Complex>()->value() /
                  checkedCast<Complex>(args[1])->value());
          default:
              throw TypeError(args[0]->typeId(), "not a number");
          }
      }},
     {">", nullptr, 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
     {"abs", "(abs number) -> absolute value of number", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
     {"complex", "(complex real imag) -> complex number from real + (b x imag)", 2,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          const auto real = checkedCast<Float>(args[0])->value();
          const auto imag = checkedCast<Float>(args[1])->value();
          return env.create<Complex>(Complex::Rep(real, imag));
      }},
     {"string", "(string ...) -> string constructed from all the args", 0,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          std::stringstream builder;
          for (auto& arg : args) {
              print(env, arg, builder);
          }
          return env.create<String>(builder.str());
      }},
     {"rstring", "(rstring ...) -> string constructed from args in reverse", 0,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          std::stringstream builder;
          for (long i = args.count() - 1; i > -1; --i) {
              print(env, args[i], builder);
          }
          return env.create<String>(builder.str());
      }},
     {"integer", "(integer val) -> integer conversion of the input", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
          case typeId<Character>():
              // FIXME!!!
              return env.create<Integer>(
                  Integer::Rep(args[0].cast<Character>()->value()[0]));
          default:
              throw ConversionError(args[0]->typeId(), typeId<Integer>());
          }
      }},
     {"float", "(float integer-or-string) -> double precision float", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
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
     {"character", "(character ascii-integer-value) -> character", 1,
      [](Environment& env, const Arguments& args) -> ValuePtr {
          const auto val = checkedCast<Integer>(args[0])->value();
          if (val > -127 and val < 127) {
              return env.create<Character>(
                  Character::Rep{{(char)val, 0, 0, 0}});
          }
          return env.getNull();
      }},
     {"load", "(load file-path) -> load ebl code from file-path", 1,
      [](Environment& env, const Arguments& args) {
          const auto path = checkedCast<String>(args[0])->value().toAscii();
          std::ifstream ifstream(path);
          if (not ifstream) {
              throw std::runtime_error("failed to load \'" + path + '\'');
          }
          std::stringstream buffer;
          buffer << ifstream.rdbuf();
          return env.exec(buffer.str());
      }},
     {"eval", "(eval data) -> evaluate data as code", 1,
      [](Environment& env, const Arguments& args) {
          std::stringstream buffer;
          print(env, checkedCast<Pair>(args[0]), buffer);
          return env.exec(buffer.str());
      }},
     {"eval-string", "(eval-string string) -> evaluate string as code", 1,
      [](Environment& env, const Arguments& args) {
          std::stringstream buffer;
          print(env, checkedCast<String>(args[0]), buffer, false);
          return env.exec(buffer.str());
      }},
     {"open-dll", "(open-dll dll-path) -> run dll in current environment", 1,
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
            env.setGlobal(info.name, fn);
        });
}

} // namespace ebl
