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

struct UnexpectedEOF {
};

ast::Ptr<ast::Expr> parseExpr(Lexer& lexer);

ast::Ptr<ast::Value> parseValue(const std::string& name)
{
    if (name == "null") {
        return make_unique<ast::Null>();
    } else if (name == "true") {
        return make_unique<ast::True>();
    } else if (name == "false") {
        return make_unique<ast::False>();
    } else {
        auto ret = make_unique<ast::LValue>();
        ret->name_ = name;
        return ret;
    }
}

ast::Ptr<ast::Statement> parseStatement(Lexer& lexer)
{
    const auto tok = lexer.lex();
    switch (tok) {
    case Lexer::Token::LPAREN:
        return parseExpr(lexer);

    case Lexer::Token::SYMBOL:
        return parseValue(lexer.rdbuf());

    case Lexer::Token::RPAREN:
        throw UnexpectedClosingParen{};

    case Lexer::Token::NONE:
        throw UnexpectedEOF{};

    case Lexer::Token::INTEGER: {
        auto ret = make_unique<ast::Integer>();
        ret->value_ = std::stoi(lexer.rdbuf());
        return ret;
    }

    case Lexer::Token::FLOAT: {
        auto ret = make_unique<ast::Double>();
        ret->value_ = std::stod(lexer.rdbuf());
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

ast::Ptr<ast::Begin> parseBegin(Lexer& lexer)
{
    auto begin = make_unique<ast::Begin>();
    try {
        while (true) {
            begin->statements_.push_back(parseStatement(lexer));
        }
    } catch (const UnexpectedClosingParen&) {
        return begin;
    }
}

ast::Ptr<ast::If> parseIf(Lexer& lexer)
{
    auto ifExpr = make_unique<ast::If>();
    ifExpr->condition_ = parseStatement(lexer);
    ifExpr->trueBranch_ = parseStatement(lexer);
    ifExpr->falseBranch_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in parse if");
    return ifExpr;
}

ast::Ptr<ast::Lambda> parseLambda(Lexer& lexer)
{
    auto lambda = make_unique<ast::Lambda>();
    expect<Lexer::Token::LPAREN>(lexer, "in parse lambda");
    while (true) {
        switch (lexer.lex()) {
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
    expect<Lexer::Token::SYMBOL>(lexer, "in parse binding");
    auto def = make_unique<ast::Def>();
    def->name_ = lexer.rdbuf();
    def->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in parse binding");
    return def;
}

ast::Ptr<ast::Import> parseImport(Lexer& lexer)
{
    expect<Lexer::Token::STRING>(lexer, "in parse import");
    auto import = make_unique<ast::Import>();
    import->name_ = lexer.rdbuf();
    expect<Lexer::Token::RPAREN>(lexer, "in parse import");
    return import;
}

ast::Ptr<ast::Let::Binding> parseLetBinding(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in parse binding");
    auto binding = make_unique<ast::Let::Binding>();
    binding->name_ = lexer.rdbuf();
    binding->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in parse binding");
    return binding;
}

ast::Ptr<ast::Let> parseLet(Lexer& lexer)
{
    auto let = make_unique<ast::Let>();
    expect<Lexer::Token::LPAREN>(lexer, "in parse let");
    while (true) {
        switch (lexer.lex()) {
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
    expect<Lexer::Token::SYMBOL>(lexer, "in parse expr");
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
    } else if (fnName == "begin") {
        return parseBegin(lexer);
    } else if (fnName == "import") {
        return parseImport(lexer);
    }
    auto apply = make_unique<ast::Application>();
    apply->target_ = fnName;
    while (true) {
        switch (lexer.lex()) {
        case Lexer::Token::LPAREN:
            apply->args_.push_back(parseExpr(lexer));
            break;

        case Lexer::Token::SYMBOL:
            apply->args_.push_back(parseValue(lexer.rdbuf()));
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

        case Lexer::Token::FLOAT: {
            auto param = make_unique<ast::Double>();
            param->value_ = std::stod(lexer.rdbuf());
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
    try {
        while (true) {
            top->statements_.push_back(parseStatement(lexer));
        }
    } catch (const UnexpectedClosingParen&) {
        return top;
    } catch (const UnexpectedEOF&) {
        return top;
    }
    return top;
}

} // namespace lisp
