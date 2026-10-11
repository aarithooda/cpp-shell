#include "tokenizer.h"

std::vector<Token> tokenize(const std::string& input) {
    if (input.empty()) return {};

    std::vector<Token> tokenisedInput;
    std::string temp;
    bool in_quotes = false;

    for (char c : input) {
        if (c == '"' && !in_quotes) {
            in_quotes = true;
            continue;
        }
        if (c == '"' && in_quotes) {
            in_quotes = false;
            continue;
        }

        if ((c == ' ' || c == '\t') && !temp.empty() && !in_quotes) {
            tokenisedInput.push_back({TokenKind::word, temp});
            temp.clear();
            continue;
        }

        if ((c != ' ' && c != '\t') || (c != '"' && in_quotes)) temp.push_back(c);
    }

    if (!temp.empty()) tokenisedInput.push_back({TokenKind::word, temp});

    return tokenisedInput;
}