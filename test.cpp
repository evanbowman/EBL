#include "lisp.hpp"
#include "lexer.hpp"

#include <iostream>

auto call(lisp::Environment& env,
          const std::string& fn,
          std::vector<lisp::ObjectPtr>& argv) {
    auto found = env.topLevel_.find(fn);
    if (found != env.topLevel_.end()) {
        auto subr = lisp::checkedCast<lisp::Subr>(env, found->second);
        return subr.deref(env).call(env, argv);
    }
    throw std::runtime_error("not found: " + std::string(fn));
}

template <lisp::Lexer::Token TOK>
lisp::Lexer::Token expect(lisp::Lexer& lexer) {
    const auto result = lexer.lex();
    if (result not_eq TOK) {
        throw std::runtime_error("bad input");
    }
    return result;
}

lisp::ObjectPtr evalExpr(lisp::Environment& env, lisp::Lexer& lexer) {
    expect<lisp::Lexer::Token::SYMBOL>(lexer);
    const std::string subrName = lexer.rdbuf();
    std::vector<lisp::ObjectPtr> params;
    while (true) {
        switch (auto tok = lexer.lex()) {
        case lisp::Lexer::Token::LPAREN:
            params.push_back(evalExpr(env, lexer));
            break;

        case lisp::Lexer::Token::SYMBOL: {
            auto found = env.topLevel_.find(lexer.rdbuf());
            if (found != env.topLevel_.end()) {
                params.push_back(found->second);
            } else {
                throw std::runtime_error("variable lookup failed!");
            }
            break;
        }

        case lisp::Lexer::Token::RPAREN:
            goto DO_CALL;

        case lisp::Lexer::Token::NONE:
            throw std::runtime_error("bad input");

        case lisp::Lexer::Token::FIXNUM: {
            auto num = lisp::FixNum::Rep(std::stoi(lexer.rdbuf()));
            params.push_back(lisp::create<lisp::FixNum>(env, num));
            break;
        }

        default:
            throw std::runtime_error("invalid token");
        }
    }
 DO_CALL:
    return call(env, subrName, params);
}

lisp::ObjectPtr eval(lisp::Environment& env, const std::string& code) {
    lisp::Lexer lexer(code);
    expect<lisp::Lexer::Token::LPAREN>(lexer);
    return evalExpr(env, lexer);
}

void display(lisp::Environment& env, lisp::ObjectPtr obj) {
    if (lisp::isType<lisp::Pair>(env, obj)) {
        auto pair = obj.cast<lisp::Pair>();
        std::cout << "(";
        display(env, pair.deref(env).getCar());
        std::cout << " ";
        display(env, pair.deref(env).getCdr());
        std::cout << ")";
    } else if (lisp::isType<lisp::FixNum>(env, obj)) {
        std::cout << obj.cast<lisp::FixNum>().deref(env).value();
    } else if (lisp::isType<lisp::Null>(env, obj)) {
        std::cout << "null";
    } else if (obj == env.booleans_[1].get()) {
        std::cout << "#t";
    } else if (obj == env.booleans_[0].get()) {
        std::cout << "#f";
    }
}

int main() {
    lisp::Environment env;
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (not input.empty()) {
            try {
                display(env, eval(env, input));
                std::cout << std::endl;
                std::cout << "heap usage: " << env.heap_.size() << std::endl;
            } catch (const std::exception& ex) {
                std::cout << "Error: " << ex.what() << std::endl;
            }
        }
    }
}
