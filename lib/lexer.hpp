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
        FLOAT,
        STRING,
        CHAR,
    };

    Lexer(const std::string& input) : position_(0), input_(input)
    {
    }

    Token lex();

    void jumpPosition(size_t offset)
    {
        position_ += offset;
    }

    const std::string& rdbuf()
    {
        return inputBuffer_;
    }

    std::string remaining() const
    {
        return input_.substr(position_);
    }

    bool hasText() const
    {
        return position_ < input_.size() - 1;
    }

    bool isOpenDelimiter(char c) const
    {
        return c == '[' or c == '(';
    }

    bool isCloseDelimiter(char c) const
    {
        return c == ']' or c == ')';
    }

private:
    bool checkWhitespace(char c) const
    {
        return c == ' ' or c == '\n' or c == '\r';
    }

    char current() const
    {
        return input_[position_];
    }

    bool checkTermCond() const
    {
        return position_ < input_.size() and not checkWhitespace(current()) and
               not isOpenDelimiter(current()) and
               not isCloseDelimiter(current());
    }

    size_t position_;
    const std::string& input_;
    std::string inputBuffer_;
};

} // namespace lisp
