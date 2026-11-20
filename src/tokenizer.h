#pragma once
#include <string>
#include <vector>

enum class TokenKind {
    word, // ordinary text
    redirect_in, // <
    redirect_out, // >
    redirect_append, // >>
    command_separator, // ;
    conditional_and, // &&
    conditional_or, // ||
    pipe // |
};

struct Token {
    TokenKind kind {};
    std::string text;
};

std::vector<Token> tokenize(const std::string& input, int& last_status);
