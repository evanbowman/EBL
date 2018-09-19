#pragma once

#include <string>

namespace lisp {

class Lexer {
public:
    enum class Token {
        NONE,
        LPAREN,
        RPAREN,
        SYMBOL,
        QUOTE,
        INTEGER,
        FLOAT
    };

    Lexer(const std::string& input) :
        position_(0),
        input_(input) {}

    Token lex();

    const std::string& rdbuf() { return inputBuffer_; }

private:
    bool checkWhitespace(char c) const {
        return c == ' ' or c == '\n' or c == '\r';
    }

    char current() const { return input_[position_]; }

    bool checkTermCond() const {
        return position_ < input_.size()
               and not checkWhitespace(current())
               and current() not_eq ')'
               and current() not_eq '(';
    }

    size_t position_;
    const std::string& input_;
    std::string inputBuffer_;
};

}
