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
        return std::move(ret);
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
        return std::move(ret);
    }

    case Lexer::Token::FLOAT: {
        auto ret = make_unique<ast::Double>();
        ret->value_ = std::stod(lexer.rdbuf());
        return std::move(ret);
    }

    case Lexer::Token::STRING: {
        auto ret = make_unique<ast::String>();
        ret->value_ = lexer.rdbuf();
        return std::move(ret);
    }

    default:
        throw std::runtime_error("invalid token in statement");
    }
}

void parseStatementList(Lexer& l, std::vector<ast::Ptr<ast::Statement>>& out)
{
    try {
        while (true) {
            out.push_back(parseStatement(l));
        }
    } catch (const UnexpectedClosingParen&) {
        return;
    }
}

ast::Ptr<ast::Begin> parseBegin(Lexer& lexer)
{
    auto begin = make_unique<ast::Begin>();
    parseStatementList(lexer, begin->statements_);
    return begin;
}

ast::Ptr<ast::If> parseIf(Lexer& lexer)
{
    auto ifExpr = make_unique<ast::If>();
    ifExpr->condition_ = parseStatement(lexer);
    ifExpr->trueBranch_ = parseStatement(lexer);
    try {
        ifExpr->falseBranch_ = parseStatement(lexer);
    } catch (const UnexpectedClosingParen&) {
        ifExpr->falseBranch_ = make_unique<ast::Null>();
        return ifExpr;
    }
    expect<Lexer::Token::RPAREN>(lexer, "in parse if");
    return ifExpr;
}


ast::Ptr<ast::Or> parseOr(Lexer& lexer)
{
    auto _or = make_unique<ast::Or>();
    parseStatementList(lexer, _or->statements_);
    return _or;
}


ast::Ptr<ast::And> parseAnd(Lexer& lexer)
{
    auto _and = make_unique<ast::And>();
    parseStatementList(lexer, _and->statements_);
    return _and;
}


ast::Ptr<ast::Lambda> parseLambda(Lexer& lexer)
{
    std::vector<std::string> argNames;
    expect<Lexer::Token::LPAREN>(lexer, "in parse lambda");
    while (true) {
        switch (lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::SYMBOL:
            argNames.push_back(lexer.rdbuf());
            break;

        default:
            throw std::runtime_error("invalid token in lambda arg list");
        }
    }
BODY:
    auto lambda = [&]() -> ast::Ptr<ast::Lambda> {
        if (not argNames.empty() and argNames.back() == "...") {
            return make_unique<ast::VariadicLambda>();
        }
        return make_unique<ast::Lambda>();
    }();
    lambda->argNames_ = std::move(argNames);

    // FIXME: this is kind of nasty
    try {
        lambda->statements_.push_back(parseStatement(lexer));
        auto second = parseStatement(lexer);
        // We can only register a docstring after making sure that at least one
        // statement follows the string, otherwise we might eat a string return
        // value.
        if (auto str =
                dynamic_cast<ast::String*>(lambda->statements_.front().get())) {
            lambda->docstring_ = str->value_;
            lambda->statements_.pop_back();
        }
        lambda->statements_.push_back(std::move(second));
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

ast::Ptr<ast::Set> parseSet(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in parse set");
    auto set = make_unique<ast::Set>();
    set->name_ = lexer.rdbuf();
    set->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in parse set");
    return set;
}

ast::Ptr<ast::Import> parseImport(Lexer& lexer)
{
    expect<Lexer::Token::STRING>(lexer, "in parse import");
    auto import = make_unique<ast::Import>();
    import->name_ = lexer.rdbuf();
    expect<Lexer::Token::RPAREN>(lexer, "in parse import");
    return import;
}


ast::Ptr<ast::Cond> parseCond(Lexer& lexer)
{
    auto cond = make_unique<ast::Cond>();
    while (true) {
        auto tok = lexer.lex();
        if (tok == Lexer::Token::LPAREN) {
            std::vector<ast::Ptr<ast::Statement>> statements;
            parseStatementList(lexer, statements);
            if (statements.empty()) {
                throw std::runtime_error("empty cond case!");
            }
            ast::Cond::Case c;
            c.condition_ = std::move(statements.front());
            for (size_t i = 1; i < statements.size(); ++i) {
                c.body_.push_back(std::move(statements[i]));
            }
            cond->cases_.push_back(std::move(c));
        } else if (tok == Lexer::Token::RPAREN) {
            return cond;
        } else {
            throw std::runtime_error("unexpected token in cond");
        }
    }
}


ast::Ptr<ast::Let> parseLet(Lexer& lexer)
{
    auto let = make_unique<ast::Let>();
    expect<Lexer::Token::LPAREN>(lexer, "in parse let");
    while (true) {
        switch (lexer.lex()) {
        case Lexer::Token::RPAREN:
            goto BODY;

        case Lexer::Token::LPAREN: {
            expect<Lexer::Token::SYMBOL>(lexer, "in parse binding");
            ast::Let::Binding binding;
            binding.name_ = lexer.rdbuf();
            binding.value_ = parseStatement(lexer);
            expect<Lexer::Token::RPAREN>(lexer, "in parse binding");
            let->bindings_.push_back(std::move(binding));
            break;
        }

        default:
            throw std::runtime_error("invalid token in let");
        }
    }
BODY:
    parseStatementList(lexer, let->statements_);
    return let;
}

ast::Ptr<ast::Expr> parseExpr(Lexer& lexer)
{
    auto apply = make_unique<ast::Application>();
    const auto tok = lexer.lex();
    if (tok == Lexer::Token::SYMBOL) {
        // special forms
        const std::string symb = lexer.rdbuf();
        if (symb == "def") {
            return parseDef(lexer);
        } else if (symb == "lambda") {
            return parseLambda(lexer);
        } else if (symb == "let") {
            return parseLet(lexer);
        } else if (symb == "if") {
            return parseIf(lexer);
        } else if (symb == "cond") {
            return parseCond(lexer);
        } else if (symb == "begin") {
            return parseBegin(lexer);
        } else if (symb == "import") {
            return parseImport(lexer);
        } else if (symb == "or") {
            return parseOr(lexer);
        } else if (symb == "and") {
            return parseAnd(lexer);
        } else if (symb == "set") {
            return parseSet(lexer);
        } else {
            apply->toApply_ = parseValue(lexer.rdbuf());
        }
    } else if (tok == Lexer::Token::LPAREN) {
        apply->toApply_ = parseExpr(lexer);
    } else {
        throw std::runtime_error("failed to parse expr");
    }
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
        parseStatementList(lexer, top->statements_);
    } catch (const UnexpectedEOF&) {
        return top;
    }
    return top;
}

} // namespace lisp
