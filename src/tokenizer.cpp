#include "tokenizer.h"
#include <iostream>

std::vector<Token> tokenize(const std::string& input, int& last_status) {
    if (input.empty()) return {};

    std::vector<Token> tokenisedInput;
    std::string temp;
    bool in_quotes = false;
    bool token_started = false;

    for (char c : input) {
        if (c == '"' && !in_quotes) {
            in_quotes = true;
            token_started = true;
            continue;
        }
        if (c == '"' && in_quotes) {
            in_quotes = false;
            continue;
        }

        if ((c == ' ' || c == '\t') && !in_quotes) {
            if (token_started) {
                tokenisedInput.push_back({TokenKind::word, temp});
                temp.clear();
                token_started = false;
            }
            continue;
        }

        temp.push_back(c);
        token_started = true;
    }

    if (in_quotes) {
        std::cout << "Syntax error: unmatched quotation mark." << std::endl;
        last_status = 1;
        return {};
    }

    if (token_started) tokenisedInput.push_back({TokenKind::word, temp});

    return tokenisedInput;
}