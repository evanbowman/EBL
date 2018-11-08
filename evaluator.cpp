#include "evaluator.hpp"
#include "lexer.hpp"
#include <iostream>

// I haven't written a bytecode compiler yet. This file includes an admittedly
// not-very-optimized lisp expression evaluator.
namespace lisp {

ObjectPtr call(Environment& env, const std::string& fnName,
               std::vector<ObjectPtr>& argv)
{
    auto fn = checkedCast<Function>(env, env.load(fnName));
    return fn->call(argv);
}

template <Lexer::Token TOK> Lexer::Token expect(Lexer& lexer, const char* ctx)
{
    const auto result = lexer.lex();
    if (result not_eq TOK) {
        throw std::runtime_error("bad input: " + std::string(ctx) +
                                 ", left: " + lexer.remaining());
    }
    return result;
}

ObjectPtr evalExpr(Environment& env, Lexer& lexer);

class UnexpectedCloseParen {
};

ObjectPtr evalValueOrExpr(Environment& env, Lexer& lexer)
{
    const auto tok = lexer.lex();
    switch (tok) {
    case Lexer::Token::LPAREN:
        return evalExpr(env, lexer);

    case Lexer::Token::SYMBOL:
        if (lexer.rdbuf() == "null") {
            return env.getNull();
        } else {
            return env.load(lexer.rdbuf());
        }

    case Lexer::Token::RPAREN:
        throw UnexpectedCloseParen{};

    case Lexer::Token::NONE:
        throw std::runtime_error("unexpected EOF");

    case Lexer::Token::INTEGER: {
        auto num = Integer::Rep(std::stoi(lexer.rdbuf()));
        return env.create<Integer>(num);
    }
    case Lexer::Token::STRING: {
        return env.create<String>(lexer.rdbuf().c_str(),
                                  lexer.rdbuf().length());
    }
    default:
        throw std::runtime_error("invalid token");
    }
}

ObjectPtr evalBinding(Environment& env, Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in eval binding");
    const std::string bindingName = lexer.rdbuf();
    env.store(bindingName, evalValueOrExpr(env, lexer));
    expect<Lexer::Token::RPAREN>(lexer, "in eval binding");
    return env.getNull();
}

ObjectPtr evalLet(Environment& env, Lexer& lexer)
{
    auto letEnv = env.derive();
    expect<Lexer::Token::LPAREN>(lexer, "in eval let");
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::LPAREN:
            evalBinding(*letEnv, lexer);
            break;

        default:
            throw std::runtime_error("invalid token");
        }
    }
BODY:
    ObjectPtr last = env.getNull();
    try {
        while (true) {
            last = evalValueOrExpr(*letEnv, lexer);
        }
    } catch (const UnexpectedCloseParen&) { // This is pretty nasty, FIXME
        return last;
    }
}

ObjectPtr evalLambda(Environment& env, Lexer& lexer);

ObjectPtr evalIf(Environment& env, Lexer& lexer)
{
    ObjectPtr cond = evalValueOrExpr(env, lexer);
    auto parsed = lexer.remaining();
    size_t parenBalance = 1;
    size_t i;
    for (i = 0; i < parsed.size(); ++i) {
        if (lexer.isOpenDelimiter(parsed[i])) {
            parenBalance++;
        } else if (lexer.isCloseDelimiter(parsed[i])) {
            parenBalance--;
            if (parenBalance == 0) {
                break;
            }
        }
    }
    parsed = parsed.substr(0, i);
    lexer.jumpPosition(i);
    expect<Lexer::Token::RPAREN>(lexer, "in eval if");
    if (cond == env.getBool(true)) {
        return eval(env, parsed);
    } else {
        return env.getBool(false);
    }
}

ObjectPtr evalExpr(Environment& env, Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in eval expr");
    const std::string fnName = lexer.rdbuf();
    // Special forms
    if (fnName == "def") {
        return evalBinding(env, lexer);
    } else if (fnName == "let") {
        return evalLet(env, lexer);
    } else if (fnName == "lambda") {
        return evalLambda(env, lexer);
    } else if (fnName == "if") {
        return evalIf(env, lexer);
    }
    std::vector<ObjectPtr> params;
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::LPAREN:
            params.push_back(evalExpr(env, lexer));
            break;

        case Lexer::Token::SYMBOL: {
            if (lexer.rdbuf() == "null") {
                params.push_back(env.getNull());
            } else {
                params.push_back(env.load(lexer.rdbuf()));
            }
            break;
        }

        case Lexer::Token::RPAREN:
            goto DO_CALL;

        case Lexer::Token::NONE:
            throw std::runtime_error("bad input");

        case Lexer::Token::INTEGER: {
            auto num = Integer::Rep(std::stoi(lexer.rdbuf()));
            params.push_back(env.create<Integer>(num));
            break;
        }

        case Lexer::Token::STRING: {
            params.push_back(env.create<String>(lexer.rdbuf().c_str(),
                                                lexer.rdbuf().length()));
            break;
        }

        default:
            throw std::runtime_error("invalid token");
        }
    }
DO_CALL:
    return call(env, fnName, params);
}

ObjectPtr eval(Environment& env, const std::string& code)
{
    Lexer lexer(code);
    ObjectPtr ret = env.getNull();
    while (lexer.hasText()) {
        ret = evalValueOrExpr(env, lexer);
    }
    return ret;
}

class UninterpretedLambda : public Function::Impl {
public:
    UninterpretedLambda(const std::vector<std::string> argNames,
                        std::string body)
        : argNames_(argNames), body_(body)
    {
    }

    ObjectPtr call(Environment& env, std::vector<ObjectPtr>& args) override
    {
        auto fnEnv = env.derive();
        for (size_t i = 0; i < argNames_.size(); ++i) {
            fnEnv->store(argNames_[i], args[i]);
        }
        return eval(*fnEnv, body_);
    }

private:
    std::vector<std::string> argNames_;
    std::string body_;
};

ObjectPtr evalLambda(Environment& env, Lexer& lexer)
{
    std::vector<std::string> argNames;
    expect<Lexer::Token::LPAREN>(lexer, "in eval lambda");
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::SYMBOL:
            argNames.push_back(lexer.rdbuf());
            break;

        default:
            throw std::runtime_error("invalid token in lambda argument list");
        }
    }
BODY:
    // Read text until reaching a paren balance of zero: "(" = +1, ")" = -1
    // We just want to store the whole function body as a string, and evaluate
    // it later when the lambda is actually invoked.
    std::string fnBody = lexer.remaining();
    size_t parenBalance = 1;
    size_t i;
    for (i = 0; i < fnBody.size(); ++i) {
        if (lexer.isOpenDelimiter(fnBody[i])) {
            parenBalance += 1;
        } else if (lexer.isCloseDelimiter(fnBody[i])) {
            parenBalance -= 1;
            if (parenBalance == 0) {
                goto DONE;
            }
        }
    }
DONE:
    fnBody = fnBody.substr(0, i);
    lexer.jumpPosition(i + 1);
    auto impl = std::unique_ptr<UninterpretedLambda>(
        new UninterpretedLambda(argNames, fnBody));
    // TODO: check for overflow? But you would have to be doing something really
    // crazy to define a function with more args that would fit in a long int.
    const Arguments::Count argc(argNames.size());
    return env.create<Function>("", argc, std::move(impl));
}

} // namespace lisp
