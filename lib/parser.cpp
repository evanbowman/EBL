#include "parser.hpp"
#include "lexer.hpp"
#include "utility.hpp"
#include <iostream>

namespace lisp {

template <Lexer::Token TOK> Lexer::Token expect(Lexer& lexer, const char* ctx)
{
    const auto result = lexer.lex();
    if (result not_eq TOK) {
        throw std::runtime_error("bad input: " + std::string(ctx) +
                                 ", left: " + lexer.remaining());
    }
    return result;
}

struct UnexpectedClosingParen {
};

ast::Ptr<ast::Expr> parseExpr(Lexer& lexer);

ast::Ptr<ast::Statement> parseStatement(Lexer& lexer)
{
    const auto tok = lexer.lex();
    switch (tok) {
    case Lexer::Token::LPAREN:
        return parseExpr(lexer);

    case Lexer::Token::SYMBOL:
        if (lexer.rdbuf() == "null") {
            return make_unique<ast::Null>();
        } else {
            auto ret = make_unique<ast::LValue>();
            ret->name_ = lexer.rdbuf();
            return ret;
        }

    case Lexer::Token::RPAREN:
        throw UnexpectedClosingParen{};

    case Lexer::Token::NONE:
        throw std::runtime_error("unexpected EOF in statement");

    case Lexer::Token::INTEGER: {
        auto ret = make_unique<ast::Integer>();
        ret->value_ = std::stoi(lexer.rdbuf());
        return ret;
    }

    case Lexer::Token::STRING: {
        auto ret = make_unique<ast::String>();
        ret->value_ = lexer.rdbuf();
        return ret;
    }

    default:
        throw std::runtime_error("invalid token in statement");
    }
}

ast::Ptr<ast::If> parseIf(Lexer& lexer)
{
    auto ifExpr = make_unique<ast::If>();
    ifExpr->condition_ = parseStatement(lexer);
    ifExpr->trueBranch_ = parseStatement(lexer);
    ifExpr->falseBranch_ = parseStatement(lexer);
    return ifExpr;
}

ast::Ptr<ast::Lambda> parseLambda(Lexer& lexer)
{
    auto lambda = make_unique<ast::Lambda>();
    expect<Lexer::Token::LPAREN>(lexer, "in parse lambda");
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::SYMBOL:
            lambda->argNames_.push_back(lexer.rdbuf());
            break;

        default:
            throw std::runtime_error("invalid token in lambda arg list");
        }
    }
BODY:
    // FIXME: this is kind of nasty
    try {
        while (true) {
            lambda->statements_.push_back(parseStatement(lexer));
        }
    } catch (const UnexpectedClosingParen&) {
        return lambda;
    }
}

ast::Ptr<ast::Def> parseDef(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in eval binding");
    auto def = make_unique<ast::Def>();
    def->name_ = lexer.rdbuf();
    def->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in eval binding");
    return def;
}

ast::Ptr<ast::Let::Binding> parseLetBinding(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in eval binding");
    auto binding = make_unique<ast::Let::Binding>();
    binding->name_ = lexer.rdbuf();
    binding->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in eval binding");
    return binding;
}

ast::Ptr<ast::Let> parseLet(Lexer& lexer)
{
    auto let = make_unique<ast::Let>();
    expect<Lexer::Token::LPAREN>(lexer, "in eval let");
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::LPAREN:
            let->bindings_.push_back(parseLetBinding(lexer));
            break;

        default:
            throw std::runtime_error("invalid token in let");
        }
    }
BODY:
    // FIXME: this is kind of nasty
    try {
        while (true) {
            let->statements_.push_back(parseStatement(lexer));
        }
    } catch (const UnexpectedClosingParen&) {
        return let;
    }
}

ast::Ptr<ast::Expr> parseExpr(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in eval expr");
    const std::string fnName = lexer.rdbuf();
    // special forms
    if (fnName == "def") {
        return parseDef(lexer);
    } else if (fnName == "lambda") {
        return parseLambda(lexer);
    } else if (fnName == "let") {
        return parseLet(lexer);
    } else if (fnName == "if") {
        return parseIf(lexer);
    }
    auto apply = make_unique<ast::Application>();
    apply->target_ = fnName;
    while (true) {
        switch (auto tok = lexer.lex()) {
        case Lexer::Token::LPAREN:
            apply->args_.push_back(parseExpr(lexer));
            break;

        case Lexer::Token::SYMBOL:
            if (lexer.rdbuf() == "null") {
                apply->args_.push_back(make_unique<ast::Null>());
            } else {
                auto lval = make_unique<ast::LValue>();
                lval->name_ = lexer.rdbuf();
                apply->args_.push_back(std::move(lval));
            }
            break;

        case Lexer::Token::RPAREN:
            goto DONE;

        case Lexer::Token::NONE:
            throw std::runtime_error("unexpected EOF in expr");

        case Lexer::Token::INTEGER: {
            auto param = make_unique<ast::Integer>();
            param->value_ = std::stoi(lexer.rdbuf());
            apply->args_.push_back(std::move(param));
            break;
        }

        case Lexer::Token::STRING: {
            auto param = make_unique<ast::String>();
            param->value_ = lexer.rdbuf();
            apply->args_.push_back(std::move(param));
            break;
        }

        default:
            throw std::runtime_error("invalid token in expr");
        }
    }
DONE:
    return apply;
}

ast::Ptr<ast::TopLevel> parse(const std::string& code)
{
    Lexer lexer(code);
    auto top = make_unique<ast::TopLevel>();
    while (lexer.hasText()) {
        top->statements_.push_back(parseStatement(lexer));
    }
    return top;
}

// ObjectPtr evalIf(Environment & env, Lexer & lexer)
// {
//     ObjectPtr cond = evalValueOrExpr(env, lexer);
//     auto parsed = lexer.remaining();
//     size_t parenBalance = 1;
//     size_t i;
//     for (i = 0; i < parsed.size(); ++i) {
//         if (lexer.isOpenDelimiter(parsed[i])) {
//             parenBalance++;
//         } else if (lexer.isCloseDelimiter(parsed[i])) {
//             parenBalance--;
//             if (parenBalance == 0) {
//                 break;
//             }
//         }
//     }
//     parsed = parsed.substr(0, i);
//     lexer.jumpPosition(i);
//     expect<Lexer::Token::RPAREN>(lexer, "in eval if");
//     if (cond == env.getBool(true)) {
//         return eval(env, parsed);
//     } else {
//         return env.getBool(false);
//     }
// }

} // namespace lisp
