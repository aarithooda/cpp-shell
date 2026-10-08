#pragma once // this tells the compiler only complie this one, does not matter how many time gets called
#include <string>
#include <vector>

enum class TokenKind {
    word,
};

struct Token {
    TokenKind kind {};
    std::string text;
};

std::vector<Token> tokenize(const std::string& input);