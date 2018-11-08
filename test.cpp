#include "lexer.hpp"
#include "lisp.hpp"

#include <iostream>

lisp::ObjectPtr call(lisp::Environment& env, const std::string& fn,
                     std::vector<lisp::ObjectPtr>& argv)
{
    auto subr = lisp::checkedCast<lisp::Subr>(env, env.load(fn));
    return subr->call(argv);
}

template <lisp::Lexer::Token TOK> lisp::Lexer::Token expect(lisp::Lexer& lexer)
{
    const auto result = lexer.lex();
    if (result not_eq TOK) {
        throw std::runtime_error("bad input");
    }
    return result;
}

lisp::ObjectPtr evalExpr(lisp::Environment& env, lisp::Lexer& lexer);

lisp::ObjectPtr evalDef(lisp::Environment& env, lisp::Lexer& lexer)
{
    expect<lisp::Lexer::Token::SYMBOL>(lexer);
    const std::string bindingName = lexer.rdbuf();
    expect<lisp::Lexer::Token::LPAREN>(lexer);
    env.store(bindingName, evalExpr(env, lexer));
    return env.getNull();
}

lisp::ObjectPtr evalExpr(lisp::Environment& env, lisp::Lexer& lexer)
{
    expect<lisp::Lexer::Token::SYMBOL>(lexer);
    const std::string subrName = lexer.rdbuf();
    if (subrName == "def") {
        return evalDef(env, lexer);
    }
    std::vector<lisp::ObjectPtr> params;
    while (true) {
        switch (auto tok = lexer.lex()) {
        case lisp::Lexer::Token::LPAREN:
            params.push_back(evalExpr(env, lexer));
            break;

        case lisp::Lexer::Token::SYMBOL: {
            params.push_back(env.load(lexer.rdbuf()));
            break;
        }

        case lisp::Lexer::Token::RPAREN:
            goto DO_CALL;

        case lisp::Lexer::Token::NONE:
            throw std::runtime_error("bad input");

        case lisp::Lexer::Token::INTEGER: {
            auto num = lisp::Integer::Rep(std::stoi(lexer.rdbuf()));
            params.push_back(env.create<lisp::Integer>(num));
            break;
        }

        default:
            throw std::runtime_error("invalid token");
        }
    }
DO_CALL:
    return call(env, subrName, params);
}

lisp::ObjectPtr eval(lisp::Environment& env, const std::string& code)
{
    lisp::Lexer lexer(code);
    expect<lisp::Lexer::Token::LPAREN>(lexer);
    return evalExpr(env, lexer);
}

void display(lisp::Environment& env, lisp::ObjectPtr obj)
{
    if (lisp::isType<lisp::Pair>(env, obj)) {
        auto pair = obj.cast<lisp::Pair>();
        std::cout << "(";
        display(env, pair->getCar());
        std::cout << " . ";
        display(env, pair->getCdr());
        std::cout << ")";
    } else if (lisp::isType<lisp::Integer>(env, obj)) {
        std::cout << obj.cast<lisp::Integer>()->value();
    } else if (lisp::isType<lisp::Null>(env, obj)) {
        std::cout << "null";
    } else if (obj == env.getBool(true)) {
        std::cout << "#t";
    } else if (obj == env.getBool(false)) {
        std::cout << "#f";
    }
}

// The bytecode compiler isn't finished yet.. for now, just read all the
// contents of a function into custom impl of Subr, and evaluate it when
// called.
class UninterpretedSubrImpl : public lisp::Subr::Impl {
public:
    UninterpretedSubrImpl(const std::vector<std::string>& argNames,
                          const std::string& body)
        : argNames_(argNames), body_(body)
    {
    }

    lisp::ObjectPtr call(lisp::Environment& env,
                         std::vector<lisp::ObjectPtr>& params)
    {
        // TODO..
    }

private:
    std::vector<std::string> argNames_;
    std::string body_;
};

int main()
{
    lisp::Context context({});
    std::string input;
    auto env = context.topLevel();
    env->store("quit", lisp::makeCSubr(*env,
                                       [](lisp::Environment& env,
                                          const lisp::Arguments& args) {
                                           exit(0);
                                           return env.getNull();
                                       },
                                       nullptr, 0));
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                display(*env, eval(*env, input));
                std::cout << std::endl;
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
