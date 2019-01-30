#include "parser.hpp"
#include "lexer.hpp"
#include "utility.hpp"

// This parser could be better... I'm considering rewriting the
// compiler in EBL anyway, so I haven't decided whether to clean this
// up yet.

namespace ebl {

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

ast::Ptr<ast::Literal> parseLiteral(Lexer::Token tok, const std::string& strval)
{
    switch (tok) {
    case Lexer::Token::INTEGER: {
        auto ret = make_unique<ast::Integer>();
        ret->value_ = std::stoi(strval);
        return ret;
    }
    case Lexer::Token::SYMBOL: {
        auto ret = make_unique<ast::Symbol>();
        ret->value_ = strval;
        return ret;
    }
    case Lexer::Token::STRING: {
        auto ret = make_unique<ast::String>();
        ret->value_ = strval;
        return ret;
    }
    case Lexer::Token::FLOAT: {
        auto ret = make_unique<ast::Float>();
        ret->value_ = std::stod(strval);
        return ret;
    }
    default:
        throw std::runtime_error("invalid literal");
    }
}

ast::Ptr<ast::Literal> parseListLiteral(Lexer& lexer)
{
    auto list = make_unique<ast::List>();
    while (true) {
        switch (const auto tok = lexer.lex()) {
        case Lexer::Token::LPAREN:
            list->contents_.push_back(parseListLiteral(lexer));
            break;

        case Lexer::Token::STRING:
        case Lexer::Token::SYMBOL:
        case Lexer::Token::INTEGER:
        case Lexer::Token::FLOAT:
            list->contents_.push_back(parseLiteral(tok, lexer.rdbuf()));
            break;

        case Lexer::Token::DOT: {
            auto pair = make_unique<ast::Pair>();
            if (list->contents_.size() > 1) {
                throw std::runtime_error("list has too many elements to be a "
                                         "dotted pair!");
            }
            pair->first_ = std::move(list->contents_.front());
            switch (const auto tok = lexer.lex()) {
            case Lexer::Token::INTEGER:
            case Lexer::Token::SYMBOL:
            case Lexer::Token::STRING:
            case Lexer::Token::FLOAT:
                pair->second_ = parseLiteral(tok, lexer.rdbuf());
                break;

            default:
                throw std::runtime_error("invalid token in dotted pair");
            }
            expect<Lexer::Token::RPAREN>(lexer, "in parse dotted pair");
            return ast::Ptr<ast::Literal>(pair.release());
        } break;

        case Lexer::Token::RPAREN:
            return ast::Ptr<ast::Literal>(list.release());

        default:
            throw std::runtime_error(
                "TODO: list literal parsing doesn\' support all types");
        }
    }
}

ast::Ptr<ast::Statement> parseQuoted(Lexer& lexer)
{
    const auto tok = lexer.lex();
    switch (tok) {
    case Lexer::Token::LPAREN:
        return parseListLiteral(lexer);

    case Lexer::Token::STRING:
    case Lexer::Token::SYMBOL:
    case Lexer::Token::INTEGER:
    case Lexer::Token::FLOAT:
        return parseLiteral(tok, lexer.rdbuf());

    default:
        throw std::runtime_error("TODO: support non-list quoted values");
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
        auto ret = make_unique<ast::Float>();
        ret->value_ = std::stod(lexer.rdbuf());
        return std::move(ret);
    }

    case Lexer::Token::QUOTE:
        return parseQuoted(lexer);

    case Lexer::Token::STRING: {
        auto ret = make_unique<ast::String>();
        ret->value_ = lexer.rdbuf();
        return std::move(ret);
    }

    case Lexer::Token::CHAR: {
        auto ret = make_unique<ast::Character>();
        if (utf8Len(lexer.rdbuf().c_str(), lexer.rdbuf().length()) not_eq 1) {
            throw std::runtime_error("char literal " + lexer.rdbuf() +
                                     " contains multiple glyphs!");
        }
        for (size_t i = 0; i < lexer.rdbuf().length(); ++i) {
            ret->value_[i] = lexer.rdbuf()[i];
        }
        return ret;
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


ast::Ptr<ast::Recur> parseRecur(Lexer& lexer)
{
    auto recur = make_unique<ast::Recur>();
    parseStatementList(lexer, recur->args_);
    return recur;
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

ast::Ptr<ast::Def> parseDefMut(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in parse binding");
    auto defmut = make_unique<ast::DefMut>();
    defmut->name_ = lexer.rdbuf();
    defmut->value_ = parseStatement(lexer);
    expect<Lexer::Token::RPAREN>(lexer, "in parse binding");
    return ast::Ptr<ast::Def>(defmut.release());
}

ast::Ptr<ast::Def> parseDefn(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in parse defn");
    auto def = make_unique<ast::Def>();
    def->name_ = lexer.rdbuf();
    def->value_ = parseLambda(lexer);
    // NOTE: lambda parsing completed the closing paren.
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

ast::Ptr<ast::Namespace> parseNamespace(Lexer& lexer)
{
    expect<Lexer::Token::SYMBOL>(lexer, "in parse namespace");
    auto ns = make_unique<ast::Namespace>();
    ns->name_ = lexer.rdbuf();
    parseStatementList(lexer, ns->statements_);
    return ns;
}


ast::Ptr<ast::If> parseCond(Lexer& lexer)
{
    struct Case {
        ast::Ptr<ast::Statement> condition_;
        std::vector<ast::Ptr<ast::Statement>> body_;
    };
    std::vector<Case> cases;
    while (true) {
        auto tok = lexer.lex();
        if (tok == Lexer::Token::LPAREN) {
            std::vector<ast::Ptr<ast::Statement>> statements;
            parseStatementList(lexer, statements);
            if (statements.empty()) {
                throw std::runtime_error("empty cond case!");
            }
            Case c;
            c.condition_ = std::move(statements.front());
            for (size_t i = 1; i < statements.size(); ++i) {
                c.body_.push_back(std::move(statements[i]));
            }
            cases.push_back(std::move(c));
        } else if (tok == Lexer::Token::RPAREN) {
            if (cases.empty()) {
                throw std::runtime_error("cond contains no expressions!");
            }
            // Build a chain of nested ifs for the branch arms.
            auto exprRoot = make_unique<ast::If>();
            ast::If* current = exprRoot.get();
            current->condition_ = make_unique<ast::True>();
            for (size_t i = 0; i < cases.size(); ++i) {
                auto detachedCond = std::move(current->condition_);
                current->condition_ = std::move(cases[i].condition_);
                auto begin = make_unique<ast::Begin>();
                begin->statements_ = std::move(cases[i].body_);
                current->trueBranch_ = std::move(begin);
                if (i == cases.size() - 1) {
                    current->falseBranch_ = make_unique<ast::Null>();
                } else {
                    current->falseBranch_ = make_unique<ast::If>();
                    current = (ast::If*)current->falseBranch_.get();
                    current->condition_ = std::move(detachedCond);
                }
            }
            return exprRoot;
        } else {
            throw std::runtime_error("unexpected token in cond");
        }
    }
}


ast::Ptr<ast::Let> parseLetImpl(Lexer& lexer, ast::Ptr<ast::Let> val)
{
    auto let = std::move(val);
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


ast::Ptr<ast::Let> parseLet(Lexer& lexer)
{
    return parseLetImpl(lexer, make_unique<ast::Let>());
}


ast::Ptr<ast::Let> parseLetMut(Lexer& lexer)
{
    return parseLetImpl(lexer, std::unique_ptr<ast::Let>(new ast::LetMut));
}


ast::Ptr<ast::Lambda> makeDelay(ast::Ptr<ast::Statement> delayed)
{
    auto lambda = make_unique<ast::Lambda>();
    lambda->statements_.push_back(std::move(delayed));
    return lambda;
}


// TODO: "delay" can be replaced with a macro in ebl code, once I add macros to
// the parser.
ast::Ptr<ast::Lambda> parseDelay(Lexer& lexer)
{
    auto lambda = makeDelay(parseStatement(lexer));
    expect<Lexer::Token::RPAREN>(lexer, "in parse delay");
    return lambda;
}


// TODO: "stream-cons" can be replaced with a macro in ebl code, once I add
// macros to the parser.
ast::Ptr<ast::Application> parseStreamCons(Lexer& lexer)
{
    auto app = make_unique<ast::Application>();
    auto cons = make_unique<ast::LValue>();
    cons->name_ = "cons";
    app->toApply_ = std::move(cons);
    app->args_.push_back(parseStatement(lexer));
    app->args_.push_back(makeDelay(parseStatement(lexer)));
    expect<Lexer::Token::RPAREN>(lexer, "in parse stream-cons");
    return app;
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
        } else if (symb == "def-mut") {
            return parseDefMut(lexer);
        } else if (symb == "defn") {
            return parseDefn(lexer);
        } else if (symb == "lambda") {
            return parseLambda(lexer);
        } else if (symb == "let") {
            return parseLet(lexer);
        } else if (symb == "let-mut") {
            return parseLetMut(lexer);
        } else if (symb == "if") {
            return parseIf(lexer);
        } else if (symb == "cond") {
            return parseCond(lexer);
        } else if (symb == "begin") {
            return parseBegin(lexer);
        } else if (symb == "namespace") {
            return parseNamespace(lexer);
        } else if (symb == "or") {
            return parseOr(lexer);
        } else if (symb == "and") {
            return parseAnd(lexer);
        } else if (symb == "set") {
            return parseSet(lexer);
        } else if (symb == "recur") {
            return parseRecur(lexer);
        } else if (symb == "delay") {
            return parseDelay(lexer);
        } else if (symb == "stream-cons") {
            return parseStreamCons(lexer);
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
            auto param = make_unique<ast::Float>();
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

        case Lexer::Token::QUOTE:
            apply->args_.push_back(parseQuoted(lexer));
            break;


        case Lexer::Token::CHAR: {
            auto param = make_unique<ast::Character>();
            if (utf8Len(lexer.rdbuf().c_str(), lexer.rdbuf().length()) not_eq
                1) {
                throw std::runtime_error("char literal " + lexer.rdbuf() +
                                         " contains multiple glyphs!");
            }
            for (size_t i = 0; i < lexer.rdbuf().length(); ++i) {
                param->value_[i] = lexer.rdbuf()[i];
            }
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

} // namespace ebl
